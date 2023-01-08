#include "CSE.h"
#include "PureFunction.h"

using PureFunction::is_pure;

void CSE::execute() {
    PureFunction::markPure(module);
    // 特殊指令：load, store, phi
    // 如果操作数都是常量，考虑提前计算出结果
    // ......
    // 通过可用表达式分析实现 CSE？
}