#!/usr/bin/env python3
# 这是一个用于ARM评测的生成ARM汇编代码的脚本框架，仅供参考，请根据需求自行修改
# 该脚本运行在希冀评测平台上
import subprocess
import os

if __name__ == '__main__':
    # 通过已有优化，编译程序得到汇编代码的命令，格式为 './compiler {编译选项} {输入程序文件路径} -o {输出汇编文件路径}'
    baseline_asm_gen_cmd = './compiler {} {} -o {}'
    # 加上你的优化，编译程序得到汇编代码的命令，格式为 './compiler {编译选项} {输入程序文件路径} -o {输出汇编文件路径}'
    optimized_asm_gen_cmd = './compiler {} {} -o {}'

    # 修改当前的python的工作目录
    os.chdir('../../build')
    # 收集所有的`../test/student`目录下的测试样例
    source_file_list = [os.path.join('../test/student', file) if file.endswith('.sy') for file in os.listdir('../test/student')]
    # 执行编译生成汇编
    # 注意生成的汇编代码一定要放在`../test/student`目录下，只有该目录将被打包发送到树莓派上进行可执行程序生成和实际运行
    subprocess.run(baseline_asm_gen_cmd.format('', '', '../test/student/demo_baseline.s'), shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    subprocess.run(optimized_asm_gen_cmd.format('', '', '../test/student/demo_opt.s'), shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)