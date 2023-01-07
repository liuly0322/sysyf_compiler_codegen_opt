# 实验报告

## 必做部分

### 支配树

#### B1-1

证明：若 $x$ 和 $y$ 支配 $b$，则要么 $x$ 支配 $y$，要么 $y$ 支配 $x$。

如果仅从支配关系的定义出发，可以使用反证法：假设 $x$ 不支配 $y$ 且 $y$ 不支配 $x$。

任取一条入口到 $b$ 结点的路径，由于 $x$ 和 $y$ 支配 $b$，这条路径上存在若干个 $x$ 结点和若干个 $y$ 结点。

因为 $x$ 不支配 $y$，一条从起点开始，经过 $x$ 到 $y$ 的路径可以被替换为从起点开始，不经过 $x$ 到 $y$ 的路径；同理一条从起点开始经过 $y$ 到 $x$ 的路径可以被替换为从起点开始不经过 $y$ 到 $x$ 的路径。

所以，只要上述的入口到 $b$ 结点的路径上同时存在 $x$ 结点和 $y$ 结点，就可以对该路径从起点开始的部分进行替换，减少 $x$ 结点或 $y$ 结点，直至最后路径上只有 $x$ 结点或只有 $y$ 结点。这就说明 $x$ 和 $y$ 并不同时支配 $b$ 结点，与题设矛盾。

综上所述，原命题成立：要么 $x$ 支配 $y$，要么 $y$ 支配 $x$。

第二种证明方式是直接使用支配树的一个性质（可能有循环论证的嫌疑，但这样更为直观）：

$Dom(b) = \{b\} ∪ IDom(b) ∪ IDom(IDom(b)) \cdots \{n0\}$

$x$ 和 $y$ 在支配树上都是 $b$ 的祖先结点，因此要么 $x$ 支配 $y$，要么 $y$ 支配 $x$。

#### B1-2

`Figure 1: The Iterative Dominator Algorithm` 的内层 `for` 循环是否一定要以后序遍历的逆序进行，为什么？

不一定，因为循环判断条件是 `Changed` 变量，当未发生改变时，说明已经达到了不动点，也即找到了一个解。

后序遍历的逆序的好处是尽可能减少收敛需要的速度（对于 DAG 图，就是拓扑排序，一次即可收敛）。

#### B1-3

`Figure 3: The Engineered Algorithm` 为计算支配树的算法。在其上半部分的迭代计算中，内层的 `for` 循环是否一定要以后序遍历的逆序进行，为什么？

与上个问题结论类似，并不一定需要后序遍历的逆序。

#### B1-4

`Figure 3: The Engineered Algorithm` 为计算支配树的算法。其中下半部分 `intersect` 的作用是什么？内层的两个 `while` 循环中的小于号能否改成大于号？为什么？

`intersect` 的作用是取交集。不可以，因为 `doms` 数组按照结点的后序索引，而每次需要将 "更深" 的结点更新为直接支配它的结点，也就是每次更新索引更小的结点。如果需要改成大于号，则循环内部的更新操作也需要对应交换。

#### B1-5

这种通过构建支配树从而得到支配关系的算法相比教材中算法 9.6 在时间和空间上的优点是什么？

为了方便分析，这里认为教材中算法 9.6 也是采用结点的深度优先序。因此，教材中的算法就是文章中一开始介绍的简单迭代算法（未经过数据结构优化）。

而通过构建支配树的策略：

在时间上省下了为每个结点分配 Doms 集合以及数据移动的开销（具体来说，二者迭代次数是相同的，但是基于构建支配树的策略每次迭代平均复杂度 $O(N+ED)$，且常数较小，教材中的算法每次迭代的开销取决于集合的具体实现，例如使用并查集，考虑拷贝开销，复杂度不低于 $O(ND+ED\alpha(D))$）。

在空间上只使用一个 Doms 数组，即 $O(N)$，而不是每个结点使用一个集合，即 $O(N^2)$。

#### B1-6

在反向支配树的构建过程中，是怎么确定 EXIT 结点的？为什么不能以流图的最后一个基本块作为 EXIT 结点？

```cpp
for (auto bb: f->get_basic_blocks()) {
    auto terminate_instr = bb->get_terminator();
    if (terminate_instr->is_ret()) {
        exit_block = bb;
        break;
    }
}
```

以第一个出现的 `ret` 结尾的基本块作为 EXIT 结点（这里事实上假定了每个函数只有一个出口基本块）。因为流图的最后一个基本块未必是函数出口（甚至未必能到达）。

### Mem2Reg

#### B2-1

请说明`Mem2Reg`优化遍的流程。

##### 概述

消除 store 与 load，修改为 SSA 的 def-use/use-def 关系，并且在适当的位置安插 Phi 和 进行变量重命名，再删除 alloca 指令。

条件：n 支配 m 的一个前驱且 n 并不严格支配 m，则说 m 在 n 的支配边界内，即 m 是 n 在 cfg 上可达但并不支配的第一个结点。则在 m 插入 phi 函数。

##### isLocalVarOp 函数

用于判断是否是需要被优化的 load/store op

- 是 load/store op
- 左值不是全局变量也不是数组指针

##### execute 函数

省略初始化：

```cpp
// 内部 load 转发
insideBlockForwarding();
// 根据 DF 生成空 phi 指令
genPhi();
// 记录基本块可用左值
valueDefineCounting();
// 递归 forward
valueForwarding(func_->get_entry_block());
// 删除所有的变量 alloca 指令（不包含数组）
removeAlloc();
```

##### insideBlockForwarding 函数

可以认为，对于一个变量（非数组）而言，store 定值，load 使用。消除 load 的过程就是将 load op 替换（forward）为对应地址的值，这需要 store 的信息。

有些 load op 只依赖于基本块内部的 store op，这个函数会消除这些 load/store。

这个函数首先遍历所有的变量（非数组） load/store 指令。

访问 store op 时维护如下信息：

- defined_list: 地址到 store 指令的映射
- new_value: 地址到值的映射
- delete_list: 需要删除的 store 指令（对于某个地址，非最后一条的 store 指令（因为最后一条 store 指令可能后续基本块还需要使用））

处理过程中需要注意，如果 store 的 rval 是被 forward 的 load op，需要替换为 forward 后的值。

访问 load op 时维护如下信息：

- forward_list: load op forward 到值

如果加载的地址之前已经有某条 store，forward_list 就可以增添 load op 到当前地址值的映射（根据 new_value）。

对于 store 指令：

在遍历完 load/store 指令后，进行替换：对于所有能 forward 的 load op，在用到的地方都替换为值。

这时，所有被 forward 的 load 指令和 delete_list 中的 store 指令也可以删除了。

##### genPhi 函数

这一部分与 pdf 算法类似。

1. 在 globals 中记录所有的被 load 的地址。因为局部的 load 已经被消除，这里的 load 都是跨越基本块的。

2. 在 defined_in_block 中记录地址在哪些基本块有 store 行为。

bb_phi_list 是每个基本块需要生成 phi 指令的地址的集合。

##### valueDefineCounting 函数

对于每个基本块，记录所有「可用的」地址，包括 phi 指令和 store 指令。

##### valueForwarding 函数

这一部分也与 pdf 的算法类似。相当于 Rename 函数，重命名，并填充 phi 函数。

##### removeAlloc 函数

删除无用的 alloca 指令。

#### B2-2

#### B2-3

### 活跃变量分析

#### B3-1

### 检查器

#### B4-1

## 选做-死代码消除

### 参考

https://www.clear.rice.edu/comp512/Lectures/10Dead-Clean-SCCP.pdf
http://nwjs.net/news/282538.html

（这个课程和 DCE.pdf 用的就是同一本书？考虑一下引用出处怎么写）

### 概念

解决以下问题：

- 无用的操作 DEAD（死代码消除）
- 无用的控制流 CLEAN（跳转至跳转）
- 无法到达的基本块（这个还没实现）

DAED：活跃变量分析或者 Mark-Sweep 的两趟流程（PDF 上有）
CLEAN：在消除死代码之后，CFG 可能会有空（以分支或跳转结尾）的基本块，需要清理
（Devised by Rob Shillingsburg ( 1992), documented by John Lu (1994)）

### DEAD

基于逆向支配关系分析，具体见 pdf 算法。

首先 mark critical 变量（改变环境（全局变量，指针参数），或者有 I/O 行为，或者是返回，jmp 语句）

- 这里区分 branch 语句和 jmp 语句（下文统称为 br 指令）

如果 x <- call f, ... 中，f 不是纯函数，那么 x 是 critical 的

如果 x <- y op z 中，x 是 critical 的，那么 y 和 z 也得是 critical 的

如果 x 是 critical 的，那么 x 的 rdf 反向支配前线的基本块的最后的 branch 语句也是 critical 的

critical 变量实际就是函数执行流上必须要计算的变量，其他的变量都是可以删除的

在 mark 完成后，执行 sweep 过程，删除其他未 mark 变量，并且 mark 变量所在 bb 的 rdf 的有条件跳转（原因见后面 clean 部分）

#### 纯函数

与上述 critical 变量的内涵类似，纯函数就是不改变环境，也没有 I/O 行为的函数

判断纯函数的好处是：

```cpp
int foo1() {
    int i = 100;
    while (i > 0) {
        putint(i);
        i = i - 1;
    }
    return 0;
}

int foo2() {
    int i = 100;
    while (i > 0) {
        i = i - 1;
    }
    return 0;
}

int main() {
    foo1();
    foo2();
    return 0;
}
```

foo2 是一个纯函数，对它的计算可以优化掉。

纯函数的判断过程类似一个 bfs，因为非纯函数会「传染」到所有它的调用者

第一遍先单独对每个函数初步判断是否是纯函数（是否有改变环境行为，这里先不考虑函数调用）

```cpp
bool DeadCode::markPureInside(Function *f) {
    // 只有函数声明，无法判断，认为非纯（主要是处理 runtime 的 I/O 函数）
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
```

这里用到了一个辅助函数：

```cpp
static inline AllocaInst *store_to_alloca(Instruction *inst) {
    // lval 无法转换为 alloca 指令说明是非局部的，有副作用
    Value *lval_runner = inst->get_operand(1);
    while (auto *gep_inst = dynamic_cast<GetElementPtrInst *>(lval_runner)) {
        lval_runner = gep_inst->get_operand(0);
    }
    return dynamic_cast<AllocaInst *>(lval_runner);
}
```

之后从非纯函数出发，进行 bfs

```cpp
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
```

#### 数组别名

这是实现上的一个细节问题。

对于 x <- y op z，如果 x critical，很容易说明 y 和 z 也 critical。

但如果对局部数组有使用，这里涉及到一个别名问题，需要将局部数组的所有可能定值都设为 critical

采取的策略是：第一遍对 store 指令建立起 lval 到 `set<StoreInst*>` 的映射，在遍历时，如果发现 alloca（也就是数组左值）critical，那么就把对数组的所有定值设为 critical。

### CLEAN

- br 目标相同，则替换成 jmp
- 空块 jmp 到其他块，则将这两个块合并
- B1 顺序执行 jmp 到 B2（B2 无其他前驱），则将两个块合并
- jmp 到一个空 Br 块，则 jmp 替换成 br（可能会导致 B2 成为不可达的基本块）

实现细节：

- 后序遍历，以便先清理某个结点的后继节点
- 导致：
  - 回边未处理，需要多次迭代
  - 迭代可能会改变 cfg，所以下次迭代要重新计算迭代顺序
- 注意维护 bb 间前驱后继关系

空块的 branch 自环无法消除？

- 其实可以由 dead 消除（假设程序出口唯一，说明这其实是无用的自环）
