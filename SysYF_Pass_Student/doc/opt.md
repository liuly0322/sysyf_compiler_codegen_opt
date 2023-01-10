# 进阶优化提交报告

本组一共进行了三个优化：

- Sparse Condition Constant Propagation, 下称 SCCP
- Common Sub-expression Elimination，下称 CSE
- Dead Code Elimination，下称 DCE

均基于 SSA 形式

## Sparse Condition Constant Propagation

### 前言

SCCP 是一个常量传播 Pass。选择添加 SCCP 优化遍，原因如下：

- 能在编译时求值的表达式不需要在执行时才求值。
- 通过用常量值替换变量来修改 IR，可以识别然后消除程序的无效代码部分，例如始终为假的跳转，从而提高程序的整体效率。

Sparse 指示了这一 Pass 是基于 SSA 图这一稀疏表示，Condition 指示了这一算法将控制流边缘与常量传播结合的能力。

### 设计思想

为了引入相对复杂的 SCCP，我们先从 SCP(Sparse Constant Propagation) 开始介绍：

首先需要分析哪些指令可以被替换为常量，再在分析结束后统一替换。

为此，需要先标记每个指令的状态。SCP 借鉴了格这一代数结构，将指令分为三种状态：

- TOP: 无法确定
- Constant: 一个常量（但后续可能会被更新为 BOT），注意 Constant 是有具体数值的，例如确定指令值为 1
- BOT: 确定是一个变量

初始时，所有指令的状态都被初始化为 TOP，而后遍历所有指令：

- 对于可以常量折叠的指令（一元/二元运算或比较指令，且操作数均为常数），将指令状态标记为 Constant
- 如果某个指令的状态发生变化，则所有用到该指令的指令都推入 worklist，重新计算状态

这里值得注意的是，指令状态变化一定是 TOP 到 BOT 的方向，至多只能变化两次（保证算法不会死循环）。

对于这个格而言，需要以下两种操作：

- 交操作，满足：
  - TOP ^ X = X
  - C_i ^ C_j = C_i, if C_i = C_j
  - C_i ^ C_j = BOT, if C_i != C_j
  - BOT ^ X = BOT
- 比较操作，当且仅当完全相等

交操作主要用于计算 phi 指令的状态（所有可达操作数的交）。

而 SCCP 在 SCP 的基础上，注意到有些控制流实际是不可达的（条件语句是 Constant），因此初始并不直接推入所有指令，而是仅推入入口基本块的指令。此外，除了 SCP 也需要的 ssa_worklist，SCCP 还需要增加一个 cfg_worklist，这是一个存储 `{pre_bb, bb}` 边的数组，意味着 bb 从 pre_bb 可达，因此 bb 的指令也可以推入 ssa_work_list。

对条件的判断主要是通过 ssa_worklist 中 br 指令的定值来考虑的：

- 如果 br 是无条件跳转，则 `{bb, jmp_bb}` 推入 cfg_worklist
- 如果 br 的 cond 操作数是常量，则只要将能进入的 bb 推入 cfg_worklist
- 否则，需要将两个目标 bb 都推入 cfg_worklist

对于指令的定值，有一些特殊情况：

- 初始可以将 Argument\* 定值为 BOT
- 对于访存指令，一律定值为 BOT（load 的行为难以预测）

### 代码实现

虽然思路很简单，但代码实现还是比较庞杂的。

#### 指令到状态的映射

我们需要自定义一个 `ValueStatus` 结构，以记录指令状态

```cpp
struct ValueStatus {
    enum Status { BOT = 0, CONST, TOP };
    Status status;
    Constant *value;
    void operator^=(ValueStatus &b) {
        // ......
    }
    bool operator!=(ValueStatus &b) const {
        // ......
    }
};
```

通过枚举体指示指令的三种状态，其中 CONST 状态额外需要一个 Constant\* 来保存实际的常量。

两个运算符重载对应上文提到的格的两种操作。

这样一来，以下的 map 可以把指令映射到对应的状态。

```cpp
auto value_map = std::unordered_map<Value *, ValueStatus>{};
```

但需要注意的是，有些指令的操作数本身就是 Constant\*，因此需要设计正确一个从 value_map 中获取值的方法。

```cpp
ValueStatus get_mapped(std::unordered_map<Value *, ValueStatus> &value_map,
                       Value *key) {
    if (auto *constant = dynamic_cast<Constant *>(key))
        return {ValueStatus::CONST, constant};
    return value_map[key];
}
```

#### 常量折叠

我们先给出常量折叠函数一个合理的声明：

```cpp
Constant *const_fold(std::unordered_map<Value *, ValueStatus> &value_map,
                     Instruction *inst, Module *module) {
    // ......
}
```

传入 inst 和 value_map，如果指令能常量折叠，就返回折叠后的 Constant\*（module 参数用于常量操作，可以忽略）。

下面按照这一指导思想，补全函数即可。

```cpp
const std::unordered_set<std::string> binary_ops{"add",  "sub",  "mul",  "sdiv",
                                                 "srem", "fadd", "fsub", "fmul",
                                                 "fdiv", "cmp",  "fcmp"};

const std::unordered_set<std::string> unary_ops{"fptosi", "sitofp", "zext"};

Constant *const_fold(std::unordered_map<Value *, ValueStatus> &value_map,
                     Instruction *inst, Module *module) {
    if (binary_ops.count(inst->get_instr_op_name()) != 0) {
        auto *const_v1 = get_mapped(value_map, inst->get_operand(0)).value;
        auto *const_v2 = get_mapped(value_map, inst->get_operand(1)).value;
        if (const_v1 != nullptr && const_v2 != nullptr) {
            return const_fold(inst, const_v1, const_v2, module);
        }
    } else if (unary_ops.count(inst->get_instr_op_name()) != 0) {
        auto *const_v = get_mapped(value_map, inst->get_operand(0)).value;
        if (const_v != nullptr) {
            return const_fold(inst, const_v, module);
        }
    }
    return nullptr;
}
```

分类讨论，如果肯定指令是可常量折叠的，就交给 const_fold 的重载函数，具体计算折叠后的值。

#### 迭代过程

首先将 Argument\* 初始化为 BOT，所有指令初始化为 TOP，然后 cfg_work_list 初始化为仅包含 entry_bb。然后即可开始迭代。

```cpp
while (i < cfg_work_list.size() || j < ssa_work_list.size()) {
    while (i < cfg_work_list.size()) {
        auto [pre_bb, bb] = cfg_work_list[i++];

        if (marked.count({pre_bb, bb}) != 0)
            continue;
        marked.insert({pre_bb, bb});

        for (auto *inst : bb->get_instructions()) {
            if (inst->is_phi())
                visit_phi(inst);
            else
                visit_inst(inst);
        }
    }
    while (j < ssa_work_list.size()) {
        auto *inst = ssa_work_list[j++];

        // 如果指令已经是 bot 了，不需要再传播
        if (get_mapped(value_map, inst).status == ValueStatus::BOT)
            continue;

        if (inst->is_phi())
            visit_phi(inst);
        else
            visit_inst(inst);
    }
}
```

算法的原理上文已经讲解过，这里不再赘述。

#### VisitPhi

```cpp
auto visit_phi = [&](Instruction *inst) {
    auto prev_status = value_map[inst];
    auto cur_status = prev_status;
    auto *phi_inst = dynamic_cast<PhiInst *>(inst);
    auto *bb = phi_inst->get_parent();

    const int phi_size = phi_inst->get_num_operand() / 2;
    for (int i = 0; i < phi_size; i++) {
        auto *pre_bb =
            static_cast<BasicBlock *>(phi_inst->get_operand(2 * i + 1));
        if (marked.count({pre_bb, bb}) != 0) {
            auto *op = phi_inst->get_operand(2 * i);
            auto op_status = get_mapped(value_map, op);
            cur_status ^= op_status;
        }
    }

    if (cur_status != prev_status) {
        value_map[inst] = cur_status;
        for (auto use : inst->get_use_list()) {
            auto *use_inst = dynamic_cast<Instruction *>(use.val_);
            ssa_work_list.push_back(use_inst);
        }
    }
};
```

phi 指令的计算就是将所有可达操作数求交。如果指令状态变化，就推入所有需要重新计算的指令。

#### VisitInst

分为三类，跳转指令，可常量折叠的指令，和其他指令（访存，函数调用等）。

##### 跳转指令

```cpp
if (static_cast<BranchInst *>(inst)->is_cond_br()) {
    auto *const_cond = static_cast<ConstantInt *>(
        get_mapped(value_map, inst->get_operand(0)).value);
    auto *true_bb = static_cast<BasicBlock *>(inst->get_operand(1));
    auto *false_bb =
        static_cast<BasicBlock *>(inst->get_operand(2));
    if (const_cond != nullptr) {
        auto cond = const_cond->get_value();
        if (cond != 0) {
            cfg_work_list.emplace_back(bb, true_bb);
        } else {
            cfg_work_list.emplace_back(bb, false_bb);
        }
    } else {
        // put both sides
        cfg_work_list.emplace_back(bb, true_bb);
        cfg_work_list.emplace_back(bb, false_bb);
    }
} else {
    auto *jmp_bb = static_cast<BasicBlock *>(inst->get_operand(0));
    cfg_work_list.emplace_back(bb, jmp_bb);
}
```

根据情况判断可达 bb 推入 cfg_work_list 即可。

##### 可常量折叠的指令

```cpp
auto prev_status = value_map[inst];
auto cur_status = prev_status;
auto *folded = const_fold(value_map, inst, module);
if (folded != nullptr) {
    cur_status = ValueStatus{ValueStatus::CONST, folded};
} else {
    // 否则是 top 或 bot，取决于 operands 是否出现 bot
    cur_status = ValueStatus{ValueStatus::TOP};
    for (auto *op : inst->get_operands()) {
        auto *op_global = dynamic_cast<GlobalVariable *>(op);
        auto *op_arg = dynamic_cast<Argument *>(op);
        auto *op_inst = dynamic_cast<Instruction *>(op);
        if (op_global != nullptr || op_arg != nullptr ||
            (op_inst != nullptr &&
                value_map[op_inst].status == ValueStatus::BOT)) {
            cur_status = ValueStatus{ValueStatus::BOT};
        }
    }
}
if (cur_status != prev_status) {
    value_map[inst] = cur_status;
    for (auto use : inst->get_use_list()) {
        auto *use_inst = dynamic_cast<Instruction *>(use.val_);
        ssa_work_list.push_back(use_inst);
    }
}
```

如果能常量折叠，则折叠，说明是 Const 状态。否则根据操作数是否有 BOT，计算状态是 TOP 还是 BOT。如果状态发生变化，则推入需要更新的指令。

##### 其他指令

统一更新为 BOT 即可。

```cpp
auto prev_status = value_map[inst];
auto cur_status = ValueStatus{ValueStatus::BOT};
if (cur_status != prev_status) {
    value_map[inst] = cur_status;
    for (auto use : inst->get_use_list()) {
        auto *use_inst = dynamic_cast<Instruction *>(use.val_);
        ssa_work_list.push_back(use_inst);
    }
}
```

#### 收尾

这里需要干两件事情：

- 将所有 Const 状态的指令替换。
- 如果条件跳转的条件恒真/假，则改写成无条件跳转形式。

指令替换比较容易：

```cpp
std::vector<Instruction *> delete_list;
for (auto *bb : f->get_basic_blocks()) {
    for (auto *inst : bb->get_instructions()) {
        if (auto *constant = get_mapped(value_map, inst).value) {
            inst->replace_all_use_with(constant);
            delete_list.push_back(inst);
        }
    }
}
for (auto *inst : delete_list) {
    inst->get_parent()->delete_instr(inst);
}
```

无效跳转需要维护数据结构并消除无效的目的基本块开头的 bb：

```cpp
void branch_to_jmp(Instruction *inst, BasicBlock *jmp_bb,
                   BasicBlock *invalid_bb) {
    // 无条件跳转至 jmp_bb
    auto *bb = inst->get_parent();
    bb->remove_succ_basic_block(invalid_bb);
    invalid_bb->remove_pre_basic_block(bb);
    inst->remove_operands(0, 2);
    inst->add_operand(jmp_bb);
    // invalid_bb 如果开头有当前 bb 来的 phi，则进行精简
    auto delete_list = std::vector<Instruction *>{};
    for (auto *inst : invalid_bb->get_instructions()) {
        if (!inst->is_phi())
            break;
        for (auto i = 1; i < inst->get_operands().size();) {
            auto *op = inst->get_operand(i);
            if (op == bb) {
                inst->remove_operands(i - 1, i);
            } else {
                i += 2;
            }
        }
        if (inst->get_num_operand() == 2) {
            inst->replace_all_use_with(inst->get_operand(0));
            delete_list.push_back(inst);
        }
    }
    for (auto *inst : delete_list) {
        invalid_bb->delete_instr(inst);
    }
}
```

### 关键点分析

上面已经介绍的很详细了。提炼出一些重要的点：

- ValueStatus 格结构的建立及基本操作重载。
- const_fold 的实现（返回 Constant\*）。
- 不同指令的处理：phi, br, 可折叠指令，其他不可折叠指令。
- 指令状态的初始化，Argument\* 初始为 TOP，其他指令初始化为 BOT。操作数也可能是全局指令，所以我们需要将 `Status` 枚举类型的默认值设置为 BOT。

### 效果展示

我们考虑 `Test/Medium/assign_complex_expr.sy`。

```c
// Use complex expression in assign structure
int main () {
    int a;
    int b;
    int c;
    int d;
    int result;
    a = 5;
    b = 5;
    c = 1;
    d = -2;
    result = (d * 1 / 2)  + (a - b) - -(c + 3) % 2;
    putint(result);
    result = ((d % 2 + 67) + -(a - b) - -((c + 2) % 2));
    result = result + 3;
    putint(result);
    return 0;
}
```

我们发现优化后 .ll 文件直接变为：

```llvm
define i32 @main() {
label_entry:
  call void @put_int(i32 -1)
  call void @put_int(i32 71)
  br label %label_ret
label_ret:                                                ; preds = %label_entry
  ret i32 0
}
```

直接计算出了结果。

### 性能分析

运行结果如下所示：

```
===========TEST START===========
now in ./student/testcases/
-----------OPT RESULT-----------
5 cases in this dir
5 cases passed
optimization options: -sccp
Line: total 192 lines to 171 lines, rate: 11%
        4 cases better than no-opt
        2 cases 10% better than no-opt
        2 cases 20% better than no-opt
        best opt-rate is 38%, testcase: 03_cse
Time: total 0.03s to 0.03s, rate: 2%
        2 cases better than no-opt
        1 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 33%, testcase: 01_sccp_03
============TEST END============
===========TEST START===========
now in ./Test_H/Easy_H/
-----------OPT RESULT-----------
20 cases in this dir
20 cases passed
optimization options: -sccp
Line: total 398 lines to 392 lines, rate: 2%
        3 cases better than no-opt
        1 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 25%, testcase: 03_empty_stmt
Time: total 0.04s to 0.03s, rate: 8%
        15 cases better than no-opt
        12 cases 10% better than no-opt
        5 cases 20% better than no-opt
        best opt-rate is 26%, testcase: 03_empty_stmt
============TEST END============
===========TEST START===========
now in ./Test_H/Medium_H/
-----------OPT RESULT-----------
40 cases in this dir
40 cases passed
optimization options: -sccp
Line: total 3240 lines to 2972 lines, rate: 9%
        13 cases better than no-opt
        11 cases 10% better than no-opt
        10 cases 20% better than no-opt
        best opt-rate is 70%, testcase: assign_complex_expr
Time: total 0.08s to 0.07s, rate: 8%
        32 cases better than no-opt
        18 cases 10% better than no-opt
        5 cases 20% better than no-opt
        best opt-rate is 30%, testcase: max_subsequence_sum
============TEST END============
===========TEST START===========
now in ./Test_H/Hard_H/
-----------OPT RESULT-----------
10 cases in this dir
10 cases passed
optimization options: -sccp
Line: total 3751 lines to 3715 lines, rate: 1%
        5 cases better than no-opt
        0 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 7%, testcase: conv
Time: total 41.33s to 40.97s, rate: 1%
        6 cases better than no-opt
        4 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 20%, testcase: kmp
============TEST END============
===========TEST START===========
now in ./Test/Easy/
-----------OPT RESULT-----------
20 cases in this dir
20 cases passed
optimization options: -sccp
Line: total 398 lines to 392 lines, rate: 2%
        3 cases better than no-opt
        1 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 25%, testcase: 03_empty_stmt
Time: total 0.05s to 0.04s, rate: 13%
        16 cases better than no-opt
        10 cases 10% better than no-opt
        5 cases 20% better than no-opt
        best opt-rate is 40%, testcase: 02_var_defn
============TEST END============
===========TEST START===========
now in ./Test/Medium/
-----------OPT RESULT-----------
40 cases in this dir
40 cases passed
optimization options: -sccp
Line: total 2681 lines to 2488 lines, rate: 8%
        12 cases better than no-opt
        10 cases 10% better than no-opt
        9 cases 20% better than no-opt
        best opt-rate is 70%, testcase: assign_complex_expr
Time: total 0.10s to 0.09s, rate: 11%
        28 cases better than no-opt
        23 cases 10% better than no-opt
        15 cases 20% better than no-opt
        best opt-rate is 32%, testcase: sort_test6
============TEST END============
===========TEST START===========
now in ./Test/Hard/
-----------OPT RESULT-----------
10 cases in this dir
10 cases passed
optimization options: -sccp
Line: total 1851 lines to 1848 lines, rate: 1%
        2 cases better than no-opt
        0 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 2%, testcase: kmp
Time: total 0.03s to 0.03s, rate: 2%
        5 cases better than no-opt
        4 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 22%, testcase: nested_calls
============TEST END============
===========TEST START===========
now in ./function_test2020/
-----------OPT RESULT-----------
67 cases in this dir
67 cases passed
optimization options: -sccp
Line: total 3742 lines to 3641 lines, rate: 3%
        11 cases better than no-opt
        7 cases 10% better than no-opt
        5 cases 20% better than no-opt
        best opt-rate is 44%, testcase: 45_equal_prior_logic
Time: total 0.17s to 0.15s, rate: 8%
        46 cases better than no-opt
        31 cases 10% better than no-opt
        13 cases 20% better than no-opt
        best opt-rate is 43%, testcase: 98_many_local_var
============TEST END============
===========TEST START===========
now in ./function_test2021/
-----------OPT RESULT-----------
37 cases in this dir
37 cases passed
optimization options: -sccp
Line: total 1394 lines to 1334 lines, rate: 5%
        18 cases better than no-opt
        16 cases 10% better than no-opt
        6 cases 20% better than no-opt
        best opt-rate is 43%, testcase: 049_unary_op
Time: total 0.09s to 0.09s, rate: 8%
        25 cases better than no-opt
        17 cases 10% better than no-opt
        9 cases 20% better than no-opt
        best opt-rate is 45%, testcase: 018_sub2
============TEST END============
        All Tests Passed
```

可以发现 SCCP 的效果很大程度上取决于样例本身，平均优化效果可能从 1% 到 11% 不等。

在 assign_complex_expr.sy 达到了 70% 的代码消除。

### 参考文献

- Constant propagation on SSA form - EPFL (no date). Available at: <http://lampwww.epfl.ch/resources/lamp/teaching/advancedCompiler/2005/slides/05-UsingSSA_CP-1on1.pdf> (Accessed: January 10, 2023).
- SCCP(稀疏条件传播）, 知乎专栏. Available at: https://zhuanlan.zhihu.com/p/434113528 (Accessed: January 10, 2023).

## Common Sub-expression Elimination

### 前言

选择添加一道公共子表达式优化遍，原因是其是一项普遍应用的经典优化技术，对于诸如大规模数组读写的优化效果比较客观，值得尝试。

### 设计思想

公共子表达式消除分为两个阶段，一是局部的，在每个基本块中完成；另一个是全局的，在整个流图中完成。此外，基于 SSA 的 CSE 要更简单些，但是针对 load 指令的除外，load 指令会在后面着重讲。

对于局部公共子表达式，只需针对每个基本块进行操作：

- 遍历基本块内的每条指令，对于当前指令，(自当前位置开始)从后往前寻找公共子表达式，若找到，则进行替换；
- 只要找到可替换的指令，则还需针对当前基本块继续迭代，因为执行替换后，可能导致出现新的公共子表达式。

对于全局公共子表达式，首先需要求出可用表达式

- $gen[B]$：基本块 B 产生可用表达式 $f(x, \cdots)$，且随后没有对操作数 $x,\cdots$ 的定值，注意，对于 SSA 来说，显然不会在后面再定值，但是 load 除外，后面再说；
- $kill[B]$：基本块 B 注销含 z 的可用表达式，若它有对 z 的定值；

然后利用数据流迭代算法求解可用表达式

- $OUT[ENTRY] = \varPhi$
- $OUT[B] = gen_B\cup(IN[B] - kill_B)$
- $IN[B] = \cap_{P\in pred[B]}OUT[P]$

接着，有了可用表达式，就可以针对每个基本块中的指令，若其表达式为该**基本块入口处**的可用表达式，则可以跨基本块替换。

下面是一些边界情形：

- 对于 call 指令，只考虑不操作全局变量的调用（即纯函数调用，纯函数在死代码删除优化里提及）；
- 对于 load 指令，若 load 指令后面又有 store/call(call 里面修改了 load 地址的内容)，则认为该条 load 的值被注销，因此在局部消除时，反向寻找的时候一旦发现遇到对该地址的 store/call 则返回，前面的公共子表达式此时失效了；对于全局，则需要注意 gen 和 kill 的定义，以避免出错。

### 代码实现

首先定义了一个表达式的类 `Expression`(可以认为其代表等价类)，首先关键是判断两个表达式是否相等，表达式相等当且仅当它们的操作类型相等，且各操作数均相等。

其次，需判断哪些指令可优化

```cpp
bool isOptmized(Instruction *inst) {
    if (inst->is_void() || inst->is_alloca()) {
        return false;
    }
    if (inst->is_call() && !PureFunction::is_pure[dynamic_cast<Function *>(inst->get_operand(0))]) {
        return false;
    }
    if (inst->is_load() && (dynamic_cast<GlobalVariable *>(inst->get_operand(0)) || dynamic_cast<Argument *>(inst->get_operand(0)))) {
        return false;
    }
    return true;
}
```

- 空指令(ret/br/store/void call)以及 alloca 指令不可被优化；
- 非纯函数调用也不可优化，因为非纯函数可能会改变全局变量；
- 还有 load 全局变量或者函数参数的也暂不考虑优化，因为中间可能有函数会修改全局变量(函数参数也可能是全局变量的地址，同理可得)，但是我们无法感知；(实际上应该是可以处理的，考虑收集各个函数对那些全局变量进行了修改，但是代码编写就比较复杂了，暂时没处理)

此外，还需要判断 load 和 store 的地址是否相同

```c++
Value *lval_runner = val;
while (auto *gep_inst = dynamic_cast<GetElementPtrInst *>(lval_runner)) {
    lval_runner = gep_inst->get_operand(0);
}
```

这里通过不断迭代，找到最源头的那条 alloca 语句。

#### execute

下面介绍 execute 流程，核心是针对每个函数

```c++
do {
	localCSE(fun);
	globalCSE(fun);
} while (!delete_list.empty());
```

先后进行局部优化和全局优化，`delete_list` 收集待删除的指令，反复迭代原因是替换后可能导致新的公共表达式产生。

#### loacalCSE

```c++
auto *preInst = isAppear(inst, insts, i);
if (preInst != nullptr) {
	delete_list.push_back(inst);
	inst->replace_all_use_with(preInst);
}
```

localCSE 流程如前所述，注意实际上每找到一条即进行替换，保证一遍迭代找到尽可能多的公共子表达式。

#### globalCSE

```c++
void CSE::globalCSE(Function *fun) {
    delete_list.clear();
    calcGenKill(fun);
    calcInOut(fun);
    findSource(fun);
    replaceSubExpr(fun);
}
```

- 首先是计算可用表达式的 gen，kill，此外还有一个 `available` 用来收集所有的可用表达式；
- 其次，依据数据流方程迭代求解每个基本块的可用表达式；
- 接着，去寻找每个可用表达式的源头(可能有多个)
- 最后执行可用表达式替换

### 关键点分析

上面已经介绍了基本流程及实现，以下介绍关键点。

其一是针对 load 指令，考虑其是否被注销：

- 若 load 指令之后为一条 store 指令，并且根据前面的寻找源地址的函数，若源地址相同，则认为 load 指令的定值被 store 注销了；
- 若 load 指令之后为一条 call 指令，且 call 的参数的源地址与 load 的源地址相同，则认为此 load 指令的定值被 call 注销了（例如数组地址作为函数参数传递）；

其二是寻找跨基本块公共表达式的源头：

- 首先，经过了局部消除后，保证剩下的都是跨基本块的
- 如果该条指令所在的基本块入口处的可用表达式，没有该条指令对应的表达式，则该条指令可作为源头；
- 对于 load 指令，还要加一个前提是其定值不被注销；
- 还有一处细节时，Expression 类初始化时选取了一条指令作为代表元，并且将其加入了源头，若该条指令定值会被注销，还需要注意从源头里删除该条指令。

其三是执行公共子表达式替换

- 对于局部消除，找到后直接执行替换即可；(下面均针对全局)
- 对于全局消除，若该条指令的表达式不在其基本块入口的可用表达式里，则不替换；
- 源头的表达式不进行替换，这实际上由上面那条可以推出；
- 在入口处可用，但是在基本块内本条指令之前值被注销了也不替换；
- 若源头只有一条，直接进行替换；
- 若源头不止一条，查阅资料时发现，绝大多数算法通过对各个源头重命名(并且是命名为相同的名字)，这不适用于我们的框架，也不适用于 SSA，因此考虑添加 phi 指令，但是最终实现上还存在问题（主要是 load 指令导致的），因此本条作废；
- 上一条添加多个源头若只有一个源头实际可达，则也可以进行替换；

### 效果展示

测试的 sy 文件如下所示，在 test/student/testcases/03_cse.sy 里。

```c++
int main() {
        int x, y;
    int a = 1;
    int b = 5;
    int c = a + b;
    int d = a + b;
    float e = 4.3 * a;
    int flag = a + b;
    if (flag) {
        x = a * b;
        y = a * b;
    } else {
        x = a - e;
        y = a - e;
    }
    return (x + y);
}
```

首先未经优化的 SSA 中间代码由如下指令生成

```bash
./compiler -emit-ir -O ../test/student/testcases/03_cse.sy -o test.ll
```

```asm
define i32 @main() {
label_entry:
  %op8 = add i32 1, 5
  %op12 = add i32 1, 5
  %op15 = sitofp i32 1 to float
  %op16 = fmul float 0x4011333340000000, %op15
  %op20 = add i32 1, 5
  %op22 = icmp ne i32 %op20, 0
  br i1 %op22, label %label24, label %label31
label_ret:                                                ; preds = %label42
  ret i32 %op45
label24:                                                ; preds = %label_entry
  %op27 = mul i32 1, 5
  %op30 = mul i32 1, 5
  br label %label42
label31:                                                ; preds = %label_entry
  %op34 = sitofp i32 1 to float
  %op35 = fsub float %op34, %op16
  %op36 = fptosi float %op35 to i32
  %op39 = sitofp i32 1 to float
  %op40 = fsub float %op39, %op16
  %op41 = fptosi float %op40 to i32
  br label %label42
label42:                                                ; preds = %label24, %label31
  %op46 = phi i32 [ %op41, %label31 ], [ %op30, %label24 ]
  %op47 = phi i32 [ %op36, %label31 ], [ %op27, %label24 ]
  %op45 = add i32 %op47, %op46
  br label %label_ret
}
```

而经过 cse 优化的如下

```bash
./compiler -emit-ir -cse ../test/student/testcases/03_cse.sy -o test.ll
```

```asm
define i32 @main() {
label_entry:
  %op8 = add i32 1, 5
  %op15 = sitofp i32 1 to float
  %op16 = fmul float 0x4011333340000000, %op15
  %op22 = icmp ne i32 %op8, 0
  br i1 %op22, label %label24, label %label31
label_ret:                                                ; preds = %label42
  ret i32 %op45
label24:                                                ; preds = %label_entry
  %op27 = mul i32 1, 5
  br label %label42
label31:                                                ; preds = %label_entry
  %op35 = fsub float %op15, %op16
  %op36 = fptosi float %op35 to i32
  br label %label42
label42:                                                ; preds = %label24, %label31

  %op45 = add i32 %op46, %op46
  br label %label_ret
}
```

首先开头两个 `c/d/flag = a + b` 即为公共子表达式，只需保留 1 个

```asm
%op8 = add i32 1, 5
%op12 = add i32 1, 5
%op20 = add i32 1, 5
```

然后 `a*b` 和 `a-e` 也均为公共子表达式，只需保留 1 个。

```asm
%op27 = mul i32 1, 5
%op30 = mul i32 1, 5

%op34 = sitofp i32 1 to float
%op35 = fsub float %op34, %op16
%op36 = fptosi float %op35 to i32
%op39 = sitofp i32 1 to float
%op40 = fsub float %op39, %op16
%op41 = fptosi float %op40 to i32
```

并且

```asm
%op15 = sitofp i32 1 to float
%op34 = sitofp i32 1 to float
```

属于跨基本块，分析可用表达式可知，同样可以进行替换

而下面这条 phi 指令在执行替换之后

```asm
%op46 = phi i32 [ %op41, %label31 ], [ %op30, %label24 ]
%op47 = phi i32 [ %op36, %label31 ], [ %op27, %label24 ]
```

在执行替换之后(`op41` 被 `op36` 替换 `op30` 被 `op27` 替换)

`op46` 和 `op47` 完全一样，`op47` 被 `op46` 替换，

```asm
%op45 = add i32 %op47, %op46
```

也变成了

```asm
%op45 = add i32 %op46, %op46
```

### 性能分析

```text
===========TEST START===========
now in ./student/testcases/
-----------OPT RESULT-----------
5 cases in this dir
5 cases passed
optimization options: -cse
Line: total 192 lines to 181 lines, rate: 6%
        2 cases better than no-opt
        1 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 28%, testcase: 03_cse
Time: total 0.04s to 0.04s, rate: 0%
        2 cases better than no-opt
        2 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 17%, testcase: 03_cse
============TEST END============
===========TEST START===========
now in ./Test_H/Easy_H/
-----------OPT RESULT-----------
20 cases in this dir
20 cases passed
optimization options: -cse
Line: total 398 lines to 397 lines, rate: 1%
        1 cases better than no-opt
        0 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 2%, testcase: 20_hanoi
Time: total 0.04s to 0.05s, rate: -11%
        10 cases better than no-opt
        4 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 32%, testcase: 14_continue
============TEST END============
===========TEST START===========
now in ./Test_H/Medium_H/
-----------OPT RESULT-----------
40 cases in this dir
40 cases passed
optimization options: -cse
Line: total 3240 lines to 3129 lines, rate: 4%
        17 cases better than no-opt
        3 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 27%, testcase: register_realloc
Time: total 0.09s to 0.08s, rate: 12%
        30 cases better than no-opt
        22 cases 10% better than no-opt
        11 cases 20% better than no-opt
        best opt-rate is 53%, testcase: int_literal
============TEST END============
===========TEST START===========
now in ./Test_H/Hard_H/
-----------OPT RESULT-----------
10 cases in this dir
10 cases passed
optimization options: -cse
Line: total 3751 lines to 3569 lines, rate: 5%
        8 cases better than no-opt
        2 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 18%, testcase: bitset
Time: total 41.11s to 36.58s, rate: 12%
        9 cases better than no-opt
        7 cases 10% better than no-opt
        5 cases 20% better than no-opt
        best opt-rate is 47%, testcase: percolation
============TEST END============
===========TEST START===========
now in ./Test/Easy/
-----------OPT RESULT-----------
20 cases in this dir
20 cases passed
optimization options: -cse
Line: total 398 lines to 397 lines, rate: 1%
        1 cases better than no-opt
        0 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 2%, testcase: 20_hanoi
Time: total 0.05s to 0.04s, rate: 14%
        14 cases better than no-opt
        14 cases 10% better than no-opt
        11 cases 20% better than no-opt
        best opt-rate is 55%, testcase: 08_var_defn_func
============TEST END============
===========TEST START===========
now in ./Test/Medium/
-----------OPT RESULT-----------
40 cases in this dir
40 cases passed
optimization options: -cse
Line: total 2681 lines to 2587 lines, rate: 4%
        13 cases better than no-opt
        3 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 27%, testcase: register_realloc
Time: total 0.10s to 0.08s, rate: 17%
        34 cases better than no-opt
        30 cases 10% better than no-opt
        21 cases 20% better than no-opt
        best opt-rate is 36%, testcase: if_test
============TEST END============
===========TEST START===========
now in ./Test/Hard/
-----------OPT RESULT-----------
10 cases in this dir
10 cases passed
optimization options: -cse
Line: total 1851 lines to 1791 lines, rate: 4%
        8 cases better than no-opt
        1 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 11%, testcase: nested_calls
Time: total 0.03s to 0.02s, rate: 12%
        8 cases better than no-opt
        6 cases 10% better than no-opt
        4 cases 20% better than no-opt
        best opt-rate is 27%, testcase: nested_calls2
============TEST END============
===========TEST START===========
now in ./function_test2020/
-----------OPT RESULT-----------
67 cases in this dir
67 cases passed
optimization options: -cse
Line: total 3742 lines to 3608 lines, rate: 4%
        15 cases better than no-opt
        2 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 26%, testcase: 94_matrix_mul
Time: total 0.18s to 0.16s, rate: 14%
        59 cases better than no-opt
        48 cases 10% better than no-opt
        31 cases 20% better than no-opt
        best opt-rate is 42%, testcase: 36_domain_test
============TEST END============
===========TEST START===========
now in ./function_test2021/
-----------OPT RESULT-----------
37 cases in this dir
37 cases passed
optimization options: -cse
Line: total 1394 lines to 1390 lines, rate: 1%
        1 cases better than no-opt
        0 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 7%, testcase: 029_if_test2
Time: total 0.10s to 0.09s, rate: 12%
        24 cases better than no-opt
        19 cases 10% better than no-opt
        14 cases 20% better than no-opt
        best opt-rate is 60%, testcase: 054_hex_defn
============TEST END============
        All Tests Passed
```

如图所示，从中间代码行数来看，单独一个公共子表达式删除的效果可能并不好，有的平均只删除了 $\%1$ 的中间代码，不过针对特定代码的效果也比较好，例如`register_realloc` 达到了 $\%27$，`bitset` 达到了 $\%18$。

将优化 `pass` 综合起来，在 CSE 前加一道常量传播可以使得 CSE 效果更好。

### 困难

主要困难在于 load 指令相关的处理，以下为悬而未决的问题

- load 全局变量或函数参数
- 跨基本块且源头有多个，目前利用 phi 指令尚不能完全解决

### 参考文献

Muchnick, S.S. (2014) Advanced compiler design and implementation. Amsterdam etc.: Morgan Kaufmann Publishers an imprint of Elsevier.

## Dead Code Elimination

### 前言

死代码消除也是一个常见的优化遍。选择添加 DCE 优化遍，原因如下：

- 可以消除无用计算。
- 可以消除无用控制流（空跳转基本块等）。
- 可以消除无前驱非入口基本块。

能消除控制流使得这一 pass 很适合承接在其他优化进行后。

### 设计思想

死代码消除分为两个部分：无用指令清理和无用控制流清理。

无用指令清理基于逆向支配关系分析，是一个类似 GC 的 mark-sweep 两趟算法。

首先明确一个概念：重要指令。重要指令包含会改编环境（全局变量或指针参数）的指令，I/O 指令，非纯函数调用（函数调用内部会改变环境或者有 I/O 指令）以及 ret 和 jmp 指令。

同时，如果一个指令是重要的，那么它的所有操作数也是重要的。如果一个指令是重要的，那么它的反向支配前线的基本块的最后的 跳转语句也是 critical 的。

重要指令相当于不能被消除的指令，我们从重要指令出发，进行反向数据流分析，就可以 mark 所有的重要指令。

在 mark 完成后，执行 sweep 过程，删除其他未 mark 变量。

我们实现的无用控制流清理主要处理以下四种情况：

- i 以分支结尾，且两个分支是相同的
- i jmp 到 j，且 i 是空基本块
- i jmp 到 j，且 j 的前驱只有 i
- i 没有前驱

后三种情况的优化都会导致 cfg 改变，因此需要使用 changed 标记变量，循环遍历至不变为止。

### 代码实现

#### 重要指令判断

注意考虑到纯函数的优化。

```cpp
bool DeadCode::is_critical_inst(Instruction *inst) {
    if (inst->is_ret() ||
        (inst->is_store() && PureFunction::store_to_alloca(inst) == nullptr)) {
        return true;
    }
    if (inst->is_br()) {
        auto *br_inst = dynamic_cast<BranchInst *>(inst);
        // 一开始只有 jmp 需要标记
        return !br_inst->is_cond_br();
    }
    if (inst->is_call()) {
        // 只标记非纯函数的调用
        auto *call_inst = dynamic_cast<CallInst *>(inst);
        auto *callee = dynamic_cast<Function *>(call_inst->get_operand(0));
        return !is_pure[callee];
    }
    return false;
}
```

#### 纯函数分析

纯函数的判断是一个 bfs 过程，初始内部有副作用的函数及只有声明的 I/O 函数都不是纯函数，而如果一个函数不是纯函数，那么所有调用它的函数也不是纯函数。

主体流程：

```cpp
void markPure(Module *module) {
    is_pure.clear();
    auto functions = module->get_functions();
    std::vector<Function *> work_list;
    // 先考虑函数本身有无副作用，并将 worklists 初始化为非纯函数
    for (auto *f : functions) {
        is_pure[f] = markPureInside(f);
        if (!is_pure[f]) {
            work_list.push_back(f);
        }
    }
    // 考虑非纯函数调用的「传染」
    for (auto i = 0; i < work_list.size(); i++) {
        auto *callee_function = work_list[i];
        for (auto &use : callee_function->get_use_list()) {
            auto *call_inst = dynamic_cast<CallInst *>(use.val_);
            auto *caller_function = call_inst->get_function();
            if (is_pure[caller_function]) {
                is_pure[caller_function] = false;
                work_list.push_back(caller_function);
            }
        }
    }
}
```

#### mark 流程

主体代码：

```cpp
for (auto *bb : f->get_basic_blocks()) {
    for (auto *inst : bb->get_instructions()) {
        if (is_critical_inst(inst)) {
            marked.insert(inst);
            work_list.push_back(inst);
        } else if (inst->is_store()) {
            auto *lval = PureFunction::store_to_alloca(inst);
            if (lval_store.count(lval) != 0) {
                lval_store[lval].insert(static_cast<StoreInst *>(inst));
            } else {
                lval_store[lval] = {static_cast<StoreInst *>(inst)};
            }
        }
    }
}

// 遍历 worklist
for (auto i = 0; i < work_list.size(); i++) {
    auto *inst = work_list[i];
    // 如果数组左值 critical，则标记所有对数组的 store 也为 critical
    if (inst->is_alloca()) {
        auto *lval = static_cast<AllocaInst *>(inst);
        for (auto *store_inst : lval_store[lval]) {
            marked.insert(store_inst);
            work_list.push_back(store_inst);
        }
        lval_store.erase(lval);
    }
    // mark 所有 operand
    for (auto *op : inst->get_operands()) {
        auto *def = dynamic_cast<Instruction *>(op);
        if (def == nullptr || marked.count(def) != 0)
            continue;
        marked.insert(def);
        work_list.push_back(def);
    }
    // mark rdf 的有条件跳转
    auto *bb = inst->get_parent();
    for (auto *rdomed_bb : bb->get_rdom_frontier()) {
        auto *terminator = rdomed_bb->get_terminator();
        if (marked.count(terminator) != 0)
            continue;
        marked.insert(terminator);
        work_list.push_back(terminator);
    }
}
```

需要注意的是，函数内部数组的 store 指令一开始无法判断是否重要，需要先保存起来。

#### sweep 流程

此时可以清理没有被 mark 的变量了。

```cpp
void DeadCode::sweep(Function *f,
                     const std::unordered_set<Instruction *> &marked) {
    std::vector<Instruction *> delete_list{};
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *op : bb->get_instructions()) {
            if (marked.count(op) != 0)
                continue;
            if (!op->is_br()) {
                delete_list.push_back(op);
                continue;
            }
        }
    }
    for (auto *inst : delete_list)
        inst->get_parent()->delete_instr(inst);
}
```

#### 无用控制流清理

具体见源文件，这里主要展示几种情况的分类。

```cpp
for (auto *i : work_list) {
    // 先确保存在 br 语句
    auto *terminator = i->get_terminator();
    if (!terminator->is_br())
        continue;
    auto *br_inst = static_cast<BranchInst *>(terminator);
    if (br_inst->is_cond_br()) {
        // 先尝试替换为 jmp
        // ......
        // 若替换成功，可以继续执行下面代码
    }

    // 下面是对 i jmp 到 j 的优化
    auto *j = i->get_succ_basic_blocks().front();
    if (i->get_instructions().size() == 1) {
        // i 是空的，到 i 的跳转直接改为到 j 的跳转
        // ......
    } else if (j->get_pre_basic_blocks().size() == 1) {
        // j 只有 i 一个前驱，合并这两个基本块
        // ......
    }
}
```

#### 无前驱基本块删除

循环寻找无前驱非入口基本块删除即可。

### 关键点分析

需要注意：

- 数组相当于取别名，因此一旦左值是重要变量，就要标记所有的 store 指令为重要变量
- sweep 时需要正确维护前驱后继关系，这也是实验的难点。

### 效果展示

这里选取我们编写的测试样例 `student/testcases/02_pure_function.sy`。

```c
int b = 1;

int foo1() {
    // 这里可以有很多计算
    int sum = 0;
    int i = 0;
    while (i < 100) {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}

// 这个函数有副作用
int foo2() {
    b = b + 1;
}

int main() {
    int i = 0;
    int mark = 1;
    while (i < 100000) {
        foo1();
        if (mark == 1) {
            putint(3);
            foo2();
            mark = 0;
        }
        i = i + 1;
    }
    return 3;
}
```

`foo1()` 是个纯函数，且本身不是关键变量，因此对它的调用被优化。

生成 .ll 文件的 main 函数：

```llvm
define i32 @main() {
label_entry:
  br label %label4
label_ret:                                                ; preds = %label4
  ret i32 3
label4:                                                ; preds = %label_entry, %label18
  %op21 = phi i32 [ 1, %label_entry ], [ %op23, %label18 ]
  %op22 = phi i32 [ 0, %label_entry ], [ %op20, %label18 ]
  %op6 = icmp slt i32 %op22, 100000
  %op7 = zext i1 %op6 to i32
  %op8 = icmp ne i32 %op7, 0
  br i1 %op8, label %label9, label %label_ret
label9:                                                ; preds = %label4
  %op12 = icmp eq i32 %op21, 1
  %op13 = zext i1 %op12 to i32
  %op14 = icmp ne i32 %op13, 0
  br i1 %op14, label %label16, label %label18
label16:                                                ; preds = %label9
  call void @put_int(i32 3)
  %op17 = call i32 @foo2()
  br label %label18
label18:                                                ; preds = %label9, %label16
  %op23 = phi i32 [ %op21, %label9 ], [ 0, %label16 ]
  %op20 = add i32 %op22, 1
  br label %label4
}
```

### 性能分析

完整测试结果如下：

```
===========TEST START===========
now in ./student/testcases/
-----------OPT RESULT-----------
5 cases in this dir
5 cases passed
optimization options: -dce
Line: total 192 lines to 145 lines, rate: 25%
        5 cases better than no-opt
        4 cases 10% better than no-opt
        2 cases 20% better than no-opt
        best opt-rate is 45%, testcase: 01_sccp_03
Time: total 0.03s to 0.01s, rate: 69%
        3 cases better than no-opt
        3 cases 10% better than no-opt
        2 cases 20% better than no-opt
        best opt-rate is 89%, testcase: 02_pure_function
============TEST END============
===========TEST START===========
now in ./Test_H/Easy_H/
-----------OPT RESULT-----------
20 cases in this dir
20 cases passed
optimization options: -dce
Line: total 398 lines to 368 lines, rate: 8%
        9 cases better than no-opt
        5 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 24%, testcase: 13_break
Time: total 0.04s to 0.04s, rate: -2%
        11 cases better than no-opt
        7 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 24%, testcase: 04_arr_defn
============TEST END============
===========TEST START===========
now in ./Test_H/Medium_H/
-----------OPT RESULT-----------
40 cases in this dir
40 cases passed
optimization options: -dce
Line: total 3240 lines to 3013 lines, rate: 8%
        29 cases better than no-opt
        10 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 29%, testcase: short_circuit
Time: total 0.08s to 0.08s, rate: 6%
        23 cases better than no-opt
        15 cases 10% better than no-opt
        8 cases 20% better than no-opt
        best opt-rate is 35%, testcase: skip_spaces
============TEST END============
===========TEST START===========
now in ./Test_H/Hard_H/
-----------OPT RESULT-----------
10 cases in this dir
10 cases passed
optimization options: -dce
Line: total 3751 lines to 3550 lines, rate: 6%
        10 cases better than no-opt
        1 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 19%, testcase: conv
Time: total 41.96s to 41.73s, rate: 1%
        5 cases better than no-opt
        1 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 15%, testcase: side_effect
============TEST END============
===========TEST START===========
now in ./Test/Easy/
-----------OPT RESULT-----------
20 cases in this dir
20 cases passed
optimization options: -dce
Line: total 398 lines to 368 lines, rate: 8%
        9 cases better than no-opt
        5 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 24%, testcase: 13_break
Time: total 0.05s to 0.05s, rate: 2%
        12 cases better than no-opt
        5 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 29%, testcase: 20_hanoi
============TEST END============
===========TEST START===========
now in ./Test/Medium/
-----------OPT RESULT-----------
40 cases in this dir
40 cases passed
optimization options: -dce
Line: total 2681 lines to 2507 lines, rate: 7%
        28 cases better than no-opt
        10 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 19%, testcase: while_break_test
Time: total 0.10s to 0.09s, rate: 8%
        28 cases better than no-opt
        19 cases 10% better than no-opt
        8 cases 20% better than no-opt
        best opt-rate is 30%, testcase: nested_calls
============TEST END============
===========TEST START===========
now in ./Test/Hard/
-----------OPT RESULT-----------
10 cases in this dir
10 cases passed
optimization options: -dce
Line: total 1851 lines to 1669 lines, rate: 10%
        9 cases better than no-opt
        5 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 13%, testcase: brainfk
Time: total 0.03s to 0.02s, rate: 5%
        6 cases better than no-opt
        3 cases 10% better than no-opt
        1 cases 20% better than no-opt
        best opt-rate is 28%, testcase: nested_calls2
============TEST END============
===========TEST START===========
now in ./function_test2020/
-----------OPT RESULT-----------
67 cases in this dir
67 cases passed
optimization options: -dce
Line: total 3742 lines to 3521 lines, rate: 6%
        49 cases better than no-opt
        12 cases 10% better than no-opt
        2 cases 20% better than no-opt
        best opt-rate is 24%, testcase: 60_while_fibonacci
Time: total 0.16s to 0.16s, rate: 5%
        42 cases better than no-opt
        26 cases 10% better than no-opt
        10 cases 20% better than no-opt
        best opt-rate is 42%, testcase: 23_if_test2
============TEST END============
===========TEST START===========
now in ./function_test2021/
-----------OPT RESULT-----------
37 cases in this dir
37 cases passed
optimization options: -dce
Line: total 1394 lines to 1303 lines, rate: 7%
        11 cases better than no-opt
        6 cases 10% better than no-opt
        0 cases 20% better than no-opt
        best opt-rate is 20%, testcase: 049_unary_op
Time: total 0.09s to 0.09s, rate: 7%
        24 cases better than no-opt
        16 cases 10% better than no-opt
        9 cases 20% better than no-opt
        best opt-rate is 39%, testcase: 052_comment1
============TEST END============
        All Tests Passed
```

可以看出 DCE 可以删除很多无用代码，平均能删除接近 10%。

因为 DCE 能删除无用控制流和无前驱的非入口基本块，所以放在别的优化遍之后效果会更好。

具体效率上很多样例也会有 10% 以上的提高。

### 参考文献

- Dead-Clean-SCCP (no date) Comp 512: Advanced compiler construction. Available at: https://www.clear.rice.edu/comp512/Lectures/ (Accessed: January 10, 2023).
- Cooper, K.D. and Torczon, L. (no date) “10.2 Eliminating Useless and Unreachable Code,” in Engineering a Compiler 2nd edition, pp. 544–551.

## 新增源代码文件说明

新编写 sy 源代码文件统一在 student/testcases 目录下，输入输出数据格式与其他测试用例文件夹保持一致。

此外，我们移植了往年的一些与现有测试样例不重复的代码，放在了 `function_test2020` 与 `function_test2021` 文件夹下。也会统一参与评测。

（注：在我们检查重复性的过程中，发现 Test 与 Test_H 目录也有部分样例重复，或许明年框架可以优化掉这些样例）

## 评测脚本使用方式

eval.sh 已经有可执行权限，直接在 student 工作目录下 `./eval.sh` 即可。

评测脚本会调用上一级目录的 test.py，这是我们在原先 test.py 的基础上魔改后的文件，可以显示每个评测目录的正确性以及代码优化指标。

也可以手动在 test 工作目录下调用 test.py，可以指定不同的优化选项。

```shell
python test.py -sccp
python test.py -sccp -cse
python test.py -sccp -cse -dce
```

以开启全部优化为例，输出格式如下：

```
===========TEST START===========
now in ./student/testcases/
-----------OPT RESULT-----------
5 cases in this dir
5 cases passed
optimization options: -dce -sccp -cse
Line: total 192 lines to 111 lines, rate: 43%
        5 cases better than no-opt
        4 cases 10% better than no-opt
        3 cases 20% better than no-opt
        best opt-rate is 80%, testcase: 03_cse
Time: total 0.03s to 0.01s, rate: 70%
        2 cases better than no-opt
        2 cases 10% better than no-opt
        2 cases 20% better than no-opt
        best opt-rate is 91%, testcase: 02_pure_function
============TEST END============
```

`TEST START` 和 `TEST END` 标识了一个评测文件夹的评测开始和结束。目前会评测以下文件夹：

```python
TEST_DIRS = [
        './student/testcases/',
        './Test_H/Easy_H/',
        './Test_H/Medium_H/',
        './Test_H/Hard_H/',
        './Test/Easy/',
        './Test/Medium/',
        './Test/Hard/',
        './function_test2020/',
        './function_test2021/'
        ]
```

`./student/testcases/` 即为我们自己编写的测试样例文件夹。
