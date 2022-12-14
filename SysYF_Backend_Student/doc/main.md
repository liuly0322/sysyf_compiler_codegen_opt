# 代码生成与优化

本实验中新增了`-O2`命令行参数，使用`-O2`参数将开启优化（默认不开启）。例如：
```shell
./compiler -emit-ir -O2 test.sy -o test.ll
```
同时保留了`-emit-ast`参数用于从AST复原代码，`-check`参数用于静态检查。

若要开启单项优化，`-cse`代表开启公共子表达式删除，`-cp`代表开启常量传播，`-dce`代表开启死代码删除。上述的几个flag需要搭配`-O`参数使用，否则无效。比如，若你想单独开启常量传播优化，需要使用如下命令：
```shell
./compiler -emit-ir -O -cp test.sy -o test.ll
```

**注意**：以下两条命令产生的IR是不同的:
```shell
./compiler -emit-ir -O test.sy -o test.ll
```

```shell
./compiler -emit-ir test.sy -o test.ll
```
原因在于`-O`选项会开启Mem2Reg。IR会变成SSA格式。

有关flag详情，可以参见`../src/main.cpp`。
