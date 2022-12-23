- [必做部分](#必做部分)
  - [编译命令的flag说明](#编译命令的flag说明)
  - [分析与优化Pass说明](#分析与优化pass说明)
  - [必做-Part1](#必做-part1)
  - [必做-Part2](#必做-part2)
  - [必做-Part3](#必做-part3)
  - [必做-Part4](#必做-part4)
  - [使用Log输出程序信息](#使用log输出程序信息)
  - [报告要求](#报告要求)

# 必做部分

必做部分为优化分析部分。你需要实现若干优化和分析。

## 编译命令的flag说明

本实验中新增了`-O2`命令行参数，使用`-O2`参数将开启全部优化（默认不开启）。例如：

```shell
./compiler -emit-ir -O2 test.sy -o test.ll
```

同时保留了`-emit-ast`参数用于从AST复原代码，`-check`参数用于静态检查。

若要开启单项优化，`-av`代表开启活跃变量分析。如果需要开启优化或者分析，则必须在命令行中搭配`-O`参数使用，否则无效。比如，若你想开启活跃变量分析或者其他分析优化遍，就需要使用如下命令：

```shell
./compiler -emit-ir -O -av test.sy -o test.ll
```

**注意：**以下两条命令产生的IR是不同的:

```shell
./compiler -emit-ir -O test.sy -o test.ll
```

```shell
./compiler -emit-ir test.sy -o test.ll
```

原因在于`-O`选项会开启Mem2Reg pass，IR会变成SSA格式。

有关flag详情，可以参见[main.cpp](src/main.cpp)。

## 分析与优化Pass说明

- Pass

  `Pass`类是所有分析与优化Pass的基类（比如Mem2Reg、DominateTree、ComSubExprEli等），定义在[Pass.h](include/Optimize/Pass.h)中。该基类中有两个纯虚函数需要子类重写：一个是`virtual void execute() = 0`，该函数是pass的总控函数，该函数会被调用去执行该pass；另一个是`virtual const std::string get_name() const = 0`，该函数用于获得该pass的名字。

  该基类中有一个成员变量`Module* module`，被子类继承。这里`module`含义表示一个编译单元，是一个源程序文件对应的中间表示。

  本实验中，你所实现的分析和优化pass均需继承自该类。你需要重写上述的`execute`函数和`get_name`函数。可以参考给出的[Mem2Reg](src/Optimize/Mem2Reg.cpp)和[DominateTree](src/Optimize/DominateTree.cpp)是如何继承Pass类并实现对应功能的。

- PassMgr

  `PassMgr`类负责管理Pass。定义在[Pass.h](include/Optimize/Pass.h)中。该类有两个私有成员变量:`module`和`pass_list`。`module`含义同上,`pass_list`为当前添加的所有pass的list。该类有两个公有的函数：`addPass`和`execute`。`addPass`函数用于向`pass_list`中添加pass，每次添加采用`push_back`的方式。`execute`函数用于顺序调用`pass_list`中pass的`execute`函数(即上述需要重写的`execute`函数)。`PassMgr`的使用实例可参见`src/main.cpp`。

  本实验中你无需修改`Pass`类和`PassMgr`类。如果有修改，请在报告中说明。

- Mem2Reg

  `Mem2Reg`用于将IR转换成为SSA形式的IR。以LLVM IR为例，在生成IR时，局部变量被生成为alloca/load/store的形式。用 alloca 指令来“声明”变量，得到一个指向该变量的指针，用 store 指令来把值存在变量里，用 load 指令来把值读出。LLVM 在 mem2reg 这个 pass 中，会识别出上述这种模式的 alloca，把它提升为 SSA value，在提升为 SSA value时会对应地消除 store 与 load，修改为 SSA 的 def-use/use-def 关系，并且在适当的位置安插 Phi 和 进行变量重命名。本次实验中，助教给出了Mem2Reg的一种实现(见[Mem2Reg.cpp](src/Optimize/Mem2Reg.cpp))，在开启优化时会开启Mem2Reg，将IR转换为SSA形式的IR。因此本实验中的所有优化均基于SSA形式的IR。

## 必做-Part1

在教材 9.6.1 节中，我们学习了支配关系的概念。

为了更高效地计算支配关系，我们可以构建支配树。支配树上的每个结点的父结点都是直接支配该结点的结点，即在所有支配它的结点中距离它最近的那个（它本身除外）。

支配树主要用于计算支配边界，从而在合适的地方插入 Phi 指令，得到 SSA（详见 [Mem2Reg](src/Optimize/Mem2Reg.cpp)）。

在死代码删除优化遍中，可能会用到反向支配树，即从流图的 EXIT 结点出发，逆着控制流的方向生成的支配树。

- **任务1-1 构建支配树的算法**

  为构建支配树，采用一种基于数据流迭代计算的方法，该方法类似教材算法 9.6。

  请仔细阅读 [dom.pdf](doc/dom.pdf)，回答以下问题：

  - **B1-1**. 证明：若 $x$ 和 $y$ 支配 $b$，则要么 $x$ 支配 $y$，要么 $y$ 支配 $x$。 

  - **B1-2**. 在 [dom.pdf](doc/dom.pdf) 中，`Figure 1: The Iterative Dominator Algorithm` 是用于计算支配关系的迭代算法，该算法的内层 `for` 循环是否一定要以后序遍历的逆序进行，为什么？

  - **B1-3**. `Figure 3: The Engineered Algorithm` 为计算支配树的算法。在其上半部分的迭代计算中，内层的 `for` 循环是否一定要以后序遍历的逆序进行，为什么？

  - **B1-4**. 在 [dom.pdf](doc/dom.pdf) 中，`Figure 3: The Engineered Algorithm` 为计算支配树的算法。其中下半部分 `intersect` 的作用是什么？内层的两个 `while` 循环中的小于号能否改成大于号？为什么？

  - **B1-5**. 这种通过构建支配树从而得到支配关系的算法相比教材中算法 9.6 在时间和空间上的优点是什么？

- **任务1-2 理解支配树和反向支配树的实现**

  [DominateTree.cpp](src/Optimize/DominateTree.cpp) 和 [RDominateTree.cpp](src/Optimize/RDominateTree.cpp) 是本实验框架提供的支配树和反向支配树的一种实现，请阅读代码并回答以下问题：

  - **B1-6**. 在反向支配树的构建过程中，是怎么确定 EXIT 结点的？为什么不能以流图的最后一个基本块作为 EXIT 结点？

## 必做-Part2

在必做的第二部分，你需要理解Mem2Reg优化遍的具体含义，结合例子说明Mem2Reg是如何对LLVM IR代码进行变换的。

- **任务2-1**

  你需要仔细阅读[`Mem2Reg.h`](include/Optimize/Mem2Reg.h)和[`Mem2Reg.cpp`](src/Optimize/Mem2Reg.cpp)，参考[静态单赋值构造](doc/%E9%9D%99%E6%80%81%E5%8D%95%E8%B5%8B%E5%80%BC%E6%A0%BC%E5%BC%8F%E6%9E%84%E9%80%A0.pdf)中的算法，将`Mem2Reg`优化遍的***流程***记录在报告中，并结合例子说明在`Mem2Reg`的***哪些阶段***分别***存储了什么***，***对IR做了什么处理***。
  - 为了体现`Mem2Reg`的效果，例子需要包含至少两层由条件分支、循环组成的嵌套结构，并且对同一个变量有在不同分支或者迭代中的定值和引用。

  两层嵌套结构例如

  ```
  while {
    if {}
    else {}
  }
  ```

  - **B2-1**. 请说明`Mem2Reg`优化遍的流程。
  - **B2-2**. 结合例子说明在`Mem2Reg`的***哪些阶段***分别***存储了什么***，***对IR做了什么处理***。需要说明
    - `Mem2Reg`可能会删除的指令类型是哪些？对哪些分配(alloca)指令会有影响？
    - 在基本块内前进`insideBlockForwarding`时，对`store`指令处理时为什么`rvalue`在`forward_list`中存在时，就需要将`rvalue`替换成`forward_list`映射中的`->second`值？
    - 在基本块内前进时，`defined_list`代表什么含义？
    - 生成phi指令`genPhi`的第一步两层for循环在收集什么信息，这些信息在后面的循环中如何被利用生成Phi指令？
    - `valueDefineCounting`为`defined_var`记录了什么信息
    - `valueForwarding`在遍历基本块时采用的什么方式
    - `valueForwarding`中为什么`value_status`需要对phi指令做信息收集
    - `valueForwarding`中第二个循环对`load`指令的替换是什么含义
    - `valueForwarding`中出现的`defined_var`和`value_status`插入条目之间有什么联系
  - **B2-3**. 请说明上述例子的要求为什么能体现`Mem2Reg`的效果？

## 必做-Part3

在必做的第三部分，你需要完成活跃变量分析。

- **任务3-1**

  你需要补充[ActiveVar.h](include/Optimize/ActiveVar.h)和[ActiveVar.cpp](src/Optimize/ActiveVar.cpp)中的`ActiveVar`类，来实现一个完整的活跃变量分析算法，分析所有`BasicBlock`块的入口和出口的活跃变量，该算法基于**SSA**形式的CFG。  
  在本次实验的框架中,`BasicBlock`类实现了4个和活跃变量分析相关的函数，你需要调用它们，设置所有`BasicBlock`的入口与出口处的活跃变量，以完成实验：  

  ```cpp
  // 设置该BasicBlock入口处的活跃变量集合
  void set_live_in(std::set<Value*> in){live_in = in;}
  // 设置该BasicBlock出口处的活跃变量集合
  void set_live_out(std::set<Value*> out){live_out = out;}
  // 获取该BasicBlock入口处的活跃变量集合
  std::set<Value*>& get_live_in(){return live_in;}
  // 获取该BasicBlock出口处的活跃变量集合
  std::set<Value*>& get_live_out(){return live_out;}
  ```

  `ActiveVar`类中的`execute`函数会被调用来执行你实现的活跃变量分析算法，你需要将你的实现流程体现在该函数中。  

- **任务3-2**

  在实验报告中回答下列问题

  - **B3-1**. 请说明你实现的活跃变量分析算法的设计思路。

- **Tips**

  和上课时所讲的活跃变量分析不同，本次实验的活跃变量分析需要考虑phi指令，而数据流方程：$\rm OUT[B] =\cup_{S是B的后继}IN[S]$ 的定义蕴含着基本块S入口处活跃的变量在基本块S所有前驱的出口处都是活跃的。  
  由于`phi`指令的特殊性，例如`%0 = phi [%op1, %bb1], [%op2, %bb2]`如果使用如上数据流方程，则默认此`phi`指令同时产生了`op1`与`op2`的活跃性，而事实上只有控制流从`%bb1`传过来才有`%op1`的活跃性，从`%bb2`传过来才有`%op2`的活跃性。因此你需要对此数据流方程做一些修改。

## 必做-Part4

由于对框架代码不熟悉、对优化算法理解不深或者在编码实现时的疏忽，我们很容易在实现了某个优化Pass之后在不经意间就对原本编译器中数据结构的一些约定进行了破坏（包括如LLVM IR中不同基本块之间的前驱后继关系，指令的操作数必须有定值才能使用，基本块必须以br、ret等指令结尾，def-use链等）。虽然可以通过打印LLVM IR并执行来确定某个Pass的正确性，并且这在很大程度上可以保证经过了该Pass优化生成的IR执行是正确的，但是这只能说明当前打印得到的IR是没有问题的，不能说明在编译器中仍然保留的IR Module是符合约定的。经过一个Pass之后的坏的IR Module可能会由于不满足约定而毒害下一个原本正确的优化遍导致出错。因此必要时我们可以在代码变换优化Pass之后添加一个**检查Pass**，来验证经过一个Pass变换之后的IR Module仍然满足一些良好的基本性质

- **任务4-1**

  - **B4-1**. 基于你对LLVM IR Module的理解，在[include/Optimize/Check.h](include/Optimize/Check.h)和[src/Optimize/Check.cpp](src/Optimize/Check.cpp)中编写相应的约定验证代码，并在[main.cpp](src/main.cpp)或其他合适的位置插入相应的代码来利用你所写的检查器检查是否满足约定。将你检查的内容记录在报告中，并说明检查理由。将检查器插入的代码位置记录在报告中，说明理由。

## 使用Log输出程序信息

该部分文档见[logging.md](doc/logging.md)。

## 报告要求

请在[doc/report.md](doc/report.md)中编写对**Bx-y**的回答
