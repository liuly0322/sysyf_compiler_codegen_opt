# 汇编代码生成相关数据结构接口

你需要了解的代码生成相关的数据结构定义在`{WorkSpace}/include/CodeGen/ValueGen.h`内，它包含汇编代码中的值的描述，包括值`IR2asm::Value`类和位置`IR2asm::Location`类两个总基类。

## IR2asm::Value
这个基类下面的子类主要用于算数类指令等使用到的值，包括常数（立即数）、寄存器，以及Arm的“第二操作数” (Operand 2)，它们实现了`is_const()`以及`is_reg()`方法，同时可以用`get_code()`调用获得它在指令中的打印格式。本实验并不过多涉及这一部分。

## IR2asm::Location
这个基类下面是表示位置的数据结构，所有子类同样使用`get_code()`调用获得它在指令中的打印格式。它有以下子类：
- `IR2asm::label`为代码中的标签，用于标记地址，通常用于跳转以及全局数组地址等。
- `IR2asm::RegLoc`表示的是立即数和寄存器位置，因为它们的处理常常类似所以放在一起，它具有`is_const`数据域表示它是常数还是寄存器，若是常数，则`const_value`域有效，表示其值，否则`reg_id`域有效，表示其寄存器号。可以用`is_constant()`，`get_constant()`, `get_reg_id()`获取这些数据域的值。同时其构造可以通过构造函数`RegLoc(int id,bool is_const_val=false)`完成，若`is_const_val`为`True`则`const_value = id`否则`reg_id = id`。
- `IR2asm::RegBase`以基址寄存器+偏移的形式保存栈上的地址，偏移具有多种形式，与ARM寻址格式相关，我们只需要使用最简单的格式，即基址寄存器加整数偏移量就可以，它的构造很直接，即`Regbase(Reg reg, int offset): reg_(reg), offset(offset){}`,比如我们要表示栈顶寄存器的位置，就使用`IR2asm::Regbase(IR2asm::sp, 0)`来创建对象就可以了。

## 其它常数
在`ValueGen.h`中的`IR2asm`命名空间中还有一些实用的常数，为了增加代码的可读性，列举如下：
```
    const int max_reg = 15;
    const int frame_ptr = 11;
    const int sp = 13;
    const int lr = 14;
    const int pc = 15;
```
