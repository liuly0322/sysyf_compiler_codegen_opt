# 进阶优化提交报告

## 优化1

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

## 新增源代码文件说明

## 评测脚本使用方式

