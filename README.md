<div align="center">
  
# SysYF-CodeGenOpt

</div>

<p align="center">

  <a href="https://github.com/liuly0322/sysyf_compiler_codegen_opt/actions/workflows/opt.yml">
    <img src="https://github.com/liuly0322/sysyf_compiler_codegen_opt/actions/workflows/opt.yml/badge.svg">
  </a>

</p>

## 简介

SysYF 语言是在 2020、2021 年全国大学生计算机系统能力大赛编译系统设计赛要求实现的 SysY 语言基础上增加了 float 类型和元素类型为 float 的数组类型。本仓库是 2022 年中国科学技术大学编译原理 H 课程的实验项目，主要完成了对中间代码（IR）的平台无关优化如下：

- 稀疏条件常量传播
- 公共子表达式消除
- 死代码消除

原先的实验文档见 [实验文档](./doc.md)。

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

你可以通过如下方式运行编译器:

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

例如，在 `SysYF_Pass_Student/test` 工作目录下，以下命令会检查三个优化全部开启后的优化效果。

`python test.py -sccp -cse -dce`
