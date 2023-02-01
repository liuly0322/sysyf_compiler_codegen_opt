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

std::map<Value *, std::vector<Value *>> value_stack;
std::set<BasicBlock *> visited;

std::vector<Value *> Mem2Reg::collectValueDefine(BasicBlock *bb) {
    auto delete_list = std::vector<Instruction *>{};
    auto define_var = std::vector<Value *>{};
    for (auto *inst : bb->get_instructions()) {
        if (inst->is_phi()) {
            auto *lvalue = dynamic_cast<PhiInst *>(inst)->get_lval();
            define_var.push_back(lvalue);
            value_stack[lvalue].push_back(inst);
        }
        if (!isVarOp(inst))
            continue;
        delete_list.push_back(inst);
        if (inst->is_load()) {
            auto *lvalue = static_cast<LoadInst *>(inst)->get_lval();
            auto *new_value = value_stack[lvalue].back();
            inst->replace_all_use_with(new_value);
        } else if (inst->is_store()) {
            auto *lvalue = dynamic_cast<StoreInst *>(inst)->get_lval();
            auto *rvalue = static_cast<StoreInst *>(inst)->get_rval();
            define_var.push_back(lvalue);
            value_stack[lvalue].push_back(rvalue);
        }
    }
    for (auto *inst : delete_list)
        bb->delete_instr(inst);
    return define_var;
}

void Mem2Reg::valueForwarding(BasicBlock *bb) {
    visited.insert(bb);

    auto define_var = collectValueDefine(bb);

    for (auto *succbb : bb->get_succ_basic_blocks()) {
        for (auto *inst : succbb->get_instructions()) {
            if (!inst->is_phi())
                break;
            auto *phi = dynamic_cast<PhiInst *>(inst);
            auto *lvalue = phi->get_lval();
            if (!value_stack[lvalue].empty()) {
                auto *new_value = value_stack[lvalue].back();
                phi->add_phi_pair_operand(new_value, bb);
            }
        }
    }

    for (auto *succbb : bb->get_succ_basic_blocks())
        if (visited.count(succbb) == 0)
            valueForwarding(succbb);

    for (auto *var : define_var)
        value_stack[var].pop_back();
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
