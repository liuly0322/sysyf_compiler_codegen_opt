<div align="center">
  
# SysYF-CodeGenOpt

</div>

<p align="center">

  <a href="https://github.com/liuly0322/sysyf_compiler_codegen_opt/actions/workflows/opt.yml">
    <img src="https://github.com/liuly0322/sysyf_compiler_codegen_opt/actions/workflows/opt.yml/badge.svg">
  </a>

</p>

## 简介

SysYF 语言是在 2020、2021 年全国大学生计算机系统能力大赛编译系统设计赛要求实现的 SysY 语言基础上增加了 float 类型和元素类型为 float 的数组类型（[SysYF 语言定义](SysYF语言定义.pdf)）。

本仓库是 2022 年中国科学技术大学编译原理 H 课程的实验项目，在提供的框架代码（已提供词法分析，语法分析，语义分析）基础上，主要完成了对中间代码（IR）的平台无关优化如下：

- 稀疏条件常量传播
- 公共子表达式消除
- 死代码消除

使用 WASM 构建了一个 [效果展示网页](https://liuly.moe/sysyf_compiler_codegen_opt/)。如果对 WASM 感兴趣可以参考本仓库的 GitHub Actions 以及 CMakeLists.txt。

具体实现上：

- 区分纯函数，并保存纯函数具体改变的全局变量
  - 公共子表达式消除和死代码消除可以进行更详细的讨论
- 稀疏条件常量传播相比普通常量传播效果更好，考虑了不可达控制流
- 公共子表达式消除
  - 分类考虑了 load 指令
    - 全局变量及局部数组可以由改变它的 call/store 指令注销
    - 全局数组及数组参数可以由任意非纯函数调用指令或任意非局部数组的 store 指令注销（因为指针来源未知）
  - 考虑了 store-load 转发，每条 store 指令首先生成一个配套的 load 指令（代表 store 定值已知），再进行公共子表达式消除
- 死代码删除循环执行直到收敛
  - 支持控制流简化
  - 支持不可达基本块删除

使用 _GitHub Actions_ 进行自动测试及网页更新，使用 _ClangFormat_ 格式化项目代码风格。

原先的实验文档见 [实验文档](./doc.md)。

除了增添的优化 Pass 外，另对框架做出如下修改：

- 对框架接口做出修改，以支持直接反向支配结点的获取
- 修复 `remove_operands` 不更新 `use.arg_no_` 的 bug
- 在维护基本块前驱后继时：
  - 对于添加操作，会首先判断前驱或后继链表中是否已存在需要添加的基本块
  - 对于删除前驱操作，会首先移除来自该前驱的 phi 指令信息
- 修复框架中所有 int 和 unsigned 比较引起的 warning
- 修复了 IR 头文件循环引用的问题

此外，使用了自己的 IRBuilder 替换了框架的：

- 减少全局变量使用
- 代码从 900+ 行减少到 700+ 行
- 所有测试样例编译（仅开启 Mem2Reg）出的 IR 总行数从 15274 行减少到 13922 行
  - 原框架存在 if 和 while 条件语句类型转换时的冗余
  - O2 优化后，中间代码总行数几乎一致，说明优化 Pass 运作良好

## 环境搭建

以 ubuntu 平台为例：

```shell
# 安装 clang, cmake, python3
sudo apt install build-essential clang cmake python3
# 进入项目目录
cd SysYF_Pass_Student
# 编译该 cmake 项目
mkdir build
cd build
cmake ..
# 多线程编译，如果单核环境可以去掉 -j
make -j
# 编译完成，生成可执行文件 compiler
```

## 运行说明

可以通过如下方式运行编译器:

```
./compiler [.sy文件路径] [参数] -o [生成文件路径]
```

参数主要如下：

- **-h** 或 **--help**: 打印提示信息
- **-p** 或 **--trace_parsing**: 打印文法扫描过程
- **-s** 或 **--trace_scanning**: 打印词法扫描过程
- **-emit-ast**: 从语法树复原代码
- **-emit-ir**: 生成 IR
- **-check**: 类型检查
- **-O**: 将 IR 转化为 SSA 格式
- **-O -cse**: 开启公共子表达式删除优化
- **-O -sccp**: 开启稀疏条件常量传播优化
- **-O -dce**: 开启死代码删除优化
- **-O2**: 开启全部优化

例如:

```
./compiler test.sy -emit-ir -O -cse -o test.ll
```

表示对 `test.sy` 分析，生成 `IR` 并执行公共子表达式删除优化，结果保存在 `test.ll` 文件中。

## 测试

`SysYF_Pass_Student/test/test.py` 提供了检验正确性和测试优化效果的脚本。

例如，在 `SysYF_Pass_Student/test` 工作目录下，以下命令会检查三个优化全部开启后的优化效果：

`python test.py -sccp -cse -dce`
