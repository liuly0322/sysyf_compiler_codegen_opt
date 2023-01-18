#ifndef SYSYF_PUREFUNCTION_H
#define SYSYF_PUREFUNCTION_H

#include "PureFunction.h"
#include "Module.h"
#include <unordered_map>

namespace PureFunction {

std::unordered_map<Function *, bool> is_pure;
std::unordered_map<Function *, std::set<Value *>> global_var_store_effects;

AllocaInst *store_to_alloca(Instruction *inst) {
    // lval 无法转换为 alloca 指令说明是非局部的，有副作用
    Value *lval_runner = inst->get_operand(1);
    while (auto *gep_inst = dynamic_cast<GetElementPtrInst *>(lval_runner)) {
        lval_runner = gep_inst->get_operand(0);
    }
    return dynamic_cast<AllocaInst *>(lval_runner);
}

bool markPureInside(Function *f) {
    // 只有函数声明，无法判断
    if (f->is_declaration()) {
        return false;
    }
    auto pure = true;
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            // store 指令，且无法找到 alloca 作为左值，说明非局部变量
            if (inst->is_store() && store_to_alloca(inst) == nullptr) {
                if (auto *var =
                        dynamic_cast<GlobalVariable *>(inst->get_operand(1)))
                    global_var_store_effects[f].insert(var);
                pure = false;
            }
        }
    }
    return pure;
}

void markPure(Module *module) {
    is_pure.clear();
    auto functions = module->get_functions();
    std::vector<Function *> work_list;

    for (auto *f : functions) {
        is_pure[f] = markPureInside(f);
        if (!is_pure[f] || !global_var_store_effects[f].empty()) {
            work_list.push_back(f);
        }
    }

    for (auto i = 0U; i < work_list.size(); i++) {
        auto *callee_function = work_list[i];
        for (auto &use : callee_function->get_use_list()) {
            auto *call_inst = dynamic_cast<CallInst *>(use.val_);
            auto *caller_function = call_inst->get_function();
            bool influenced{};
            for (auto *var : global_var_store_effects[callee_function]) {
                if (global_var_store_effects[caller_function].count(var) == 0) {
                    global_var_store_effects[caller_function].insert(var);
                    influenced = true;
                }
            }
            if (is_pure[caller_function]) {
                is_pure[caller_function] = false;
                influenced = true;
            }
            if (influenced)
                work_list.push_back(caller_function);
        }
    }
}

} // namespace PureFunction

#endif // SYSYF_PUREFUNCTION_H