#ifndef SYSYF_PUREFUNCTION_H
#define SYSYF_PUREFUNCTION_H

#include "Module.h"
#include <map>
#include <set>

namespace PureFunction {

extern std::map<Function *, bool> is_pure;
extern std::map<Function *, std::set<Value *>> global_var_store_effects;

/**
 * @brief 试图还原 store 到 左值 (alloca 指令)
 *
 * 用于判断是否具有副作用
 * @param inst store 指令
 */
AllocaInst *store_to_alloca(Instruction *inst);

/**
 * @brief 不考虑函数内部调用其他函数，判断纯函数
 *
 * 用于第一遍生成 worklist
 * @param f
 * @return true 不考虑函数调用，f 是纯函数
 * @return false 不考虑函数调用，f 也不是纯函数
 */
bool markPureInside(Function *f);

/**
 * @brief 判断函数是否是纯函数，维护 is_pure 表
 *
 * 非纯函数：有对全局变量或传入数组的 store，或者调用了非纯函数
 * 这里不考虑 MMIO 等 load 导致的非纯函数情形
 */
void markPure(Module *module);

} // namespace PureFunction

#endif // SYSYF_PUREFUNCTION_H