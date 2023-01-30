<div align="center">
  
# SysYF-CodeGenOpt

</div>

<p align="center">
<a href="https://github.com/liuly0322/sysyf_compiler_codegen_opt/actions/workflows/CI.yml">
  <img src="https://github.com/liuly0322/sysyf_compiler_codegen_opt/actions/workflows/CI.yml/badge.svg">
</a>
</p>

<p align="center">
<a href="https://liuly.moe/sysyf_compiler_codegen_opt/">demo 网页</a> | <a href="https://github.com/liuly0322/sysyf_compiler_codegen_opt/blob/main/SysYF%E8%AF%AD%E8%A8%80%E5%AE%9A%E4%B9%89.pdf">SysYF 语言定义</a> | <a href="https://github.com/liuly0322/sysyf_compiler_codegen_opt/blob/main/doc.md">实验文档</a>
</p>

本项目来源于 2022 年 USTC 编译原理 H 的课程实验。将 SysYF 语言代码编译到 LLVM IR，并对 IR 进行平台无关优化如下：

- 稀疏条件常量传播
- 公共子表达式消除
- 死代码消除

SysYF 语言是 C 语言的一个精简子集，没有指针，没有 for 语句。本仓库的实现仅支持一维数组。

## 实现

实验框架提供了词法分析（flex），语法分析（bison）的完整代码。在此基础上：

- [中间代码生成](./SysYF_Pass_Student/src/SysYFIRBuilder/IRBuilder.cpp) 根据语法分析得到的语法树生成 LLVM IR
- [优化 Pass](./SysYF_Pass_Student/src/Optimize/) 提高生成 IR 的性能

对实验框架做出所有修改记录如下：

- 增加优化 Pass
- 替换 IRBuilder.cpp
- 修改基本块接口，以支持直接反向支配结点的获取
- 修复 User 类的 `remove_operands` 方法不更新 `use.arg_no_` 的 bug
- 添加基本块前驱后继时去重
- 删除基本块前驱时，维护当前基本块 phi 指令
- 修复框架中所有 int 和 unsigned 比较引起的 warning
- 修复了 IR 头文件循环引用的问题
- 使用 `.clang-format` 统一代码风格

### 代码优化

#### 纯函数判别

记录每个函数是否是纯函数，如果非纯函数会对哪些全局变量 store。

这是为了服务于其他优化 Pass。

#### 稀疏条件常量传播

相比普通常量传播效果更好，考虑了分支指令的条件是常量时的处理。

#### 公共子表达式消除

在该优化 Pass 前，首先会为每条 store 指令生成一条对应的 load 指令，插入在对应 store 指令之后。该 load 指令的值已知（即 store 指令写入的值），因此可以实现 store-load 的转发，删除更多的「公共子表达式」。只需要最后把这些「虚拟」load 指令替换为已知定值即可。

对于工作在 SSA 上的公共子表达式删除而言，需要特殊讨论的只有 load 指令何时会被注销：

- 对全局变量的 load 指令可以由对该全局变量的 store 指令，或会改变该全局变量的 call 指令注销。
- 对全局数组的某个下标的 load 指令可以由对该全局数组任意下标的 store 指令，或对任意作为函数参数的数组的任意下标的 store 指令，或非纯函数调用指令注销。
- 对作为函数参数的数组的某个下标的 load 指令可以由对任意全局数组任意下标的 store 指令或任意作为函数参数的数组的任意下标的 store 指令，或非纯函数调用指令注销。
- 对局部数组的某个下标的 load 指令可以由对该局部数组任意下标的 store 指令，或以该局部数组为参数的非纯函数调用指令注销。

注：如果 store 指令的下标确定与 load 指令的下标不同，则该 store 指令不会注销 load 指令。

#### 死代码删除

首先用 Mark-Sweep 算法删除无用指令，然后再尝试简化控制流并删除不可达基本块。循环执行以上步骤，直到收敛。

## 构建

### 构建编译器

```shell
# 安装 cmake
sudo apt install build-essential cmake
# 进入工作目录
cd SysYF_Pass_Student
# 编译该 cmake 项目
mkdir build
cd build
cmake ..
# 多线程编译，如果单核环境可以去掉 -j
make -j
# 得到可执行文件 compiler
```

### 构建 WASM

```shell
# 安装 docker
sudo apt install docker
# 拉取 emscripten 环境的 docker 镜像
docker pull emscripten/emsdk
# 进入工作目录
cd SysYF_Pass_Student
# 运行 docker 容器构建
docker run \
  --rm \
  -v $(pwd):/src \
  -u $(id -u):$(id -g) \
  emscripten/emsdk \
  sh build_wasm.sh
# 得到 compiler.js 和 compiler.wasm
```

将 compiler.js 和 compiler.wasm 放置在项目目录下的 static 文件夹内即可。

## 运行

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
