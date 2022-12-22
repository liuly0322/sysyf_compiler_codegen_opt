echo "enter Backend, current dir is:"
pwd
# 在此处编写你的测试脚本，注意使用相对路径，千万不要使用绝对路径

## 示例：对`test/student`目录下的所有sy文件编译生成汇编代码
## 1. 希冀平台上的自动评测脚本会自动编译你的程序，用如下命令，你无须重复编译
# mkdir -p build
# cd build
# cmake ../
# make
# cd ../test/student

## 2. 调用你的编译器，并对所有你提供的测试样例进行编译得到汇编代码
python3 build_asm.py 


