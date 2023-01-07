#ifndef SYSYF_PUREFUNCTION_H
#define SYSYF_PUREFUNCTION_H

#include "PureFunction.h"
#include "BasicBlock.h"
#include <unordered_map>

namespace PureFunction {

std::unordered_map<Function *, bool> is_pure;

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
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            // store 指令，且无法找到 alloca 作为左值，说明非局部变量
            if (inst->is_store() && store_to_alloca(inst) == nullptr) {
                return false;
            }
        }
    }
    return true;
}

void markPure(Module *module) {
    is_pure.clear();
    auto functions = module->get_functions();
    std::vector<Function *> work_list;
    // 先考虑函数本身有无副作用，并将 worklists 初始化为非纯函数
    for (auto *f : functions) {
        PureFunction::is_pure[f] = markPureInside(f);
        if (!PureFunction::is_pure[f]) {
            work_list.push_back(f);
        }
    }
    // 考虑非纯函数调用的「传染」
    for (auto i = 0; i < work_list.size(); i++) {
        auto *callee_function = work_list[i];
        for (auto &use : callee_function->get_use_list()) {
            auto *call_inst = dynamic_cast<CallInst *>(use.val_);
            auto *caller_function = call_inst->get_function();
            if (PureFunction::is_pure[caller_function]) {
                PureFunction::is_pure[caller_function] = false;
                work_list.push_back(caller_function);
            }
        }
    }
}

} // namespace PureFunction

#endif // SYSYF_PUREFUNCTION_H