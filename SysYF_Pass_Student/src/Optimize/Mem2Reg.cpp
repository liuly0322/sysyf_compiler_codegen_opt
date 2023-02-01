#include "Mem2Reg.h"

using PureFunction::global_var_store_effects;
using PureFunction::markPure;

void Mem2Reg::execute() {
    markPure(module);
    for (auto *fun : module->get_functions()) {
        if (fun->is_declaration())
            continue;
        func_ = fun;

        insideBlockForwarding();
        genPhi();
        // module->set_print_name(); // 活跃变量分析对拍用
        valueForwarding(func_->get_entry_block());
        removeAlloc();
    }
}

void Mem2Reg::insideBlockForwarding() {
    for (auto *bb : func_->get_basic_blocks()) {
        auto defined_list = std::map<Value *, StoreInst *>{};

        const auto insts = std::vector<Instruction *>{
            bb->get_instructions().begin(), bb->get_instructions().end()};
        for (auto *inst : insts) {
            // 非纯函数调用将注销所有受影响全局变量的定值
            if (inst->is_call()) {
                auto *callee = static_cast<Function *>(inst->get_operand(0));
                for (auto *var : global_var_store_effects[callee])
                    defined_list.erase(var);
            }

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

            if (inst->is_load()) {
                Value *lvalue = static_cast<LoadInst *>(inst)->get_lval();
                globals.insert(lvalue);
            } else if (inst->is_store()) {
                Value *lvalue = static_cast<StoreInst *>(inst)->get_lval();
                defined_in_block[lvalue].insert(bb);
            }
        }
    }

    std::map<BasicBlock *, std::set<Value *>> bb_phi_list;

    for (auto *var : globals) {
        auto define_bbs = defined_in_block[var];
        std::vector<BasicBlock *> queue{define_bbs.begin(), define_bbs.end()};
        auto iter_pointer = 0U;
        for (; iter_pointer < queue.size(); iter_pointer++) {
            for (auto *bb_domfront : queue[iter_pointer]->get_dom_frontier()) {
                // 对每个基本块的支配边界，在其中插入当前变量的 φ 函数
                auto &phi_insts = bb_phi_list[bb_domfront];
                if (phi_insts.find(var) != phi_insts.end())
                    continue;
                phi_insts.insert(var);
                auto *newphi = PhiInst::create_phi(
                    var->get_type()->get_pointer_element_type(), bb_domfront);
                newphi->set_lval(var);
                bb_domfront->add_instr_begin(newphi);
                queue.push_back(bb_domfront);
            }
        }
    }
}

std::map<Value *, std::vector<Value *>> value_status;
std::set<BasicBlock *> visited;

void Mem2Reg::valueForwarding(BasicBlock *bb) {
    visited.insert(bb);
    std::set<Instruction *> delete_list;

    // φ 指令给地址定值
    for (auto *inst : bb->get_instructions()) {
        if (!inst->is_phi())
            break;
        auto *lvalue = dynamic_cast<PhiInst *>(inst)->get_lval();
        value_status[lvalue].push_back(inst);
    }

    for (auto *inst : bb->get_instructions()) {
        if (!isVarOp(inst))
            continue;
        // 传播，用地址定值替换不必要的 load 和 store
        if (inst->is_load()) {
            Value *lvalue = static_cast<LoadInst *>(inst)->get_lval();
            Value *new_value = value_status[lvalue].back();
            inst->replace_all_use_with(new_value);
        } else if (inst->is_store()) {
            Value *lvalue = static_cast<StoreInst *>(inst)->get_lval();
            Value *rvalue = static_cast<StoreInst *>(inst)->get_rval();
            value_status[lvalue].push_back(rvalue);
        }
        delete_list.insert(inst);
    }

    // 给 bb 的后继基本块的 φ 指令确定来源
    for (auto *succbb : bb->get_succ_basic_blocks()) {
        for (auto *inst : succbb->get_instructions()) {
            if (!inst->is_phi())
                break;
            auto *phi = dynamic_cast<PhiInst *>(inst);
            auto *lvalue = phi->get_lval();
            // φ 来源是地址在当前基本块的定值
            if (value_status.find(lvalue) != value_status.end()) {
                if (!value_status[lvalue].empty()) {
                    Value *new_value = value_status[lvalue].back();
                    phi->add_phi_pair_operand(new_value, bb);
                }
            }
        }
    }

    // 递归访问后续 bb
    for (auto *succbb : bb->get_succ_basic_blocks())
        if (visited.count(succbb) == 0)
            valueForwarding(succbb);

    // 删除定值
    auto var_set = collectValueDefine(bb);
    for (auto *var : var_set)
        if (!value_status[var].empty())
            value_status[var].pop_back();

    for (auto *inst : delete_list)
        bb->delete_instr(inst);
}

void Mem2Reg::removeAlloc() {
    auto *entry_bb = func_->get_entry_block();
    std::set<Instruction *> delete_list;
    for (auto *inst : entry_bb->get_instructions()) {
        if (!inst->is_alloca())
            continue;
        auto *alloc_inst = dynamic_cast<AllocaInst *>(inst);
        if (!alloc_inst->get_alloca_type()->is_array_type())
            delete_list.insert(inst);
    }
    for (auto *inst : delete_list)
        entry_bb->delete_instr(inst);
}