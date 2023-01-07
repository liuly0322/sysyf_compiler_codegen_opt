# 实验报告

## 必做部分

### 支配树

#### B1-1

记入口点是 $e$，$x\text{ dom }b\Rightarrow\forall p(e,b),x\in p$，$y\text{ dom }b\Rightarrow\forall p(e,b),y\in p$。

取一条 $e,b$ 之间的路径 $p'(e,b)$，已知 $x,y\in p'$，假设 $p'$ 上所有的 $x,y$ 节点中离 $b$ 最近的是一个 $y$ 节点，$p'$ 上从这个 $y$ 节点到 $b$ 的子路（记为 $p^*$）不包含 $x$。

如果 $x$ 不支配 $y$，则存在 $e,y$ 之间的路径 $p''(e,y),x\not\in p''$。取新路 $\tilde{p}(e,b)=p''\cup p^*$，可知 $x\not \in \tilde{p}$，与 $x\text{ dom }b$ 矛盾。

反之，假设 $p'$ 上所有的 $x,y$ 节点中离 $b$ 最近的是一个 $x$ 节点，$p'$ 上从这个 $x$ 节点到 $b$ 的子路（记为 $p^*$）不包含 $y$。

如果 $y$ 不支配 $x$，则存在 $e,x$ 之间的路径 $p''(e,x),y\not\in p''$。取新路 $\tilde{p}(e,b)=p''\cup p^*$，可知 $y\not \in \tilde{p}$，与 $y\text{ dom }b$ 矛盾。

所以，如果 $x\text{ dom }b$ 且 $y\text{ dom }b$，则必有 $x\text{ dom }y$ 或 $y\text{ dom }x$。

#### B1-2

使用逆后序遍历顺序，可以保证当访问一个非 $e$ 节点（比如 $x$）时，其所有前驱都已经被访问过，这样访问之后可以保证对 $x$ 的更新是有效的。在这里的 for 循环中，我们只要保证取出节点的顺序满足上述条件即可。图 $G$ 的任何拓扑排序的逆序皆可。

#### B1-3

同上。

#### B1-4

`intersect` 是取交集，实际上因为 DOM 集合规定了顺序，交集计算只需要将索引指针不断左移即可。

不可以只改变内层两个 while 循环的小于号，因为 doms 数组记录了支配树上自下而上的信息，更新过程按照逆后序遍历的索引顺序，需要更新的索引是越来越小的，因此按照当前的框架，这里的符号一定是小于号。

#### B1-5

**时间复杂度**：教材算法和论文算法的迭代次数上界是一样的，在支配树上计算一个逆后序序列所需的时间也是一样的，即 $O(N)$。每次迭代的时间 = 计算后序序列时间 + 对每条边执行 `intersect` 操作的时间，后者时间复杂度为 $O(D)$，$D$ 是最大 DOM 集合的大小，所以平均迭代时间为 $O(N+E·D)$。教材算法没有优化数据结构，每次都需要使用一个新的集合，时间复杂度至少是 $O(D(N+E))$ 的。

**空间复杂度**：论文算法只需要维护 doms 数组，空间复杂度是 $O(N)$；教材算法对每个节点都需要维护 DOM 集合，空间复杂度是 $O(N^2)$。

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
> ---
>
> 构造 SSA，需要在 CFG 的汇合点处插入 $\phi$ 函数，并且在最简单的实现中，如果某个 Block 有 $n$ 个前驱，则在此 Block 中定义的变量的 $\phi$ 函数就要有 $n$ 个参数。
>
> - $x_m=\phi(x_a,...,x_l)$（重命名之后的）
>
> 第二步是重命名，即*相当于*给每个同名变量加下标以示区分。
>
> 但是在引入支配信息之后，$\phi$ 函数可以简化，可以去除一些不必要的引入。比如对于支配关系……
>
> - 对于只在单个 bb 中活跃的变量肯定不需要 φ 函数，所以只需要统计哪些名字是跨 bb 的，只对这些名字考虑插入 φ 函数就行了
>
> ```Python
> # 找到跨域名字的算法
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
> # 插入 φ 函数的算法
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
> def NewName(n:名字):
>     i = counter[n]
>     counter[n]++
>     stack[n].push(i)
>     return "n_i"
> 
> def Rename(b:块):
>     for φ语句 "x = φ(...)" in b:
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
>     for 跨域名字i:
>         counter[i] = 0
>         stack[i] = {}
>     Rename(entry)
> ```

#### B2-1

> **注**：ysy 已经写好了大框架，这里只是做些修改。

以下分函数来阐述 `Mem2Reg` 优化遍的流程。

> **Remark**：
>
> - store 指令可看作 `store rvalue lvalue`（*lvalue = rvalue）
> - load 指令可看做 `rvalue = load lvalue` （rvalue = *lvalue）；但是 `loadInst` 类中实际上并无 `rvalue` 成员，这里的 `rvalue` 应当看作一个临时的、匿名的变量，它是 `load lvalue` 的结果。
> - 左值 (lvalue) 实际上是 `alloca` 的地址，以下左值即指这些地址，而右值指地址中实际的值。
> - `isLocalVarOp` 是一个辅助函数，仅当指令满足条件：左值为非全局变量且非数组指针，且为 store/load 指令时返回 `true`。实际上也就是可能可以被优化掉的 store/load 指令。
>

##### execute

这是 `Mem2Reg` 执行的总流程，遍历每个函数，进行如下操作

- `insideBlockForwarding`，按指令顺序进行基本块内优化，删去了块内可被优化的 store/load 指令；
- `genPhi`，在每个基本块开头生成 $\phi$ 指令；
- `valueDefineCounting`，记录每个基本块中定值的左值列表；
- `valueForwarding(func_->get_entry_block())`，进行重命名（即 `Rename(s)`）；
- `removeAlloc`，删除 `alloca` 指令；

##### insideBlockForwarding

用到的数据结构，生存周期均为一个基本块：

- `defined_list`：`map<Value, Instruction>`，存储**左值**到**对该左值定值的（store）指令**的映射；
- `forward_list`：`map<Instruction, Value>`，存储 **load 指令**到**右值**的映射。 并且此 load 指令的左值在本基本块已被定值，而映射的正是对此左值定值的右值。所以实际上这条 load 指令可以被删去，其 `rvalue` 可以被映射的右值替换；
- `new_value`：`map<Value, Value>`，存储**左值**到**（最新的）对该左值定值的右值**的映射；
- `delete_list`：`set<Instruction>`，存储定值被注销的（store）指令。

此函数的流程为，首先遍历每个基本块中的每条可能可被优化的指令（`isLocalVarOp` 判断为真的指令）。

**对于 store 指令**：

- 若 `rvalue` 为 load 指令右值且该条 load 指令在 `forward_list` 中，则替换右值；
- 若 `lvalue` 已在 `defined_list` 中，则将这条 store 指令加入`delete_list` 中（表明这条指令的定值在本块内即已被注销），`defined_list` 中的值更新为本条指令；否则 `defined_list` 加入这条 store 指令；
- 若 `lvalue` 在 `new_value` 中，则更新其值，否则新增；

**对于 load 指令**：

- 若 `lvalue` 不在 `defined_list` 中，则无事发生（说明在本基本块中没有对此左值的定值，也即其跨基本块）；否则在 `new_value` 中找到对该左值定值的右值，加入 `forward_list` 中；

接下来，遍历 `forward_list`，对用到 `rvalue` 的地方进行替换，然后删除该 load 指令；最后删除 `delete_list` 中的 store 指令。

**总结**：本函数删去了不必要的 load（块内对 load 的左值定值过）和 store（其定值在块内被其他语句注销），对用到这些语句右值的地方进行了替换维护。

##### genPhi

用到的数据结构：

- `globals`：`set<Value>`，存储 load 指令的 `lvalue`，由于经过 `insideBlockForwarding`，此时的 load 都是跨基本块的；
- `defined_in_block`：`map<Value, set<BasicBlock>>`，存储 `lvalue` 到其被定义的 `BasicBlock` 的映射；
- `bb_phi_list`：`map<BasicBlock, set<Value>>`，存储 `BasicBlock` 到该基本块中需要 $\phi$ 指令的 `lvalue` 集合的映射。

以上三者生存周期为单个函数，下面两个数据结构的生存周期对应与 `globals` 中的单个 `lvalue`

- `define_bbs`：由 `defined_in_block` 维护，即 `lvalue` 在哪些 `BasicBlock` 中被定义；
- `queue`：`vector<BasicBlock>`，初始为 `define_bbs`。

此函数的流程为，首先遍历每个基本块中的每条可被优化的指令(`isLocalVarOp` 判断为真的指令)。

- 对于 load 指令：将 `lvalue` 加入 `globals`，因为经过 `insideBlockForwarding`，此时保留的是跨基本块的，所以加入 `globals`；

- 对于 store 指令：将 `lvalue` 对应的基本块加入 `defined_in_block`；

然后，遍历 `globals` 中的每个变量，然后遍历 `queue` 中的每个基本块；遍历该基本块的支配边界 ，若该基本块对应的需要 $\phi$ 指令的 `lvalue` 集合中没有该 `lvalue` ，则创建 $\phi$ 指令，并加入，然后将此基本块入队；否则新建 $\phi$ 函数。

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

以上两个生存周期为全局，以下为单个基本块：

- `delete_list`：`set<Instruction>`，存储待删除的 load/store 指令

此函数的流程为，首先标记本基本块已访问，遍历基本块的每条指令，将 $\phi$ 指令加入 `value_status`；

然后继续遍历 `isLocalVarOp` 的指令：

- **若为 load 指令**：`new_value` 为该左值对应的最新的右值（由 `value_status` 得），然后替换所有使用该 load 右值处为 `new_value`；
- **若为 store 指令**：`value_status` 插入该 store 指令的右值；

- 无论是 load 还是 store 最后都加入 `delete_list` 中；

接着遍历**后继基本块**，遍历每条 $\phi$ 指令，`new_value` 为该 $\phi$ 指令左值对应的最新的值，然后将 `new_value` 和**本基本块**加入该条 $\phi$ 指令的操作数中，即填写 $\phi$ 指令的参数。

然后对每个**没访问过的后继基本块**，进行 `valueForwarding`。

然后对与该基本块中定值的左值，将其最新定值进行 pop；最后将 `delete_list` 中的指令都删除。

##### removeAlloc

删除所有的 int/float/pointer 的 `alloca` 指令。

##### 总结

总体上 `Mem2Reg` 优化遍流程如下，对于每个函数：

- 首先进行基本块内优化，删去了块内可被优化的 store/load 指令；
- 其次，在合适的地方插入 $\phi$ 函数；
- 接着记录每个基本块中定值的左值列表（下面重命名用）；
- 进行重命名，并删除不必要的 store/load 指令，即非全局非数组指针的 store/load 都被删去；
- 删除不必要的 `alloca` 指令。

#### B2-2

#### B2-3

### 活跃变量分析

> --- **Block** B
>
> IN[B]
>
> - $use_B$：引用先于定值的变量
> - $def_B$：定值先于引用的变量
>
> OUT[B]
>
> --- **End** B

计算 IN[B] 时已经拿到了 OUT[B]，OUT[B] 中有一些变量（记为 $X$）是先定值后引用的，那么在 $X$ 的定值之前，$X$ 是死的，所以计算 IN[B] 时需要删除。

有一些变量（记为 $Y$）是先引用后定值的，$Y$ 不在 OUT[B] 中（因为后定值杀死了 $Y$），所以计算 IN[B] 时并上 $Y$。

而在计算 OUT[B] 时，其后继可能含有若干 $\phi$ 函数（例如 $\phi(x_1,...,x_n)$），若某个变量 $x_j(1\le j\le n)$ 出现在了 B 中，则在 B 的出口必须将其置为活跃。

注意，这里 $\phi$ 是 $\phi([x_1,B_1],...)$ 这样的形式，要分别对不同的前驱设置不同的额外活跃性。

- $\text{IN}(B)=use_B\cup(\text{OUT}[B]-def_B)$
- $\text{OUT}[B]=\cup_{S\in succ(B)}(\text{IN}[S]\cup\Phi[S,B])$

不错，现在开始看看原来的整体算法

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

活跃变量分析的 execute 环节是按如下流程设计的：

- 以当前 module 的所有 function 为基本单位进行分析；
- 寻找当前 function 的 $\text{EXIT}$ 基本块（联系 B1-6）；
- 对所有基本块计算 $\text{IN}$ 和 $\text{OUT}$ 集合；
  - **在计算 $\text{IN}[B]$ 集合时**，已经拿到了当前 $\text{OUT}[B]$，$\text{OUT}[B]$ 中有一些变量（记为 $def$）是先定值后引用的，那么在 $def$ 的定值之前，$def$ 是死的，所以计算 $\text{IN}[B]$ 时需要删除；有一些变量（记为 $use$）是先引用后定值的，$use$ 不在 $\text{OUT}[B]$ 中（因为后定值杀死了 $Y$），所以计算 $\text{IN}[B]$ 时并上 $use$。这一做法和教材上的分析是一致的，反映到数据流方程上就是 $\text{IN}(B)=use_B\cup(\text{OUT}[B]-def_B)$。
  - **在计算 $\text{OUT}[B]$ 集合时**，除教材上的分析外，还需考虑到其后继可能含有若干 $\phi$ 函数。例如，在 $B$ 的某后继 $S$ 中有 $\phi$ 函数 $\phi=[op_1,A],[op_2,B]$（不考虑别名），若 $op_2$ 出现在 $\text{OUT}[B]$ 中，则在 $B$ 的出口必须将其置为活跃。反映到数据流方程上，若记后继 $S$ 中关于 $B$ 的 $\phi$ 变量引用为 $\Phi[S,B]$，则 $\text{OUT}[B]=\cup_{S\in succ(B)}(\text{IN}[S]\cup\Phi[S,B])$。
- 对所有的基本块的一次计算为一次迭代，计算 $\text{IN}$ 集合时自动维护变量 `changed` 以示迭代计算是否结束。迭代计算结束后，调用 `BasicBlock` 的两个接口 `set_live_in` 和 `set_live_out`，将计算结果保存。

### 检查器

#### B4-1
