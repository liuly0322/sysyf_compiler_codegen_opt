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
genPhi();
valueDefineCounting();
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

#### B2-2

#### B2-3

### 活跃变量分析

#### B3-1

### 检查器

#### B4-1
