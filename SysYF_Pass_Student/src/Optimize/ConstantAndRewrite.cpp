#include "ConstantAndRewrite.h"
#include "PureFunction.h"

using PureFunction::is_pure;

void ConstantAndRewrite::execute() {
    // 先序遍历支配树，load, store, phi 跳过
    // 如果操作数都是常量，考虑提前计算出结果
    // 否则保存复写关系
    // 注意：纯函数的相同调用才能复写
}