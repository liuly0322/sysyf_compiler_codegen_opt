# 实验报告

## 必做部分

### 支配树

#### B1-1

> 证明：若 $x$ 和 $y$ 支配 $b$，则要么 $x$ 支配 $y$，要么 $y$ 支配 $x$。

反证：若 $x$ 和 $y$ 支配 $b$, 假设 $x$ 不支配 $y$ 且 $y$ 也不支配 $x$。

考虑一条从入口 $n_0$ 到 $b$ 的路径，由 $x$ 和 $y$ 均支配 $b$，因此 $x$ 和 $y$ 都在这条路径上，事实上这条路径总可以写成 $P(n_0,\ x) + P(x,\ b)$（其中 $P(x,\ b)$ 不含 $y$）或者 $P(n_0,\ y) + P(y,\ b)$（其中 $P(y,\ b)$ 不含 $x$）

- 由于 $x$ 不支配 $y$，因此存在一条从 $n_0$ 到 $y$ 的路径不含 $x$，记为 $P_1(n_0,\ y)$；
- 由于 $y$ 不支配 $x$，因此存在一条从 $n_0$ 到 $x$ 的路径不含 $y$；记为 $P_1(n_0,\ x)$

因此 $P(n_0,\ x) + P(x,\ b)$ 可以被替换为 $P_1(n_0,\ x) + P(x,\ b)$，为一条从 $n_0$ 到 $b$ 的路径且不含 $x$，这与 $x$ 支配 $b$ 矛盾！

(对于 $P(n_0,\ y) + P(y,\ b)$ 同理可得)

综上，若 $x$ 和 $y$ 支配 $b$，则要么 $x$ 支配 $y$，要么 $y$ 支配 $x$。

#### B1-2

> 在 [dom.pdf](doc/dom.pdf) 中，`Figure 1: The Iterative Dominator Algorithm` 是用于计算支配关系的迭代算法，该算法的内层 `for` 循环是否一定要以后序遍历的逆序进行，为什么？

此处后序遍历的逆序目是保证访问某个结点时，其所有前驱已经被访问了，这样完成更新该结点才有效。

遍历的顺序应该并不唯一，如上所述，只需要保证访问某个结点其所有前驱已被访问即可，不过后序遍历的逆序确实算是一个简单的能满足此要求的遍历顺序。

#### B1-3

> `Figure 3: The Engineered Algorithm` 为计算支配树的算法。在其上半部分的迭代计算中，内层的 `for` 循环是否一定要以后序遍历的逆序进行，为什么？

这里目的与 `B1-2` 相同，保证访问某个结点其所有前驱已被访问，因此不唯一。

#### B1-4

> 在 [dom.pdf](doc/dom.pdf) 中，`Figure 3: The Engineered Algorithm` 为计算支配树的算法。其中下半部分 `intersect` 的作用是什么？内层的两个 `while` 循环中的小于号能否改成大于号？为什么？

`intersect` 的作用是取两个 Dom 集合的交集（实际是返回一个索引）。

不能换成大于号。在上半部分的迭代计算中，内层的 `for` 循环是以后序遍历的逆序进行，节点索引也是**后序遍历的逆序**，在支配树中越**上层**的结点其索引越**大**，因此取交集的时候，是要往上层更新为直接支配结点（`doms` 数组实际维护的是 IDom 信息），也就是说索引小的才应该要变化，因此是小于号。

如果要改成大于号，则上半部分迭代的顺序和索引的构建需要相应改变。

#### B1-5

> 这种通过构建支配树从而得到支配关系的算法相比教材中算法 9.6 在时间和空间上的优点是什么？

以下简称该算法为支配树算法，并且认为教材算法 9.6 为论文中初始提出的迭代算法(未经优化)。

- 时间上，考虑两种算法的迭代次数是一样的，因此以下比较单次迭代时间复杂度。支配树算法计算反向后序序列需要 $O(N)$ 时间。计算 Dom 和 IDom 的遍历访问每个节点，对于每个节点，在每条边上进行 `intersect`，在整个遍历过程中，做 $O(E)$ 次，所需时间与它们使用的 $Dom$ 集的大小成比例。因此，每次迭代的时间为$O(N+ED)$，其中 D 是最大 $Dom$ 集的大小。而算法 9.6 取决于集合的具体实现，以并查集为例，还要考虑并集时的复制开销，时间复杂度为 $O(ND + \alpha(D)ED)$，对比可见单次迭代的时间有较大改进。

- 空间上，支配树算法只需要维护 Doms 数组，而算法 9.6 需要为每一个节点维护一个 Dom 集合，前者只需 O(N)，而后者需要 $O(ND)$。


#### B1-6

> 在反向支配树的构建过程中，是怎么确定 EXIT 结点的？为什么不能以流图的最后一个基本块作为 EXIT 结点？

在 `RDominateTree::get_revserse_post_order` 中可以发现如下代码片段确定 EXIT 结点为第一个终结指令为 ret 的基本块。（但是这样做应该是假设只有一个有 ret）

```cpp
for (auto bb: f->get_basic_blocks()) {
    auto terminate_instr = bb->get_terminator();
    if (terminate_instr->is_ret()) {
        exit_block = bb;
        break;
    }
}
```

而流图的最后一个基本块实际上未必以 ret 结尾，比如可能是跳转指令，并非实际的出口，所以不能作为 EXIT 结点。

### Mem2Reg

#### B2-1

以下分函数来阐述 `Mem2Reg` 优化遍的流程。

> **Remark**：
>
> - store 指令可看作 `store rvalue lvalue`（*lvalue = rvalue）
> - load 指令可看做 `rvalue = load lvalue` （rvalue = *lvalue）
>
> 所以左值(lvalue) 实际上是 `alloca` 的地址，以下左值即指这些地址，而右值指地址中实际的值。

##### execute

这是 `Mem2Reg` 执行的总流程，遍历每个函数，进行如下操作

- `insideBlockForwarding`，进行基本块内优化，删去了块内可被优化的 store/load 指令；
- `genPhi`，生成 $\phi$ 指令；
- `module->set_print_name`，为每条指令设置名字，在 Mem2Reg 优化遍应该并不重要，可忽略；
- `valueDefineCounting`，记录每个基本块中定值的左值列表；
- `valueForwarding(func_->get_entry_block())`，进行重命名（即 `Rename(s)`）；
- `removeAlloc`，删除 `alloca` 指令；

##### isLocalVarOp

辅助函数，只有左值为非全局变量且非数组指针的 store/load 指令才会判断为真。实际上也就是可以被优化掉的 store/load 指令。

##### insideBlockForwarding

用到的数据结构，生存周期均为一个基本块：

- `defined_list`：`map<Value, Instruction>`，存储**左值**到**对该左值定值的(store)指令**的映射；
- `forward_list`：`map<Instruction, Value>`，存储 **load 指令**到**右值**的映射。 并且此 load 指令的左值在本基本块已被定值，而映射的正是对此左值定值的右值。所以实际上这条 load 指令可以被删去，其 `rvalue` 可以被映射的右值替换；
- `new_value`：`map<Value, Value>`，存储**左值**到**(最新的)对该左值定值的右值**的映射；
- `delete_list`：`set<Instruction>`，存储定值被注销的(store)指令

此函数的流程为，首先遍历每个基本块中的每条可被优化的指令(`isLocalVarOp` 判断为真的指令)。

对于 store 指令：

- 若 `rvalue` 为 load 指令右值且该条 load 指令在 `forward_list` 中，则替换右值；
- 若 `lvalue` 已在 `defined_list` 中，则将那条 store 指令加入`delete_list` 中（表明那条指令的定值在本块内即已被注销），`defined_list` 中的值更新为本条指令；否则 `defined_list` 加入这条 store 指令；
- 若 `lvalue` 在 `new_value` 中，则更新其值，否则新增；

对于 load 指令：

- 若 `lvalue` 不在 `defined_list` 中，则无事发生（说明在本基本块中没有对此左值的定值，也即其跨基本块）；否则在 `new_value` 中找到对该左值定值的右值，加入 `forward_list` 中；

接下来，遍历 `forward_list`，对用到 `rvalue` 的地方进行替换，然后删除该 load 指令；最后删除 `delete_list` 中的 store 指令

**总结**：本函数删去了不必要的 load（对本块内定值的 load）和 store（在本块内定值被注销），对用到那些右值的地方进行了替换维护。

##### genPhi

用到的数据结构：

- `globals`：`set<Value>`，存储 load 指令的 `lvalue`，由于经过 `insideBlockForwarding`，此时的 load 都是跨基本块的；
- `defined_in_block`：`map<Value, set<BasicBlock>>`，存储 `lvalue` 到其被定义的 `BasicBlock` 的映射；
- `bb_phi_list`：`map<BasicBlock, set<Value>>`，存储 `BasicBlock` 到该基本块中需要 $\phi$ 指令的 `lvalue` 集合的映射；

以上三者生存周期为单个函数，以下生存周期对应与 `globals` 中的单个 `lvalue`

- `define_bbs`：由 `defined_in_block` 维护，即 `lvalue` 在哪些 `BasicBlock` 中被定义；
- `queue`：`vector<BasicBlock>`，初始为 `define_bbs` (对应 pdf 中`WorkList <- Blocks(x)`)

此函数的流程为，首先遍历每个基本块中的每条可被优化的指令(`isLocalVarOp` 判断为真的指令)。

- 对于 load 指令：将 `lvalue` 加入 `globals`，因为经过 `insideBlockForwarding`，此时保留的是跨基本块的，所以加入 `globals`；

- 对于 store 指令：将 `lvalue` 对应的基本块加入 `defined_in_block`；

然后，遍历 `globals` 中的每个变量（对应 pdf 中 `for each name x in Globals` ），然后遍历 `queue` 中的每个基本块（对应 pdf 中 `for each block b in WorkList`）；遍历该基本块的支配边界 （对应 pdf 中`for each block d in DF(b)`），若该基本块对应的需要 $\phi$ 指令的 `lvalue` 集合中没有该 `lvalue` ，则创建 $\phi$ 指令，并加入，然后将此基本块入队；否则新建 Phi 范数，对应于 pdf 中

```c
if d has no Phi-function for x then 
	insert a Phi-function for x in d
    worklist <- workList U {d}  
```

**总结**：本函数主要插入 $\phi$ 指令，条件是，若基本块 `b` 中有对 `x` 的定义，且其跨基本块作用，则在DF(b)(支配边界) 中的每个结点起始处都放置一个对应的 $\phi$ 函数。

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

接着遍历**后继基本块**，遍历每条 $\phi$ 指令，`new_value` 为该 $\phi$ 指令左值对应的最新的值，然后将 `new_value` 和**本基本块**加入该条 $\phi$ 指令的操作数中，即设置 $\phi$ 指令的参数；对应 pdf 中

```c
for each successor of b in the CFG 
	fill in Phi-function parameters
```

然后对每个**没访问过的后继基本块**，进行 `valueForwarding`；对应 pdf 中

```c
for each successor s of b in the dominator tree 
// 不过这里的实现应该是
// for each successor s of b in the cfg
	Rename(s)
```

然后对与该基本块中定值的左值，将其最新定值进行 pop；最后将 `delete_list` 中的指令都删除。

##### removeAlloc

本函数比较简单，就是删去所有的 int/float/pointer 的 `alloca` 指令。

##### 总结

> 还有一个 `phiStatistic` 应该是 debug 用，就不多赘述了。

总体上 `Mem2Reg` 优化遍流程如下，对于每个函数：

- 首先进行基本块内优化，删去了块内可被优化的 store/load 指令；
- 其次，在合适的地方插入 $\phi$ 函数，若基本块 `b` 中有对 `x` 的定义，且其跨基本块作用，则在DF(b)(支配边界) 中的每个结点起始处都放置一个对应的 $\phi$ 函数。
- 接着记录每个基本块中定值的左值列表（下面重命名用）；
- 然后进行重命名，并删除不必要的 store/load 指令，即非全局非数组指针的 store/load 都被删去；
- 最后删除不必要的 `alloca` 指令；

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

Q：生成phi指令`genPhi`的第一步两层for循环在收集什么信息，这些信息在后面的循环中如何被利用生成Phi指令？

A：收集 `globals`(存储 load 指令的 `lvalue`，由于经过 `insideBlockForwarding`，此时的 load 都是跨基本块的) 和 `defined_in_block`(存储 `lvalue` 到其被定义的 `BasicBlock` 的映射)信息。

- `globals` 用来维护需要生成 $\phi$ 指令的变量列表，对应于 pdf 中 `for each name x in Globals` 中的 `Globals`
- `defined_in_block` 相当于维护 pdf 中的 Blocks，`defined_in_block.find(var)->second;` 包含所有定义 `var` 的基本块，这被用来初始化 `queue`（对应 pdf 中的 Worklist）；

Q：`valueDefineCounting`为`defined_var`记录了什么信息

A：记录 `BasicBlock` 到该基本块中定值的左值列表的映射，该列表包括 $\phi$ 和 store 指令定值的左值。

Q：`valueForwarding`在遍历基本块时采用的什么方式

A：首先是进入入口基本块，然后每次进入一个基本块就标记已访问，然后递归地访问还未被标记的**后继基本块**完成遍历。

Q：`valueForwarding`中为什么`value_status`需要对phi指令做信息收集

A：`value_status` 实际上相当于 pdf 中的 Stack，记录最新的值信息，而 $\phi$ 指令也会对左值进行定值(或者说赋予了新名字)，因此 Phi 指令也需要记录；

Q：`valueForwarding`中第二个循环对load指令的替换是什么含义

A：因为这条 load 指令实际上不必要，`value_status` 中已经有该左值最新的右值信息，因此可以用该右值去**替换**所有用到此 load 右值的地方，这样就可以删去此 load 指令。

Q：`valueForwarding`中出现的`defined_var`和`value_status`插入条目之间有什么联系

A：`defined_var` 存储的是该基本块定义的左值，而 `value_status` 中有这些左值对应的最新的右值信息。

#### B2-3

> 请说明上述例子的要求为什么能体现`Mem2Reg`的效果？

例子要求包含至少两层由条件分支、循环组成的嵌套结构，并且对同一个变量有在不同分支或者迭代中的定值和引用。

以上要求能体现 `Mem2Reg` 的两处效果(这些都可以通过先前的例子体现)

- 一是 `valueForwarding` 中能删去不必要的 store/load 指令，因为对一个变量的多处定值或引用会有许多冗余的 store/load；
- 二是能体现出增加了 $\phi$ 指令，有两层嵌套，且在不同分支有定值或引用，在不同分支汇合的基本块中就会需要产生 $\phi$ 指令。

### 活跃变量分析

#### B3-1

### 检查器

#### B4-1