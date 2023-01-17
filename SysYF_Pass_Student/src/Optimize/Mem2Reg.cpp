#include "Mem2Reg.h"
#include "IRBuilder.h"

void Mem2Reg::execute() {
    for (auto *fun : module->get_functions()) {
        if (fun->is_declaration())
            continue;
        func_ = fun;
        lvalue_connection.clear();
        no_union_set.clear();

        insideBlockForwarding();
        genPhi();
        // module->set_print_name(); // 活跃变量分析对拍用
        valueDefineCounting();
        valueForwarding(func_->get_entry_block());
        removeAlloc();
    }
}

void Mem2Reg::insideBlockForwarding() {
    for (auto *bb : func_->get_basic_blocks()) {
        std::map<Value *, StoreInst *> defined_list;

        const auto insts = std::vector<Instruction *>{
            bb->get_instructions().begin(), bb->get_instructions().end()};
        for (auto *inst : insts) {
            if (!isVarOp(inst, true))
                continue;

            if (auto *store_inst = dynamic_cast<StoreInst *>(inst)) {
                Value *lvalue = store_inst->get_lval();
                if (auto *forwarded = defined_list[lvalue])
                    bb->delete_instr(forwarded);
                defined_list[lvalue] = store_inst;
            } else if (auto *load_inst = dynamic_cast<LoadInst *>(inst)) {
                Value *lvalue = load_inst->get_lval();
                if (auto *forwarded = defined_list[lvalue]) {
                    Value *value = forwarded->get_rval();
                    load_inst->replace_all_use_with(value);
                    bb->delete_instr(load_inst);
                }
            }
        }
    }
}

void Mem2Reg::genPhi() {
    std::set<Value *> globals;
    std::map<Value *, std::set<BasicBlock *>> defined_in_block;

    for (auto *bb : func_->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (!isVarOp(inst))
                continue;

            if (inst->get_instr_type() == Instruction::OpID::load) {
                Value *lvalue = static_cast<LoadInst *>(inst)->get_lval();
                globals.insert(lvalue);
            } else if (inst->get_instr_type() == Instruction::OpID::store) {
                Value *lvalue = static_cast<StoreInst *>(inst)->get_lval();
                auto defined_block = defined_in_block.find(lvalue);
                if (defined_block != defined_in_block.end()) {
                    defined_block->second.insert(bb);
                } else {
                    defined_in_block.insert({lvalue, {bb}});
                }
            }
        }
    }

    std::map<BasicBlock *, std::set<Value *>> bb_phi_list;

    for (auto *var : globals) {
        auto define_bbs = defined_in_block.find(var)->second;
        std::vector<BasicBlock *> queue;
        queue.assign(define_bbs.begin(), define_bbs.end());
        auto iter_pointer = 0U;
        for (; iter_pointer < queue.size(); iter_pointer++) {
            for (auto *bb_domfront : queue[iter_pointer]->get_dom_frontier()) {
                // 对每个基本块的支配边界，在其中插入当前变量的φ函数
                auto phis = bb_phi_list.find(bb_domfront);
                if (phis != bb_phi_list.end()) {
                    if (phis->second.find(var) == phis->second.end()) {
                        phis->second.insert(var);
                        auto *newphi = PhiInst::create_phi(
                            var->get_type()->get_pointer_element_type(),
                            bb_domfront);
                        newphi->set_lval(var);
                        bb_domfront->add_instr_begin(newphi);
                        queue.push_back(bb_domfront);
                    }
                } else {
                    auto *newphi = PhiInst::create_phi(
                        var->get_type()->get_pointer_element_type(),
                        bb_domfront);
                    newphi->set_lval(var);
                    bb_domfront->add_instr_begin(newphi);
                    queue.push_back(bb_domfront);
                    bb_phi_list.insert({bb_domfront, {var}});
                }
            }
        }
    }
}

// 计算变量定值相关的信息
void Mem2Reg::valueDefineCounting() {
    // 清空
    define_var = std::map<BasicBlock *, std::vector<Value *>>();
    // 对每个基本块计算定值
    for (auto *bb : func_->get_basic_blocks()) {
        define_var.insert({bb, {}});
        for (auto *inst : bb->get_instructions()) {
            // φ指令，是一种对内存的定值
            if (inst->get_instr_type() == Instruction::OpID::phi) {
                auto *lvalue = dynamic_cast<PhiInst *>(inst)->get_lval();
                define_var.find(bb)->second.push_back(lvalue);
            }
            // store指令，也是一种对内存的定值
            else if (inst->get_instr_type() == Instruction::OpID::store) {
                // 非局部名字不考虑
                if (!isVarOp(inst))
                    continue;
                auto *lvalue = dynamic_cast<StoreInst *>(inst)->get_lval();
                define_var.find(bb)->second.push_back(lvalue);
            }
        }
    }
}

std::map<Value *, std::vector<Value *>> value_status;
std::set<BasicBlock *> visited;

// 对于传入的一个基本块做操作
void Mem2Reg::valueForwarding(BasicBlock *bb) {
    std::set<Instruction *> delete_list;
    // 将本基本块标记为已访问（感觉可以用vector<bool>优化）
    visited.insert(bb);
    // 处理φ指令
    for (auto *inst : bb->get_instructions()) {
        if (inst->get_instr_type() != Instruction::OpID::phi)
            break;
        auto *lvalue = dynamic_cast<PhiInst *>(inst)->get_lval();
        auto value_list = value_status.find(lvalue);
        if (value_list != value_status.end()) {
            value_list->second.push_back(inst);
        } else {
            value_status.insert({lvalue, {inst}});
        }
    }

    // 基本块中不涉及φ、非局部名字的指令
    for (auto *inst : bb->get_instructions()) {
        if (inst->get_instr_type() == Instruction::OpID::phi)
            continue;
        if (!isVarOp(inst))
            continue;
        // 传播，用记录的值替换不必要的load和store
        if (inst->get_instr_type() == Instruction::OpID::load) {
            Value *lvalue = static_cast<LoadInst *>(inst)->get_lval();
            Value *new_value = *(value_status.find(lvalue)->second.rbegin());
            inst->replace_all_use_with(new_value);
        } else if (inst->get_instr_type() == Instruction::OpID::store) {
            Value *lvalue = static_cast<StoreInst *>(inst)->get_lval();
            Value *rvalue = static_cast<StoreInst *>(inst)->get_rval();
            if (value_status.find(lvalue) != value_status.end()) {
                value_status.find(lvalue)->second.push_back(rvalue);
            } else {
                value_status.insert({lvalue, {rvalue}});
            }
        }
        delete_list.insert(inst);
    }

    // 给bb的后继基本块确定φ值
    // bb的后继基本块
    for (auto *succbb : bb->get_succ_basic_blocks()) {
        for (auto *inst : succbb->get_instructions()) {
            // 如果有φ指令
            if (inst->get_instr_type() == Instruction::OpID::phi) {
                auto *phi = dynamic_cast<PhiInst *>(inst);
                auto *lvalue = phi->get_lval();
                // 取到φ的左值，然后把定值表中最后一个定值给他，注意参数bb是pre_bb
                if (value_status.find(lvalue) != value_status.end()) {
                    if (!value_status.find(lvalue)->second.empty()) {
                        Value *new_value =
                            *(value_status.find(lvalue)->second.rbegin());
                        phi->add_phi_pair_operand(new_value, bb);
                    }
                }
            }
        }
    }

    // 如果没访问，就递归访问后续bb
    for (auto *succbb : bb->get_succ_basic_blocks()) {
        if (visited.find(succbb) != visited.end())
            continue;
        valueForwarding(succbb);
    }

    // 删除定值
    auto var_set = define_var.find(bb)->second;
    for (auto *var : var_set) {
        if (value_status.find(var) == value_status.end())
            continue;
        if (value_status.find(var)->second.empty())
            continue;
        value_status.find(var)->second.pop_back();
    }

    for (auto *inst : delete_list) {
        bb->delete_instr(inst);
    }
}

// 删除Alloca语句
void Mem2Reg::removeAlloc() {
    for (auto *bb : func_->get_basic_blocks()) {
        std::set<Instruction *> delete_list;
        for (auto *inst : bb->get_instructions()) {
            if (inst->get_instr_type() != Instruction::OpID::alloca)
                continue;
            auto *alloc_inst = dynamic_cast<AllocaInst *>(inst);
            if (alloc_inst->get_alloca_type()->is_integer_type() ||
                alloc_inst->get_alloca_type()->is_float_type() ||
                alloc_inst->get_alloca_type()->is_pointer_type()) {
                delete_list.insert(inst);
            }
        }
        for (auto *inst : delete_list) {
            bb->delete_instr(inst);
        }
    }
}

void Mem2Reg::phiStatistic() {}
