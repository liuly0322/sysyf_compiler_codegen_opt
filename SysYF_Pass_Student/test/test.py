#!/usr/bin/env python3
import subprocess
import os
import sys
import time

IRBuild_ptn = '"{}" "-emit-ir" "-o" "{}" "{}" "-O"'
ExeGen_ptn = '"clang" "{}" "-o" "{}" "{}" "../lib/lib.c"'
Exe_ptn = '"{}"'

def eval(EXE_PATH, TEST_BASE_PATH, optimization):
    global IRBuild_ptn
    print('===========TEST START===========')
    print('now in {}'.format(TEST_BASE_PATH))
    dir_succ = True
    # 一共有多少个测试样例
    testcase_num = len(testcases)
    statistic_line_count = []
    statistic_line_count_opt = []
    total_line_count = 0
    total_line_count_opt = 0
    statistic_run_time = []
    statistic_run_time_opt = []
    total_run_time = 0
    total_run_time_opt = 0
    testcase_names = []
    # 对每个测试样例
    for case in testcases:
        testcase_names.append(case)
        # print('Case %s:' % case, end='')
        TEST_PATH = TEST_BASE_PATH + case
        SY_PATH = TEST_BASE_PATH + case + '.sy'
        LL_PATH = TEST_BASE_PATH + case + '.ll'
        INPUT_PATH = TEST_BASE_PATH + case + '.in'
        OUTPUT_PATH = TEST_BASE_PATH + case + '.out'
        # 是否需要输入
        need_input = testcases[case]
        # 给IR生成命令添加优化选项，并构造新的中间文件名、输出文件名
        IRBuild_withopt = IRBuild_ptn
        LL_PATH_OPT = TEST_BASE_PATH + case
        TEST_PATH_OPT = TEST_PATH
        for opt in optimization:
            assert opt == "-dce" or opt == "-sccp" or opt == "-cse"
            IRBuild_withopt += f' {opt}'
            LL_PATH_OPT += f'_{opt[1:]}'
            TEST_PATH_OPT += f'_{opt[1:]}'
        LL_PATH_OPT += '.ll'
        # 给compiler将sy编译成ll
        IRBuild_result = subprocess.run(IRBuild_ptn.format(EXE_PATH, LL_PATH, SY_PATH), shell=True, stderr=subprocess.PIPE)
        IRBuild_result_opt = subprocess.run(IRBuild_withopt.format(EXE_PATH, LL_PATH_OPT, SY_PATH), shell=True, stderr=subprocess.PIPE)
        # 成功sy转ll
        if IRBuild_result.returncode == 0 and IRBuild_result_opt.returncode == 0:
            # 计算代码行数优化
            with open(LL_PATH, "r") as no_opt_f:
                lines = no_opt_f.read().count('\n') - 20
                statistic_line_count.append(lines)
                total_line_count += lines
            with open(LL_PATH_OPT, "r") as opt_f:
                lines = opt_f.read().count('\n') - 20
                statistic_line_count_opt.append(lines)
                total_line_count_opt += lines

            input_option = None
            # 如果需要输入
            if need_input:
                with open(INPUT_PATH, "rb") as fin:
                    input_option = fin.read()
            try:
                # 让clang编译ll文件生成exe文件
                res = subprocess.run(ExeGen_ptn.format("-O0", TEST_PATH, LL_PATH), shell=True, stderr=subprocess.PIPE)
                res_opt = subprocess.run(ExeGen_ptn.format("-O0", TEST_PATH_OPT, LL_PATH_OPT), shell=True, stderr=subprocess.PIPE)
                # clang编译失败
                if res.returncode != 0 or res_opt.returncode != 0:
                    dir_succ = False
                    print(res.stderr.decode(), end='')
                    print('\tClangExecute Fail')
                    continue
                # clang编译成功，执行生成的可执行文件，顺便记录时间

                time_before_no_opt = time.time()
                result = subprocess.run(Exe_ptn.format(TEST_PATH), shell=True, input=input_option, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                time_after_no_opt = time.time()
                time_cost_no_opt = time_after_no_opt - time_before_no_opt
                statistic_run_time.append(time_cost_no_opt)
                total_run_time += time_cost_no_opt

                time_before_opt = time.time()
                result_opt = subprocess.run(Exe_ptn.format(TEST_PATH_OPT), shell=True, input=input_option, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                time_after_opt = time.time()
                time_cost_opt = time_after_opt - time_before_opt
                statistic_run_time_opt.append(time_cost_opt)
                total_run_time_opt += time_cost_opt

                # 执行的输出
                out = result_opt.stdout.split(b'\n')
                if result_opt.returncode != b'':
                    out.append(str(result.returncode).encode())
                for i in range(len(out)-1, -1, -1):
                    # 移除\r
                    out[i] = out[i].strip(b'\r')
                    if out[i] == b'':
                        out.remove(b'')
                # 成功执行，写入输出
                case_succ = True
                # 打开输出文件
                with open(OUTPUT_PATH, "rb") as fout:
                    i = 0
                    for line in fout.readlines():
                        line = line.strip(b'\r').strip(b'\n')
                        if line == '':
                            continue
                        if out[i] != line:
                            dir_succ = False
                            case_succ = False
                            break
                        i = i + 1
                    if case_succ:
                        pass
                        # print('\tPass')
                    else:
                        print('\tWrong Answer')
            except Exception as _:
                dir_succ = False
                print(_, end='')
                print('\tCodeGen or CodeExecute Fail')
            # 这里删除了所有中间文件
            finally:
                subprocess.call(["rm", "-rf", TEST_PATH, TEST_PATH])
                subprocess.call(["rm", "-rf", TEST_PATH, TEST_PATH_OPT])
                subprocess.call(["rm", "-rf", TEST_PATH, TEST_PATH + ".o"])
                subprocess.call(["rm", "-rf", TEST_PATH, LL_PATH])
                subprocess.call(["rm", "-rf", TEST_PATH, LL_PATH_OPT])
        else:
            dir_succ = False
            print('\tIRBuild Fail')

    if dir_succ:
        # 统计优化结果
        print('-----------OPT RESULT-----------')
        print(f'{testcase_num} cases in this dir')
        effective_num = len(statistic_line_count)
        print(f'{effective_num} cases passed')
        prompt1 = 'optimization options: '
        for opt_option in optimization:
            prompt1 += opt_option + ' '
        print(prompt1)

        line_better = 0
        line_10_better = 0
        line_20_better = 0
        line_best_i = 0
        line_best = 1

        time_better = 0
        time_10_better = 0
        time_20_better = 0
        time_best_i = 0
        time_best = 1

        for i in range(effective_num):
            if statistic_line_count_opt[i] < statistic_line_count[i]:
                line_better += 1
            opt_rate = statistic_line_count_opt[i]/statistic_line_count[i]
            if opt_rate < 0.9:
                line_10_better += 1
            if opt_rate < 0.8:
                line_20_better += 1
            if opt_rate < line_best:
                line_best_i = i
                line_best = opt_rate
            
            if statistic_run_time_opt[i] < statistic_run_time[i]:
                time_better += 1
            opt_rate = statistic_run_time_opt[i]/statistic_run_time[i]
            if opt_rate < 0.9:
                time_10_better += 1
            if opt_rate < 0.8:
                time_20_better += 1
            if opt_rate < time_best:
                time_best_i = i
                time_best = opt_rate

        print(f'Line: total {total_line_count} lines to {total_line_count_opt} lines, rate: {100 - int(total_line_count_opt / total_line_count * 100)}%')
        print(f'\t{line_better} cases better than no-opt')
        print(f'\t{line_10_better} cases 10% better than no-opt')
        print(f'\t{line_20_better} cases 20% better than no-opt')
        print(f'\tbest opt-rate is {100-int(line_best*100)}%, testcase: {testcase_names[line_best_i]}')
        print('Time: total {:.2f}s to {:.2f}s, rate: {}%'.format(total_run_time, total_run_time_opt, 100 - int(total_run_time_opt / total_run_time * 100)))
        print(f'\t{time_better} cases better than no-opt')
        print(f'\t{time_10_better} cases 10% better than no-opt')
        print(f'\t{time_20_better} cases 20% better than no-opt')
        print(f'\tbest opt-rate is {100-int(time_best*100)}%, testcase: {testcase_names[time_best_i]}')
    else:
        print('Fail in dir {}'.format(TEST_BASE_PATH))

    print('============TEST END============')
    return dir_succ


if __name__ == "__main__":

    # you can only modify this to add your testcase
    TEST_DIRS = [
                './student/testcases/',
                './Test_H/Easy_H/',
                './Test_H/Medium_H/',
                './Test_H/Hard_H/',
                './Test/Easy/',
                './Test/Medium/',
                './Test/Hard/',
                './function_test2020/',
                './function_test2021/'
                ]
    # you can only modify this to add your testcase

    # 获取优化选项
    opt_options = sys.argv[1:]
    optimization = {}
    for opt in opt_options:
        if opt == "-dce":
            optimization["-dce"] = True
            continue
        if opt == "-sccp":
            optimization["-sccp"] = True
            continue
        if opt == "-cse":
            optimization["-cse"] = True
            continue

    # 失败的样例集
    fail_dirs = set()
    for TEST_BASE_PATH in TEST_DIRS:
        testcases = {}  # { name: need_input }
        # compiler地址
        EXE_PATH = os.path.abspath('../build/compiler')
        if not os.path.isfile(EXE_PATH):
            print("compiler does not exist")
            exit(1)
        # 检查样例集文件夹是否存在
        for Dir in TEST_DIRS:
            if not os.path.isdir(Dir):
                print("folder {} does not exist".format(Dir))
                exit(1)
        # map返回一个元组
        testcase_list = list(map(lambda x: x.split('.'), os.listdir(TEST_BASE_PATH)))
        testcase_list.sort()
        # 移除奇怪的testcase
        for i in range(len(testcase_list)-1, -1, -1):
            if len(testcase_list[i]) == 1:
                testcase_list.remove(testcase_list[i])
        # 初始化所有都不需要输入
        for i in range(len(testcase_list)):
            testcases[testcase_list[i][0]] = False
        # 如果有in文件则需要输入
        for i in range(len(testcase_list)):
            testcases[testcase_list[i][0]] = testcases[testcase_list[i][0]] | (testcase_list[i][1] == 'in')
        # 执行
        if not eval(EXE_PATH, TEST_BASE_PATH, optimization=optimization):
            fail_dirs.add(TEST_BASE_PATH)
    
    if len(fail_dirs) > 0:
        fail_dir_str = ''
        for Dir in fail_dirs:
            fail_dir_str += (Dir + "\t")
        print("\tTest Fail in dirs {}".format(fail_dir_str))
    else:
        print("\tAll Tests Passed")
        
