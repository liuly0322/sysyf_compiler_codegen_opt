## 分析与优化Pass说明

### Pass

`Pass`类是所有程序分析与优化Pass的基类（比如Mem2Reg、DominateTree、ComSubExprEli等）。定义在`include/Optimize/Pass.h`中。

该基类中有两个纯虚函数需要子类重写：一个是`virtual void execute() = 0`，该函数是pass的总控函数，该函数会被调用去执行该pass；另一个是`virtual const std::string get_name() const = 0`，该函数用于获得该pass的名字。

该基类中有一个成员变量`Module* module`，被子类继承。这里`module`含义表示一个编译单元，是一个源程序文件对应的中间表示。

本实验中，你所实现的分析和优化pass均需继承自该类。你需要重写上述的`execute`函数和`get_name`函数。可以参考给出的`Mem2Reg`和`DominateTree`是如何继承Pass类并实现对应功能的。

### PassMgr

`PassMgr`类负责管理Pass。定义在`include/Optimize/Pass.h`中。该类有两个私有成员变量:`module`和`pass_list`。`module`含义同上,`pass_list`为当前添加的所有pass的list。该类有两个公有的函数：`addPass`和`execute`。`addPass`函数用于向`pass_list`中添加pass，每次添加采用`push_back`的方式。`execute`函数用于顺序调用`pass_list`中pass的`execute`函数(即上述需要重写的`execute`函数)。`PassMgr`的使用实例可参见`src/main.cpp`。

本实验中你无需修改`Pass`类和`PassMgr`类。
