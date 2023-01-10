# 实验报告

## 必做部分

### 支配树

#### B1-1

反证：若 $x$ 和 $y$ 支配 $b$, 假设 $x$ 不支配 $y$ 且 $y$ 也不支配 $x$。

考虑一条从入口 $n_0$ 到 $b$ 的路径，由 $x$ 和 $y$ 均支配 $b$，因此 $x$ 和 $y$ 都在这条路径上，事实上这条路径总可以写成 $P(n_0,\ x) + P(x,\ b)$（其中 $P(x,\ b)$ 不含 $y$）或者 $P(n_0,\ y) + P(y,\ b)$（其中 $P(y,\ b)$ 不含 $x$）

- 由于 $x$ 不支配 $y$，因此存在一条从 $n_0$ 到 $y$ 的路径不含 $x$，记为 $P_1(n_0,\ y)$
- 由于 $y$ 不支配 $x$，因此存在一条从 $n_0$ 到 $x$ 的路径不含 $y$；记为 $P_1(n_0,\ x)$

因此 $P(n_0,\ x) + P(x,\ b)$ 可以被替换为 $P_1(n_0,\ x) + P(x,\ b)$，为一条从 $n_0$ 到 $b$ 的路径且不含 $x$，这与 $x$ 支配 $b$ 矛盾！

对于 $P(n_0,\ y) + P(y,\ b)$ 同理可得。

综上，若 $x$ 和 $y$ 支配 $b$，则要么 $x$ 支配 $y$，要么 $y$ 支配 $x$。

#### B1-2

不一定，因为循环判断条件是 `Changed` 变量，当未发生改变时，说明已经达到了不动点，也即找到了一个解。

后序遍历的逆序的好处是尽可能减少收敛需要的速度（对于 DAG 图，就是拓扑排序，一次即可收敛）。

#### B1-3

与上个问题结论类似，并不一定需要后序遍历的逆序。

#### B1-4

`intersect` 的作用是取两个 Dom 集合的交集（实际是返回一个索引）。

不能换成大于号。在上半部分的迭代计算中，内层的 `for` 循环是以后序遍历的逆序进行，节点索引也是**后序遍历的逆序**，在支配树中越**上层**的结点其索引越**大**，因此取交集的时候，是要往上层更新为直接支配结点（`doms` 数组实际维护的是 IDom 信息），也就是说索引小的才应该要变化，因此是小于号。

如果要改成大于号，则上半部分迭代的顺序和索引的构建需要相应改变。

#### B1-5

为了方便分析，这里认为教材中算法 9.6 也是采用结点的深度优先序。因此，教材中的算法就是文章中一开始介绍的简单迭代算法（未经过数据结构优化）。

而通过构建支配树的策略：

在时间上省下了为每个结点分配 Doms 集合以及数据移动的开销（具体来说，二者迭代次数是相同的，但是基于构建支配树的策略每次迭代平均复杂度 $O(N+ED)$，且常数较小，教材中的算法每次迭代的开销取决于集合的具体实现，例如使用并查集，考虑拷贝开销，复杂度不低于 $O(ND+\alpha(D)ED)$）。

在空间上只使用一个 Doms 数组，即 $O(N)$，而不是每个结点使用一个集合，即 $O(N^2)$。

#### B1-6

在 `RDominateTree` 中，是使用如下代码确定 exit 节点的：

```C++
for (auto bb : f->get_basic_blocks()) {
	auto terminate_instr = bb->get_terminator();
	if (terminate_instr->is_ret()){
		exit_block = bb;
		break;
	}
}
```

也就是选取以 ret 指令结尾的块作为 exit 节点（这种实现认为只有一个块包括 ret 指令）。流图的最后一个基本块不一定以 ret 指令结尾，也就是说不一定是函数的出口，所以不能将流图的最后一个基本块认为是 exit 节点。

### Mem2Reg

> 在分析代码实现之前，不妨先来回顾一下 SSA 的动机。
>
> 这一遍优化，对于优化前的 IR 考虑三种语句：
>
> - $x=\text{alloca }T$（表示开辟一个 $T$ 类型的内存单元，指针放入 $x$）
> - $x=\text{load }T,ptr$（表示从 $ptr$ 指向的内存单元中读取一个 $T$ 类型的值，放入 $x$）
> - $\text{store }T\ x,ptr$（表示把一个存在 $x$ 中的 $T$ 类型值放入 $ptr$ 指向的内存单元）
>
> 构造 SSA，需要在 CFG 的汇合点处插入 $\phi$ 函数，并且在最简单的实现中，如果某个 Block 有 $n$ 个前驱，则在此 Block 中定义的变量的 $\phi$ 函数就要有 $n$ 个参数。
>
> - $x_m=\phi(x_a,...,x_l)$（重命名之后的）
>
> 第二步是重命名，即相当于给每个同名变量加下标以示区分。
>
> 但是在引入支配信息之后，$\phi$ 函数可以简化，可以去除一些不必要的引入。
>
> - 对于只在单个 bb 中活跃的变量肯定不需要 $\phi$ 函数，所以只需要统计哪些名字是跨 bb 的，只对这些名字考虑插入 $\phi$ 函数就行了
>
> ```Python
># 找到跨域名字的算法
> Globals = {}
> 任意名字i, Blocks[i] = {}
> for 块b:
>     注销的定值VarKill = {}
>     for 指令i x = y op z: # 按顺序
>         if y not in VarKill:
>             # y在此之前从未定值，说明不是在此定义，此处只是引用，说明y是个跨域的名字
>             Globals += y
>         if z not in VarKill:
>             Globals += z
>         VarKill += x
>         # x 此处有定值，**谨慎**起见后面的都不能算跨域
>         Blocks(x) += b
> ```
> 
> ```Python
># 插入 φ 函数的算法
> for 名字x in Globals:
>     WorkList = Blocks(x)
>     for 基本块b in WorkList:
>         for 基本块d in {b的支配边界}:
>             if d 没有关于 x 的 φ 函数:
>                 在 d 中插入关于 x 的 φ 函数
>                 WorkList += 块d # 传播
> ```
> 
> - 重命名时，为每个名字设置一个栈
>
> ```Python
>def NewName(n:名字):
>     i = counter[n]
>     counter[n]++
>     stack[n].push(i)
>     return "n_i"
> 
> def Rename(b:块):
>    for φ语句 "x = φ(...)" in b:
>         重命名 x 为 NewName(x)
>     for 指令i "x = y op z" in b:
>         重命名 y 为 stack[y].top()
>         重命名 z 为 stack[z].top()
>         重命名 x 为 NewName(x)
>     for b的所有 CFG 上后继块:
>         填充φ语句的参数
>     for b的支配树上后继块s:
>         Rename(s)
>     for b中对x定值的语句（包括phi）:
>         stack[x].pop()
> 
> def main():
>    for 跨域名字i:
>         counter[i] = 0
>         stack[i] = {}
>     Rename(entry)
> ```

#### B2-1

以下分函数来阐述 `Mem2Reg` 优化遍的流程。

> **Remark**：
>
> - store 指令可看作 `store rvalue lvalue`（*lvalue = rvalue）
> - load 指令可看做 `rvalue = load lvalue` （rvalue = *lvalue）；但是 `loadInst` 类中实际上并无 `rvalue` 成员，这里的 `rvalue` 应当看作一个临时的、匿名的变量，它是 `load lvalue` 的结果。
>- 左值 (lvalue) 实际上是 `alloca` 的地址，以下左值即指这些地址，而右值指地址中实际的值。
> - `isLocalVarOp` 是一个辅助函数，仅当指令满足条件：左值为非全局变量且非数组指针，且为 store/load 指令时返回 `true`。实际上也就是可能可以被优化掉的 store/load 指令。

##### execute

这是 `Mem2Reg` 执行的总流程，遍历每个函数，进行如下操作

- `insideBlockForwarding`，进行基本块内优化，删去了块内可被优化的 store/load 指令；
- `genPhi`，生成 $\phi$ 指令；
- `valueDefineCounting`，记录每个基本块中定值的左值列表；
- `valueForwarding(func_->get_entry_block())`，进行重命名（即 `Rename(s)`）；
- `removeAlloc`，删除 `alloca` 指令；

##### insideBlockForwarding

用到的数据结构，生存周期均为一个基本块：

- `defined_list`：`map<Value, Instruction>`，存储**左值**到**对该左值定值的(store)指令**的映射；
- `forward_list`：`map<Instruction, Value>`，存储 **load 指令**到**右值**的映射。 并且此 load 指令的左值在本基本块已被定值，而映射的正是对此左值定值的右值。所以实际上这条 load 指令可以被删去，其 `rvalue` 可以被映射的右值替换；
- `new_value`：`map<Value, Value>`，存储**左值**到**(最新的)对该左值定值的右值**的映射；
- `delete_list`：`set<Instruction>`，存储定值被注销的(store)指令

此函数的流程为，首先遍历每个基本块中的每条可被优化的指令(`isLocalVarOp` 判断为真的指令)。

**对于 store 指令**：

- 若 `rvalue` 为 load 指令右值且该条 load 指令在 `forward_list` 中，则替换右值；
- 若 `lvalue` 已在 `defined_list` 中，则将那条 store 指令加入`delete_list` 中（表明那条指令的定值在本块内即已被注销），`defined_list` 中的值更新为本条指令；否则 `defined_list` 加入这条 store 指令；
- 若 `lvalue` 在 `new_value` 中，则更新其值，否则新增；

**对于 load 指令**：

- 若 `lvalue` 不在 `defined_list` 中，则无事发生（说明在本基本块中没有对此左值的定值，也即其跨基本块）；否则在 `new_value` 中找到对该左值定值的右值，加入 `forward_list` 中；

接下来，遍历 `forward_list`，对用到 `rvalue` 的地方进行替换，然后删除该 load 指令；最后删除 `delete_list` 中的 store 指令。

**总结**：本函数删去了不必要的 load（块内对 load 的左值定值过）和 store（其定值在块内被其他语句注销），对用到那些右值的地方进行了替换维护。

##### genPhi

用到的数据结构：

- `globals`：`set<Value>`，存储 load 指令的 `lvalue`，由于经过 `insideBlockForwarding`，此时的 load 都是跨基本块的；
- `defined_in_block`：`map<Value, set<BasicBlock>>`，存储 `lvalue` 到其被定义的 `BasicBlock` 的映射；
- `bb_phi_list`：`map<BasicBlock, set<Value>>`，存储 `BasicBlock` 到该基本块中需要 $\phi$ 指令的 `lvalue` 集合的映射。

以上三者生存周期为单个函数，以下生存周期对应与 `globals` 中的单个 `lvalue`

- `define_bbs`：由 `defined_in_block` 维护，即 `lvalue` 在哪些 `BasicBlock` 中被定义；
- `queue`：`vector<BasicBlock>`，初始为 `define_bbs`。

此函数的流程为，首先遍历每个基本块中的每条可被优化的指令(`isLocalVarOp` 判断为真的指令)。

- 对于 load 指令：将 `lvalue` 加入 `globals`，因为经过 `insideBlockForwarding`，此时保留的是跨基本块的，所以加入 `globals`；

- 对于 store 指令：将 `lvalue` 对应的基本块加入 `defined_in_block`；

然后，遍历 `globals` 中的每个变量，然后遍历 `queue` 中的每个基本块；遍历该基本块的支配边界，若该基本块对应的需要 $\phi$ 指令的 `lvalue` 集合中没有该 `lvalue` ，则创建 $\phi$ 指令，并加入，然后将此基本块入队；否则新建 $\phi$ 函数。

**总结**：本函数主要插入 $\phi$ 指令，条件是：若基本块 $b$ 中有对 $x$ 的定义，且其跨基本块作用，则在 $DF(b)$ 中的每个结点起始处都放置一个对应的 $\phi$ 函数。

##### valueDefineCounting

本函数主要维护 `define_var`，存储 `BasicBlock` 到该基本块中定值的左值列表的映射。

流程为，遍历每个基本块的每个指令

- 若为 $\phi$ 指令，将左值加入；
- 若为 store 指令，且满足 `isLocalVarOp`，将左值加入；

##### valueForwarding

本函数实际上即为 `Rename`，进行重命名，同时还删除了不必要的 store/load 指令。

用到的数据结构：

- `value_status`：`map<Value, vector<Value>>`，存储 `lvalue` 到其对应的 $\phi$ **指令**或对应的 store 指令的**右值**的列表的映射，实际上相当于 pdf 中的 Stack，记录最新的右值信息；
- `visited`：`set<BasicBlock>`，标记已访问过的 Block；

以上两个生存周期为全局，以下为单个基本块

- `delete_list`：`set<Instruction>`，存储待删除的 load/store 指令

此函数的流程为，首先标记本基本块已访问，遍历基本块的每条指令，将 $\phi$ 指令加入 `value_status`；

然后继续遍历 `isLocalVarOp` 的指令

- 若为 load 指令：`new_value` 为该左值对应的最新的右值（由 `value_status` 得），然后替换所有使用该 load 右值处为 `new_value`；
- 若为 store 指令：`value_status` 插入该 store 指令的右值；

- 无论是 load 还是 store 最后都加入 `delete_list` 中；

接着遍历**后继基本块**，遍历每条 $\phi$ 指令，`new_value` 为该 $\phi$ 指令左值对应的最新的值，然后将 `new_value` 和**本基本块**加入该条 $\phi$ 指令的操作数中，即填写 $\phi$ 指令的参数。

然后对每个**没访问过的后继基本块**，进行 `valueForwarding`。

然后对与该基本块中定值的左值，将其最新定值进行 pop；最后将 `delete_list` 中的指令都删除。

##### removeAlloc

删去所有的 int/float/pointer 的 `alloca` 指令。

##### 总结

总体上 `Mem2Reg` 优化遍流程如下，对于每个函数：

- 进行基本块内优化，删去了块内可被优化的 store/load 指令；
- 在合适的地方插入 $\phi$ 函数；
- 记录每个基本块中定值的左值列表（下面重命名用）；
- 进行重命名，并删除不必要的 store/load 指令，即非全局非数组指针的 store/load 都被删去；
- 删除不必要的 `alloca` 指令；

#### B2-2

实际上在 B2-1 中对每个函数的详细分析已经能够回答本题了，以下结合例子，再针对问题逐一解答。

##### 处理

首先用于测试的 `test.sy` 如下，方便起见，只有一个函数。

```c
int a;
int b;

int main() {
	b = 1;
	a = 3;
    int x[10];
    int c = 2, d = 4;
    c = a;
    d = b;
    while(c > 0) {
        if (d < 5) {
		    d = d + c * c;
        } else {
		    d = d + c;
        }
        c = c - 1;
	}
	return c;
}
```

初始未经任何优化产生的 IR 如下。

```asm
@a = global i32 zeroinitializer
@b = global i32 zeroinitializer
define i32 @main() {
label_entry:
  %op0 = alloca i32
  store i32 1, i32* @b
  store i32 3, i32* @a
  %op1 = alloca [10 x i32]
  %op2 = alloca i32
  store i32 2, i32* %op2
  %op3 = alloca i32
  store i32 4, i32* %op3
  %op4 = load i32, i32* @a
  store i32 %op4, i32* %op2
  %op5 = load i32, i32* @b
  store i32 %op5, i32* %op3
  br label %label7
label_ret:                                                ; preds = %label17
  %op6 = load i32, i32* %op0
  ret i32 %op6
label7:                                                ; preds = %label_entry, %label29
  %op8 = load i32, i32* %op2
  %op9 = icmp sgt i32 %op8, 0
  %op10 = zext i1 %op9 to i32
  %op11 = icmp ne i32 %op10, 0
  br i1 %op11, label %label12, label %label17
label12:                                                ; preds = %label7
  %op13 = load i32, i32* %op3
  %op14 = icmp slt i32 %op13, 5
  %op15 = zext i1 %op14 to i32
  %op16 = icmp ne i32 %op15, 0
  br i1 %op16, label %label19, label %label25
label17:                                                ; preds = %label7
  %op18 = load i32, i32* %op2
  store i32 %op18, i32* %op0
  br label %label_ret
label19:                                                ; preds = %label12
  %op20 = load i32, i32* %op3
  %op21 = load i32, i32* %op2
  %op22 = load i32, i32* %op2
  %op23 = mul i32 %op21, %op22
  %op24 = add i32 %op20, %op23
  store i32 %op24, i32* %op3
  br label %label29
label25:                                                ; preds = %label12
  %op26 = load i32, i32* %op3
  %op27 = load i32, i32* %op2
  %op28 = add i32 %op26, %op27
  store i32 %op28, i32* %op3
  br label %label29
label29:                                                ; preds = %label19, %label25
  %op30 = load i32, i32* %op2
  %op31 = sub i32 %op30, 1
  store i32 %op31, i32* %op2
  br label %label7
}
```

首先是 `insideBlockForwarding`，删去了块内可被优化的 store/load 指令；在本例子中具体表现为删去了

```asm
store i32 2, i32* %op2
store i32 4, i32* %op3
```

因为这两个个定值随后被

```asm
store i32 %op4, i32* %op2
store i32 %op5, i32* %op3
```

所注销。

再是 `genPhi`，在合适的地方插入 Phi 函数，接着 `valueDefineCounting`，记录每个基本块中定值的左值列表。

这里列举出本阶段定值的左值列表如下

```asm
label_entry
op2 op3

label7
op3 op2

label17
op0

label19
op3

label25
op3

label29
op3 op2
```

然后 `valueForwarding(func_->get_entry_block())`，进行重命名，并删除不必要的 store/load 指令，即非全局非数组指针的 store/load 都被删去，这里列举出重命名后的 $\phi$ 指令如

```asm
label7:                                                ; preds = %label_entry, %label29
  %op32 = phi i32 [ %op5, %label_entry ], [ %op34, %label29 ]
  %op33 = phi i32 [ %op4, %label_entry ], [ %op31, %label29 ]

label29:                                                ; preds = %label19, %label25
  %op34 = phi i32 [ %op28, %label25 ], [ %op24, %label19 ]
```

同时本阶段被删除的 load/store 指令有

```asm
label_ret
  %op6 = load i32, i32* %op0

label17
  %op18 = load i32, i32* %op2
  store i32 %op33, i32* %op0

label29
  %op30 = load i32, i32* %op2
  store i32 %op31, i32* %op2

label25
  %op26 = load i32, i32* %op3
  %op27 = load i32, i32* %op2
  store i32 %op28, i32* %op3

label19
  %op20 = load i32, i32* %op3
  %op21 = load i32, i32* %op2
  %op22 = load i32, i32* %op2
  store i32 %op24, i32* %op3

label12
  %op13 = load i32, i32* %op3

label7
  %op8 = load i32, i32* %op2

label_entry
  store i32 %op4, i32* %op2
  store i32 %op5, i32* %op3
```

最后 `removeAlloc`， 删除不必要的 `alloca` 指令如下

```asm
%op2 = alloca i32
%op3 = alloca i32
%op0 = alloca i32
```

中间过程中各种替换过于繁杂，就不一一列举了，最终优化后的中间代码如下。

```asm
@a = global i32 zeroinitializer
@b = global i32 zeroinitializer
define i32 @main() {
label_entry:
  store i32 1, i32* @b
  store i32 3, i32* @a
  %op1 = alloca [10 x i32]
  %op4 = load i32, i32* @a
  %op5 = load i32, i32* @b
  br label %label7
label_ret:                                                ; preds = %label17
  ret i32 %op33
label7:                                                ; preds = %label_entry, %label29
  %op32 = phi i32 [ %op5, %label_entry ], [ %op34, %label29 ]
  %op33 = phi i32 [ %op4, %label_entry ], [ %op31, %label29 ]
  %op9 = icmp sgt i32 %op33, 0
  %op10 = zext i1 %op9 to i32
  %op11 = icmp ne i32 %op10, 0
  br i1 %op11, label %label12, label %label17
label12:                                                ; preds = %label7
  %op14 = icmp slt i32 %op32, 5
  %op15 = zext i1 %op14 to i32
  %op16 = icmp ne i32 %op15, 0
  br i1 %op16, label %label19, label %label25
label17:                                                ; preds = %label7
  br label %label_ret
label19:                                                ; preds = %label12
  %op23 = mul i32 %op33, %op33
  %op24 = add i32 %op32, %op23
  br label %label29
label25:                                                ; preds = %label12
  %op28 = add i32 %op32, %op33
  br label %label29
label29:                                                ; preds = %label19, %label25
  %op34 = phi i32 [ %op28, %label25 ], [ %op24, %label19 ]
  %op31 = sub i32 %op33, 1
  br label %label7
}
```

##### 问题回答

Q：`Mem2Reg`可能会删除的指令类型是哪些？对哪些分配(alloca)指令会有影响？

A：可能删除的是满足 `isLocalVarOp` 条件的指令，即左值为非全局变量且非数组指针的 store/load 指令。还会删除 int/float/pointer 的 `alloca` 指令。通过阅读源代码以及比较上面的例子可以发现如针对全局变量的 store/load 以及数组的 `alloca` 都被保留。

```asm
  store i32 1, i32* @b
  store i32 3, i32* @a
  %op1 = alloca [10 x i32]
  %op4 = load i32, i32* @a
  %op5 = load i32, i32* @b
```

Q：在基本块内前进`insideBlockForwarding`时，对store指令处理时为什么`rvalue`在`forward_list`中存在时，就需要将`rvalue`替换成`forward_list`映射中的`->second`值？

A：因为 `forward_list` 存储 **load 指令**到**右值**的映射，此 load 指令的左值在本基本块已被定值，而映射的是对此左值定值的右值。所以 `rvalue` 在 `forward_list` 中存在表明原来的 load 指令可以删去，`rvalue` 应该被映射的右值替换。

Q：在基本块内前进时，`defined_list`代表什么含义？

A：`defined_list` 存储**左值**到**对该左值定值的(store)指令**的映射；

Q：生成 phi 指令`genPhi`的第一步两层 for 循环在收集什么信息，这些信息在后面的循环中如何被利用生成 Phi 指令？

A：收集 `globals`(存储 load 指令的 `lvalue`，由于经过 `insideBlockForwarding`，此时的 load 都是跨基本块的) 和 `defined_in_block`(存储 `lvalue` 到其被定义的 `BasicBlock` 的映射)信息。

- `globals` 用来维护需要生成 $\phi$ 指令的变量列表，对应于 pdf 中 `for each name x in Globals` 中的 `Globals`
- `defined_in_block` 相当于维护 pdf 中的 Blocks，`defined_in_block.find(var)->second;` 包含所有定义 `var` 的基本块，这被用来初始化 `queue`（对应 pdf 中的 Worklist）；

Q：`valueDefineCounting`为`defined_var`记录了什么信息

A：记录 `BasicBlock` 到该基本块中定值的左值列表的映射，该列表包括 $\phi$ 和 store 指令定值的左值。

Q：`valueForwarding`在遍历基本块时采用的什么方式

A：首先是进入入口基本块，然后每次进入一个基本块就标记已访问，然后递归地访问还未被标记的**后继基本块**完成遍历。

Q：`valueForwarding`中为什么`value_status`需要对 $\phi$ 指令做信息收集

A：`value_status` 实际上相当于 pdf 中的 Stack，记录最新的值信息，而 $\phi$ 指令也会对左值进行定值(或者说赋予了新名字)，因此 $\phi$ 指令也需要记录；

Q：`valueForwarding`中第二个循环对load指令的替换是什么含义

A：因为这条 load 指令实际上不必要，`value_status` 中已经有该左值最新的右值信息，因此可以用该右值去**替换**所有用到此 load 右值的地方，这样就可以删去此 load 指令。

Q：`valueForwarding`中出现的`defined_var`和`value_status`插入条目之间有什么联系

A：`defined_var` 存储的是该基本块定义的左值，而 `value_status` 中有这些左值对应的最新的右值信息。

#### B2-3

例子要求包含至少两层由条件分支、循环组成的嵌套结构，并且对同一个变量有在不同分支或者迭代中的定值和引用。

以上要求能体现 `Mem2Reg` 的两处效果 (这些都可以通过先前的例子体现)：

- 一是 `valueForwarding` 中能删去不必要的 store/load 指令，因为对一个变量的多处定值或引用会有许多冗余的 store/load；
- 二是能体现出增加了 $\phi$ 指令，有两层嵌套，且在不同分支有定值或引用，在不同分支汇合的基本块中就会需要产生 $\phi$ 指令。

### 活跃变量分析

原来的整体算法：

```Python
IN[Exit] = φ
for (every block B):
    IN[B] = φ
changed = True
while changed:
    changed = False
    for (every block B except Exit):
        caculate OUT[B]
        caculate IN[B] # includes caculate use_B & def_B
```

#### B3-1

##### execute 流程设计

- 以当前 module 的所有 function 为基本单位进行分析；
- 寻找当前 function 的 $\text{EXIT}$ 基本块（联系 B1-6）；
- 对所有基本块计算 $\text{IN}$ 和 $\text{OUT}$ 集合；
  - **在计算 $\text{IN}[B]$ 集合时**，已经拿到了当前 $\text{OUT}[B]$，$\text{OUT}[B]$ 中有一些变量（记为 $def$）是先定值后引用的，那么在 $def$ 的定值之前，$def$ 是死的，所以计算 $\text{IN}[B]$ 时需要删除；有一些变量（记为 $use$）是先引用后定值的，$use$ 不在 $\text{OUT}[B]$ 中（因为后定值杀死了 $Y$），所以计算 $\text{IN}[B]$ 时并上 $use$。这一做法和教材上的分析是一致的，反映到数据流方程上就是 $\text{IN}(B)=use_B\cup(\text{OUT}[B]-def_B)$。
  - **在计算 $\text{OUT}[B]$ 集合时**，除教材上的分析外，还需考虑到其后继可能含有若干 $\phi$ 函数。例如，在 $B$ 的某后继 $S$ 中有 $\phi$ 函数 $\phi=[op_1,A],[op_2,B]$（不考虑别名），若 $op_2$ 出现在 $\text{OUT}[B]$ 中，则在 $B$ 的出口必须将其置为活跃。反映到数据流方程上，若记后继 $S$ 中关于 $B$ 的 $\phi$ 变量引用为 $\Phi[S,B]$，则 $\text{OUT}[B]=\cup_{S\in succ(B)}(\text{IN}[S]\cup\Phi[S,B])$。
- 对所有的基本块的一次计算为一次迭代，计算 $\text{IN}$ 集合时自动维护变量 `changed` 以示迭代计算是否结束。迭代计算结束后，调用 `BasicBlock` 的两个接口 `set_live_in` 和 `set_live_out`，将计算结果保存。

##### 代码细节

`ActiveVar` 类中的 `execute` 函数会被调用来执行活跃变量分析算法。该函数用到的数据结构如下：

- `std::map<BasicBlock*, std::set<Value*>>` 保存 $\text{IN}/\text{OUT}$ 集合
- `std::set<Value*>` 保存计算 $\text{IN}$ 集合过程中计算的 $use/def$ 集合

根据算法，首先需要寻找 $\text{EXIT}$ 基本块，这里使用 B1-6 中的方法：

```C
for (auto bb : f->get_basic_blocks()) {
	auto terminate_instr = bb->get_terminator();
	if (terminate_instr->is_ret()){
		exit_block = bb;
		break;
	}
}
```

也就是选取以 ret 指令结尾的块作为 $\text{EXIT}$ 节点。之后，进入迭代计算循环，对所有的基本块的一次计算为一次迭代。

对每个基本块计算时，先计算 $\text{OUT}$ 集合。

- 利用 CFG 的接口 `bb->get_succ_basic_block()` 可以直接获取一个基本块 $B$ 的 CFG 后继；
- 在操作每个后继 $S$ 时，先将 $\text{IN}[S]$ 插入 $\text{OUT}[B]$ 中，再遍历 $S$ 的指令（$\phi$ 指令都在块的开头），将 $\phi$ 指令中关联 $B$ 的变量插入 $\text{OUT}[B]$ 中。使用 $\phi$ 指令的 `get_operands()` 接口获取操作数，这些操作数是以 $var_1,block_1,var_2,block_2,...$ 的顺序排布的，需要将操作数看成一个个 $var-block$ 对进行处理。

然后是计算 $\text{IN}$ 集合。

- 计算 $\text{IN}$ 集合时自动维护变量 `changed` 以示迭代计算是否结束；
- 维护 $use,def$：按顺序遍历每条指令，通过查看操作数是否在 $def$ 中可以知道在本次引用前是否有定值；
- 按数据流方程计算 $\text{IN}(B)=use_B\cup(\text{OUT}[B]-def_B)$ 时，因为本次迭代与之前无关，所以需要将上一次迭代的 $\text{IN}[B]$ 清除，再构造新的 $\text{IN}[B]$。

迭代计算结束后，遍历所有的基本块，调用 `BasicBlock` 的两个接口 `set_live_in` 和 `set_live_out`，将计算结果保存。

### 检查器

#### B4-1

以下根据 `Check.h` 中定义的关键函数来介绍检查的内容

```cpp
void valueDefineCounting(Function *fun);
void checkFunction(Function *fun);
void checkBasicBlock(BasicBlock *bb);
void checkPhiInstruction(Instruction *inst);
void checkCallInstruction(Instruction *inst);
void checkInstruction(Instruction *inst);
void checkUse(User *user);
```

##### execute

- 首先检查**全局变量**的使用是否符合约定(具体在 `checkUse`)；

- 然后就是检查当前**模块**的每个函数是否符合约定。

##### valueDefineCounting

辅助函数，用于收集当前函数定义的所有定值。可以理解为收集 `%op == ... ` 的 `%op`。

##### checkFunction

- 首先检查函数是否存在于当前模块，即检查上下文；
- 其次检查函数**返回值的类型**是否合法；
- 接着检查函数的**入口基本块**是否没有**前驱**；
- 然后对每个基本块做检查。

##### checkBasicBlock

- 首先，同样检查上下文，当前基本块是否位于函数内；
- 其次，检查是否以终结语句结尾；
- 接着，收集**开头**的 $\phi$ 指令，检查每一条 $\phi$ 指令是否符合约定；
- 然后，检查后面的指令，特别地，如果此时发现 $\phi$ 指令也是不合约定的，因为 $\phi$ 指令应该出现在每个基本块开头；此外，在检查指令之前，确认该条指令的上下文是否**为本基本块**，例如可能存在将指令从该基本块中删除/或加入，但是指令的 parent 没有及时修改的情况。
- 最后，检查基本块的前驱后继关系，即当前基本块是否为其后继基本块的前驱。

##### checkPhiInstruction

- 首先，将 $\phi$ 指令普通指令检查；
- 其次，检查 $\phi$ 指令来的路径是否为所有前驱(查资料时发现 LLVM IR 有此条约定，但是实际上，有些路径并没用，最后打印时会发现有 `undef`，因此本条实际没必要，暂时注释)
- 接着，检查 $\phi$ 指令来的路径(即前驱基本块)是否确实包含有该操作数；
- 然后，检查其操作数中的基本块是否唯一，即不可能从同一基本块有多个值到达；
- 最后，检查每一个操作数中每一个值的类型是否相同。

##### checkInstruction

- 首先，检查上下文，是否位于基本块中；
- 其次，检查使用定值的地方是否符合约定，见 `checkUse`；
- 接着，检查每一操作数是否已有定值，例如，形如 `%op0 = %op1 ...` 右侧出现的 `%op1` 应该有定值。这一条可以防止删除某个值的的时候忘记对使用的地方进行修正；
- 若操作数为函数/基本块，同样需要检查是否已有定义，防止删除的时候忘修改；
- 若为 `call` 指令，还需要进一步检查；
- 若为 `br` 指令，检查跳转目标是否在后继基本块里，防止修改 `succ_bb` 和 `prev_bb` 产生问题；
- 若为 `ret` 指令，需检查返回类型是否匹配。

##### checkCallInstruction

- 首先，检查函数的返回类型和调用的返回类型是否一致；
- 其次，检查形参和实参个数是否匹配；
- 接着，检查每一个参数的类型是否匹配。

##### checkUse

- 检查每一个使用该全局变量/定值的地方是否为合法的指令，主要为检查 `use_list` 是否有异常修改。
- 检查使用该全局变量/定值的指令的指定操作数是否为该全局变量/定值。

> 所有检查都已列举在上面，包括灵感来自自己编写的优化遍，就不再在 opt.md 中额外补充了

最后在每一遍优化之后加入检查遍，来保证每一遍优化之后的 `IR` 依然符合约定。

这里默认初始生成的 `IR` 没有问题(一开始通过对初始 `IR` 的检查来反馈我们的检查遍是否合理)。

#### 花絮

虽然前面一句说「默认初始生成的 `IR` 没有问题」，但本次实验中 `check.cpp` 成功协助我们找到了两处代码框架的 bug。

第一处是 [User.cpp](../src/SysYFIR/User.cpp) 的 `remove_operands` 函数。

```cpp
void User::remove_operands(int index1,int index2){
    for(int i=index1;i<=index2;i++){
        operands_[i]->remove_use(this);
    }
    operands_.erase(operands_.begin()+index1,operands_.begin()+index2+1);
    // std::cout<<operands_.size()<<std::endl;
    num_ops_=operands_.size();
}
```

这里的问题在于，假设对于一个 4 个操作数的 phi 指令：`%op = phi i32 [ %op1, %label1 ], [ %op2, %label2 ]`，我们删除第 0 到 1 个操作数，此时会发现原先的第 2 到 3 个操作数变成了第 0 到 1 个操作数，但它们的 `use.arg_no_` 仍然是原来的 2 和 3，并没有及时更新。

为了解决这一问题，我们需要将 index2 下标之后的操作数也进行维护：

```cpp
void User::remove_operands(int index1,int index2){
    // index2 之后的也要删，但是要加回去
    auto backup = std::vector<Value*>{};
    for (int i = index2 + 1; i < num_ops_; i++) {
        backup.push_back(operands_[i]);
    }
    for(int i = index1; i < num_ops_; i++){
        operands_[i]->remove_use(this);
    }
    operands_.erase(operands_.begin() + index1,operands_.begin() + num_ops_);
    // std::cout<<operands_.size()<<std::endl;
    num_ops_=operands_.size();
    for (auto* op: backup) {
        add_operand(op);
    }
}
```

第二处是 [IRBuilder.cpp](../src/SysYFIRBuilder/IRBuilder.cpp)。在创建 alloca 指令时，需要注意，如果当前不是起始基本块，则需要将新创建指令的 parent 设置为起始基本块。该文件中有三个这样的 alloca 语句，第一句设置了，但其他两句遗漏了。

```cpp
var = builder->create_alloca(array_type);
cur_fun_cur_block->get_instructions().pop_back();
cur_fun_entry_block->add_instruction(dynamic_cast<Instruction *>(var));
// 设置 parent
dynamic_cast<Instruction *>(var)->set_parent(cur_fun_entry_block);
```

### 其他

[Mem2Reg.cpp](../src/Optimize/Mem2Reg.cpp) 中的 insideBlockForwarding 函数有些令人费解，以下是一个重构后的版本。

```cpp
void Mem2Reg::insideBlockForwarding() {
    for (auto *bb : func_->get_basic_blocks()) {
        const auto insts = std::vector<Instruction *>{
            bb->get_instructions().begin(), bb->get_instructions().end()};

        std::map<Value *, StoreInst *> defined_list;
        for (auto *inst : insts) {
            if (!isLocalVarOp(inst))
                continue;

            if (inst->get_instr_type() == Instruction::OpID::store) {
                Value *lvalue = static_cast<StoreInst *>(inst)->get_lval();
                auto *forwarded = defined_list[lvalue];
                if (forwarded != nullptr)
                    bb->delete_instr(forwarded);
                defined_list[lvalue] = static_cast<StoreInst *>(inst);
            } else if (inst->get_instr_type() == Instruction::OpID::load) {
                Value *lvalue = static_cast<LoadInst *>(inst)->get_lval();
                auto *forwarded = defined_list[lvalue];
                if (forwarded == nullptr)
                    continue;

                Value *value = forwarded->get_rval();
                for (auto use : inst->get_use_list()) {
                    auto *user = static_cast<User *>(use.val_);
                    user->set_operand(use.arg_no_, value);
                }
                bb->delete_instr(inst);
            }
        }
    }
}
```

只有 `defined_list` 是真正的核心，需要维护。