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
> - `store` 指令可看作 `store rvalue lvalue`（*lvalue = rvalue）
> - `load` 指令可看做 `rvalue = load lvalue` （rvalue = *lvalue）
>
> 所以左值(lvalue) 实际上是 `alloca` 的地址，以下左值即指这些地址，而右值指地址中实际的值。

##### execute

这是 `Mem2Reg` 执行的总流程，遍历每个函数，进行如下操作

- `insideBlockForwarding`，进行基本块内优化，删去了块内可被优化的 store/load 指令；
- `genPhi`，生成 `Phi` 指令；
- `module->set_print_name`，为每条指令设置名字，在 Mem2Reg 优化遍应该并不重要，可忽略；
- `valueDefineCounting`，记录每个基本块中定值的左值列表；
- `valueForwarding(func_->get_entry_block())`，进行重命名（即 `Rename(s)`）；
- `removeAlloc`，删除 `alloca` 指令；

##### isLocalVarOp

辅助函数，只有左值为非全局变量且非数组指针的 store/load 指令才会判断为真。实际上也就是可以被优化掉的 store/load 指令。

##### insideBlockForwarding

用到的数据结构，生存周期均为一个基本块：

- `defined_list`：`map<Value, Instruction>`，存储**左值**到**对该左值定值的(`store`)指令**的映射；
- `forward_list`：`map<Instruction, Value>`，存储 **`load` 指令**到**右值**的映射。 并且此 `load` 指令的左值在本基本块已被定值，而映射的正是对此左值定值的右值。所以实际上这条 `load` 指令可以被删去，其 `rvalue` 可以被映射的右值替换；
- `new_value`：`map<Value, Value>`，存储**左值**到**(最新的)对该左值定值的右值**的映射；
- `delete_list`：`set<Instruction>`，存储定值被注销的(`store`)指令

此函数的流程为，首先遍历每个基本块中的每条可被优化的指令(`isLocalVarOp` 判断为真的指令)。

对于 `store` 指令：

- 若 `rvalue` 为 `load` 指令右值且该条 `load` 指令在 `forward_list` 中，则替换右值；
- 若 `lvalue` 已在 `defined_list` 中，则将那条 `store` 指令加入`delete_list` 中（表明那条指令的定值在本块内即已被注销），`defined_list` 中的值更新为本条指令；否则 `defined_list` 加入这条 `store` 指令；
- 若 `lvalue` 在 `new_value` 中，则更新其值，否则新增；

对于 `load` 指令：

- 若 `lvalue` 不在 `defined_list` 中，则无事发生（说明在本基本块中没有对此左值的定值，也即其跨基本块）；否则在 `new_value` 中找到对该左值定值的右值，加入 `forward_list` 中；

接下来，遍历 `forward_list`，对用到 `rvalue` 的地方进行替换，然后删除该 `load` 指令；最后删除 `delete_list` 中的 `store` 指令

**总结**：本函数删去了不必要的 `load`（对本块内定值的 `load`）和 `store`（在本块内定值被注销），对用到那些右值的地方进行了替换维护。

##### genPhi

用到的数据结构：

- `globals`：`set<Value>`，存储 `load` 指令的 `lvalue`，由于经过 `insideBlockForwarding`，此时的 `load` 都是跨基本块的；
- `defined_in_block`：`map<Value, set<BasicBlock>>`，存储 `lvalue` 到其被定义的 `BasicBlock` 的映射；
- `bb_phi_list`：`map<BasicBlock, set<Value>>`，存储 `BasicBlock` 到该基本块中需要 `Phi` 指令的 `lvalue` 集合的映射；

以上三者生存周期为单个函数，以下生存周期对应与 `globals` 中的单个 `lvalue`

- `define_bbs`：由 `defined_in_block` 维护，即 `lvalue` 在哪些 `BasicBlock` 中被定义；
- `queue`：`vector<BasicBlock>`，初始为 `define_bbs` (对应 pdf 中`WorkList <- Blocks(x)`)

此函数的流程为，首先遍历每个基本块中的每条可被优化的指令(`isLocalVarOp` 判断为真的指令)。

- 对于 `load` 指令：将 `lvalue` 加入 `globals`，因为经过 `insideBlockForwarding`，此时保留的是跨基本块的，所以加入 `globals`；

- 对于 `store` 指令：将 `lvalue` 对应的基本块加入 `defined_in_block`；

然后，遍历 `globals` 中的每个变量（对应 pdf 中 `for each name x in Globals` ），然后遍历 `queue` 中的每个基本块（对应 pdf 中 `for each block b in WorkList`）；遍历该基本块的支配边界 （对应 pdf 中`for each block d in DF(b)`），若该基本块对应的需要 `Phi` 指令的 `lvalue` 集合中没有该 `lvalue` ，则创建 `Phi` 指令，并加入，然后将此基本块入队；否则新建 Phi 范数，对应于 pdf 中

```c
if d has no Phi-function for x then 
	insert a Phi-function for x in d
    worklist <- workList U {d}  
```

**总结**：本函数主要插入 `Phi` 指令，条件是，若基本块 `b` 中有对 `x` 的定义，且其跨基本块作用，则在DF(b)(支配边界) 中的每个结点起始处都放置一个对应的 $\phi$ 函数。

##### valueDefineCounting

本函数主要维护 `define_var`，存储 `BasicBlock` 到该基本块中定值的左值列表的映射。

流程为，遍历每个基本块的每个指令

- 若为 `Phi` 指令，将左值加入；
- 若为 `store` 指令，且满足 `isLocalVarOp`，将左值加入；

##### valueForwarding

本函数实际上即为 `Rename`，进行重命名，同时还删除了不必要的 store/load 指令。

用到的数据结构：

- `value_status`：`map<Value, vector<Value>>`，存储 `lvalue` 到其对应的 `Phi` **指令**或对应的 `store` 指令的**右值**的列表的映射，实际上相当于 pdf 中的 Stack，记录最新的右值信息；
- `visited`：`set<BasicBlock>`，标记已访问过的 Block；

以上两个生存周期为全局，以下为单个基本块

- `delete_list`：`set<Instruction>`，存储待删除的 load/store 指令

此函数的流程为，首先标记本基本块已访问，遍历基本块的每条指令，将 `Phi` 指令加入 `value_status`；

然后继续遍历 `isLocalVarOp` 的指令

- 若为 `load` 指令：`new_value` 为该左值对应的最新的右值（由 `value_status` 得），然后替换所有使用该 `load` 右值处为 `new_value`；
- 若为 `store` 指令：`value_status` 插入该 `store` 指令的右值；

- 无论是 `load` 还是 `store` 最后都加入 `delete_list` 中；

接着遍历**后继基本块**，遍历每条 `Phi` 指令，`new_value` 为该 `Phi` 指令左值对应的最新的值，然后将 `new_value` 和**本基本块**加入该条 `Phi` 指令的操作数中，即设置 `Phi` 指令的参数；对应 pdf 中

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
- `valueDefineCounting`，记录每个基本块中定值的左值列表；
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

然后 `valueForwarding(func_->get_entry_block())`，进行重命名，并删除不必要的 store/load 指令，即非全局非数组指针的 store/load 都被删去，这里列举出重命名后的 `Phi` 指令如

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

Q：在基本块内前进`insideBlockForwarding`时，对`store`指令处理时为什么`rvalue`在`forward_list`中存在时，就需要将`rvalue`替换成`forward_list`映射中的`->second`值？

A：因为 `forward_list` 存储 **`load` 指令**到**右值**的映射，此 `load` 指令的左值在本基本块已被定值，而映射的是对此左值定值的右值。所以 `rvalue` 在 `forward_list` 中存在表明原来的 `load` 指令可以删去，`rvalue` 应该被映射的右值替换。

Q：在基本块内前进时，`defined_list`代表什么含义？

A：`defined_list` 存储**左值**到**对该左值定值的(`store`)指令**的映射；

Q：生成phi指令`genPhi`的第一步两层for循环在收集什么信息，这些信息在后面的循环中如何被利用生成Phi指令？

A：收集 `globals`(存储 `load` 指令的 `lvalue`，由于经过 `insideBlockForwarding`，此时的 `load` 都是跨基本块的) 和 `defined_in_block`(存储 `lvalue` 到其被定义的 `BasicBlock` 的映射)信息。

- `globals` 用来维护需要生成 `Phi` 指令的变量列表，对应于 pdf 中 `for each name x in Globals` 中的 `Globals`
- `defined_in_block` 相当于维护 pdf 中的 Blocks，`defined_in_block.find(var)->second;` 包含所有定义 `var` 的基本块，这被用来初始化 `queue`（对应 pdf 中的 Worklist）；

Q：`valueDefineCounting`为`defined_var`记录了什么信息

A：记录 `BasicBlock` 到该基本块中定值的左值列表的映射，该列表包括 `Phi` 和 `store` 指令定值的左值。

Q：`valueForwarding`在遍历基本块时采用的什么方式

A：首先是进入入口基本块，然后每次进入一个基本块就标记已访问，然后递归地访问还未被标记的**后继基本块**完成遍历。

Q：`valueForwarding`中为什么`value_status`需要对phi指令做信息收集

A：`value_status` 实际上相当于 pdf 中的 Stack，记录最新的值信息，而 `Phi` 指令也会对左值进行定值(或者说赋予了新名字)，因此 Phi 指令也需要记录；

Q：`valueForwarding`中第二个循环对`load`指令的替换是什么含义

A：因为这条 `load` 指令实际上不必要，`value_status` 中已经有该左值最新的右值信息，因此可以用该右值去**替换**所有用到此 `load` 右值的地方，这样就可以删去此 `load` 指令。

Q：`valueForwarding`中出现的`defined_var`和`value_status`插入条目之间有什么联系

A：`defined_var` 存储的是该基本块定义的左值，而 `value_status` 中有这些左值对应的最新的右值信息。

#### B2-3

> 请说明上述例子的要求为什么能体现`Mem2Reg`的效果？

例子要求包含至少两层由条件分支、循环组成的嵌套结构，并且对同一个变量有在不同分支或者迭代中的定值和引用。

以上要求能体现 `Mem2Reg` 的两处效果(这些都可以通过先前的例子体现)

- 一是 `valueForwarding` 中能删去不必要的 store/load 指令，因为对一个变量的多处定值或引用会有许多冗余的 store/load；
- 二是能体现出增加了 `Phi` 指令，有两层嵌套，且在不同分支有定值或引用，在不同分支汇合的基本块中就会需要产生 `Phi` 指令。

### 活跃变量分析

#### B3-1

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

> 所有检查都已列举在上面，包括灵感来自自己编写的优化遍，就不再在 opt.md 中额外补充了

最后在每一遍优化之后加入检查遍，来保证每一遍优化之后的 `IR` 依然符合约定。

这里默认初始生成的 `IR` 没有问题(一开始通过对初始 `IR` 的检查来反馈我们的检查遍是否合理)。

## Common Sub-expression Elimination

> 本优化遍基于 SSA 形式

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

《Advanced Compiler Design Implementation》 Steven S.Muchnick