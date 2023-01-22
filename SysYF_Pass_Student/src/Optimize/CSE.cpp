#include "CSE.h"
#include <algorithm>
#include <iostream>
#include <ostream>
#include <vector>

using PureFunction::global_var_store_effects;
using PureFunction::is_pure;
using PureFunction::markPure;

void CSE::forward_store() {
    auto store_insts = std::vector<std::list<Instruction *>::iterator>{};
    for (auto *fun : module->get_functions()) {
        for (auto *bb : fun->get_basic_blocks()) {
            auto &insts = bb->get_instructions();
            for (auto iter = insts.begin(); iter != insts.end(); iter++) {
                if ((*iter)->is_store())
                    store_insts.push_back(iter);
            }
        }
    }
    for (auto iter : store_insts) {
        auto *store_inst = static_cast<StoreInst *>(*iter);
        auto *bb = store_inst->get_parent();
        auto *lval = store_inst->get_lval();
        auto *rval = store_inst->get_rval();
        auto *type = rval->get_type();
        auto *load_inst = LoadInst::create_load(type, lval, bb);
        bb->get_instructions().pop_back();
        bb->add_instruction(++iter, load_inst);
        forwarded_load.emplace_back(load_inst, rval);
    }
}

void CSE::delete_forward() {
    for (auto &[load_inst, value] : forwarded_load) {
        load_inst->replace_all_use_with(value);
        load_inst->get_parent()->delete_instr(load_inst);
    }
}

void CSE::execute() {
    markPure(module);
    forward_store();
    for (auto *fun : module->get_functions()) {
        if (fun->get_basic_blocks().empty()) {
            continue;
        }
        do {
            localCSE(fun);
            globalCSE(fun);
        } while (!delete_list.empty());
    }
    delete_forward();
}

Value *CSE::findOrigin(Value *val) {
    Value *lval_runner = val;
    while (auto *gep_inst = dynamic_cast<GetElementPtrInst *>(lval_runner)) {
        lval_runner = gep_inst->get_operand(0);
    }
    // GlobalVariable or Argument or AllocaInst
    return lval_runner;
}

bool CSE::cmp(Instruction *inst1, Instruction *inst2) {
    // assert inst1 is load
    if (!inst1->is_load())
        return false;
    auto *lval_load = inst1->get_operand(0);
    auto *target_load = findOrigin(lval_load);
    auto isArgOrGlobalArrayOp = [](Value *lval, Value *target) {
        return dynamic_cast<Argument *>(target) != nullptr ||
               (dynamic_cast<GlobalVariable *>(target) != nullptr &&
                dynamic_cast<GlobalVariable *>(lval) == nullptr);
    };
    // 1. argument or global array
    if (isArgOrGlobalArrayOp(lval_load, target_load)) {
        if (inst2->is_store()) {
            auto *lval_store = inst2->get_operand(1);
            auto *target_store = findOrigin(lval_store);
            // if both global
            if (dynamic_cast<GlobalVariable *>(target_load) != nullptr &&
                dynamic_cast<GlobalVariable *>(target_store) != nullptr)
                return target_load == target_store;
            // else any global/arg store erase load inst
            return isArgOrGlobalArrayOp(lval_store, target_store);
        }
        if (inst2->is_call()) {
            auto *callee = dynamic_cast<Function *>(inst2->get_operand(0));
            return !is_pure[callee];
        }
        return false;
    }
    // 2. local array or global variable
    if (inst2->is_store()) {
        return target_load == findOrigin(inst2->get_operand(1));
    }
    if (inst2->is_call()) {
        auto *callee = static_cast<Function *>(inst2->get_operand(0));
        if (global_var_store_effects[callee].count(lval_load) != 0)
            return true;
        for (auto *opr : inst2->get_operands())
            if (findOrigin(opr) == target_load)
                return true;
    }
    return false;
}

Instruction *CSE::isAppear(Instruction *inst, std::vector<Instruction *> &insts,
                           int index) {
    for (auto i = index - 1; i >= 0; --i) {
        auto *instr = insts[i];
        if (cmp(inst, instr)) {
            return nullptr;
        }
        if (Expression(inst) == Expression(instr)) {
            return instr;
        }
    }
    return nullptr;
}

void CSE::localCSE(Function *fun) {
    for (auto *bb : fun->get_basic_blocks()) {
        do {
            delete_list.clear();
            std::vector<Instruction *> insts;
            for (auto *inst : bb->get_instructions()) {
                insts.push_back(inst);
            }
            for (auto i = 0U; i < insts.size(); ++i) {
                auto *inst = insts[i];
                if (!isOptmized(inst)) {
                    continue;
                }
                auto *preInst = isAppear(inst, insts, i);
                if (preInst != nullptr) {
                    delete_list.push_back(inst);
                    inst->replace_all_use_with(preInst);
                    break;
                }
            }
            delete_instr();
        } while (!delete_list.empty());
    }
}

void CSE::globalCSE(Function *fun) {
    delete_list.clear();
    calcGenKill(fun);
    calcInOut(fun);
    findSource(fun);
    replaceSubExpr(fun);
}

bool CSE::isKill(Instruction *inst, std::vector<Instruction *> &insts,
                 unsigned index) {
    for (auto i = index + 1; i < insts.size(); ++i) {
        auto instr = insts[i];
        if (cmp(inst, instr)) {
            return true;
        }
    }
    return false;
}

void CSE::calcGenKill(Function *fun) {
    // Get available expressions
    available.clear();
    for (auto *bb : fun->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (!isOptmized(inst)) {
                continue;
            }
            const Expression expr{inst};
            if (std::find(available.begin(), available.end(), expr) !=
                available.end()) {
                continue;
            }
            available.push_back(expr);
        }
    }

    // Get Gen
    GEN.clear();
    for (auto *bb : fun->get_basic_blocks()) {
        std::vector<bool> gen(available.size(), false);
        std::vector<Instruction *> insts;
        for (auto *inst : bb->get_instructions()) {
            insts.push_back(inst);
        }
        for (auto i = 0U; i < insts.size(); ++i) {
            auto *inst = insts[i];
            if (!isOptmized(inst)) {
                continue;
            }
            if (inst->is_load()) {
                if (isKill(inst, insts, i)) {
                    continue;
                }
            }
            auto index = std::find(available.begin(), available.end(),
                                   Expression(inst)) -
                         available.begin();
            gen[index] = true;
        }
        GEN.insert({bb, gen});
    }

    // Get Kill
    KILL.clear();
    for (auto *bb : fun->get_basic_blocks()) {
        std::vector<bool> kill(available.size(), false);
        for (auto *inst : bb->get_instructions()) {
            if (inst->is_ret() || inst->is_br()) {
                continue;
            }
            Value *target = inst;
            for (auto i = 0U; i < available.size(); ++i) {
                if (available[i].inst->is_load()) {
                    if (cmp(available[i].inst, inst)) {
                        kill[i] = true;
                    }
                }
                auto &operands = available[i].operands;
                if (std::find(operands.begin(), operands.end(), target) !=
                    operands.end()) {
                    kill[i] = true;
                }
            }
        }
        KILL.insert({bb, kill});
    }
}

void CSE::calcInOut(Function *fun) {
    // initialize
    const std::vector<bool> phi(available.size(), false);
    const std::vector<bool> U(available.size(), true);
    IN.clear();
    OUT.clear();
    IN.insert({fun->get_entry_block(), phi});
    for (auto *bb : fun->get_basic_blocks()) {
        OUT.insert({bb, U});
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto *bb : fun->get_basic_blocks()) {
            // calculate IN
            if (bb != fun->get_entry_block()) {
                std::vector<bool> in(U);
                for (auto *pre : bb->get_pre_basic_blocks()) {
                    for (auto i = 0U; i < available.size(); i++) {
                        in[i] = in[i] && OUT[pre][i];
                    }
                }
                IN.insert_or_assign(bb, in);
            }

            // calculate OUT
            const std::vector<bool> preOut = OUT[bb];
            for (auto i = 0U; i < available.size(); ++i) {
                OUT[bb][i] = GEN[bb][i] || (IN[bb][i] && !KILL[bb][i]);
            }
            if (preOut != OUT[bb]) {
                changed = true;
            }
        }
    }
}

void CSE::findSource(Function *fun) {
    for (auto *bb : fun->get_basic_blocks()) {
        std::vector<Instruction *> insts;
        for (auto *inst : bb->get_instructions()) {
            insts.push_back(inst);
        }
        for (auto i = 0U; i < insts.size(); ++i) {
            auto *inst = insts[i];
            if (!isOptmized(inst)) {
                continue;
            }
            auto it =
                std::find(available.begin(), available.end(), Expression(inst));
            auto index = it - available.begin();
            auto &expr = *it;
            if (inst->is_load() && isKill(inst, insts, i)) {
                expr.source.erase(inst);
            } else if ((!IN[bb][index] || KILL[bb][index]) && GEN[bb][index]) {
                expr.source.insert(inst);
            }
        }
    }
}

BasicBlock *traverse(BasicBlock *curbb, BasicBlock *dstbb, BasicBlock *prebb,
                     std::set<BasicBlock *> &visited, bool flag) {
    if (visited.count(curbb)) {
        return nullptr;
    }
    if (flag) {
        visited.insert(curbb);
    }
    if (flag && curbb == dstbb) {
        return prebb;
    }
    prebb = curbb;
    for (auto *succ_bb : curbb->get_succ_basic_blocks()) {
        auto *ret = traverse(succ_bb, dstbb, prebb, visited, true);
        if (ret != nullptr) {
            return ret;
        }
    }
    return nullptr;
}

BasicBlock *CSE::isReach(BasicBlock *bb, Instruction *inst) {
    auto *curbb = inst->get_parent();
    std::set<BasicBlock *> visited;
    return traverse(curbb, bb, curbb, visited, false);
}

void CSE::replaceSubExpr(Function *fun) {
    for (auto *bb : fun->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (!isOptmized(inst)) {
                continue;
            }
            auto it =
                std::find(available.begin(), available.end(), Expression(inst));
            auto &expr = *it;
            auto index = it - available.begin();
            if (!IN[bb][index] || KILL[bb][index] || expr.source.count(inst)) {
                continue;
            }
            if (expr.source.size() == 1) {
                auto *repInst = *expr.source.begin();
                delete_list.push_back(inst);
                inst->replace_all_use_with(repInst);
            } else {
                std::vector<std::pair<Instruction *, BasicBlock *>> vec;
                for (auto *instr : expr.source) {
                    auto *srcbb = isReach(bb, instr);
                    if (srcbb != nullptr) {
                        vec.emplace_back(instr, srcbb);
                    }
                }
                if (vec.size() == 1) {
                    delete_list.push_back(inst);
                    auto *repInst = (*vec.begin()).first;
                    inst->replace_all_use_with(repInst);
                    continue;
                }
            }
        }
    }
    delete_instr();
}

void CSE::delete_instr() {
    for (auto *inst : delete_list) {
        inst->get_parent()->delete_instr(inst);
    }
}