#!/usr/bin/env python3
# 这是一个用于ARM评测的编译ARM汇编代码并实际执行的脚本框架，仅供参考，请根据需求自行修改；尤其是实际汇编、链接过程，
# 该脚本运行在树莓派平台上
import subprocess
import os

if __name__ == '__main__':
    # 已有优化得到的汇编代码的路径列表
    baseline_asm_list = []
    # baseline汇编对应的可执行程序路径列表
    baseline_exe_list = []
    # 加上你的优化得到的汇编代码的路径列表
    optimized_asm_list = []
    # 优化后汇编对应的可执行程序路径列表
    optimized_exe_list = []

    # 使用gcc编译上述所有汇编程序，得到可执行程序, 参照格式为 'gcc -march=armv7-a -g sylib.c {汇编程序路径} -o {输出可执行程序路径}'
    # 注意这里可能用到输入输出运行时文件，请放在该文件夹里
    gcc_compile_cmd = 'gcc -march=armv7-a -g sylib.c {} -o {}'
    for asm_file, exe_file in zip(baseline_asm_list, baseline_exe_list):
        subprocess.run(gcc_compile_cmd.format(asm_file, exe_file), shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    for asm_file, exe_file in zip(optimized_asm_list, optimized_exe_list):
        subprocess.run(gcc_compile_cmd.format(asm_file, exe_file), shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # 执行可执行程序，参照格式为 'cat  {输入文件名} | {可执行程序} > {输出文件路径}'
    # 注意所有的输入输出样例也一定要放在该目录下
    exe_cmd = 'cat {} | {} > {}'
    for baseline_exe, opt_exe in zip(baseline_exe_list, optimized_exe_list):
        result1 = subprocess.run(exe_cmd.format('', baseline_exe, ''), shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        result2 = subprocess.run(exe_cmd.format('', opt_exe, ''), shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # 解析你的输出结果，输出优化前后的对比效果
    # parse()
    