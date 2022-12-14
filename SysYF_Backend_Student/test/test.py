#!/usr/bin/env python3
import subprocess
import os

IRBuild_ptn = '"{}" "-emit-ir" "-o" "{}" "{}" "-O2"'
ExeGen_ptn = '"clang" "{}" "-o" "{}" "{}" "../lib/lib.c"'
Exe_ptn = '"{}"'

def eval(EXE_PATH, TEST_BASE_PATH, optimization):
    print('===========TEST START===========')
    print('now in {}'.format(TEST_BASE_PATH))
    dir_succ = True
    for case in testcases:
        print('Case %s:' % case, end='')
        TEST_PATH = TEST_BASE_PATH + case
        SY_PATH = TEST_BASE_PATH + case + '.sy'
        LL_PATH = TEST_BASE_PATH + case + '.ll'
        INPUT_PATH = TEST_BASE_PATH + case + '.in'
        OUTPUT_PATH = TEST_BASE_PATH + case + '.out'
        need_input = testcases[case]

        IRBuild_result = subprocess.run(IRBuild_ptn.format(EXE_PATH, LL_PATH, SY_PATH), shell=True, stderr=subprocess.PIPE)
        if IRBuild_result.returncode == 0:
            input_option = None
            if need_input:
                with open(INPUT_PATH, "rb") as fin:
                    input_option = fin.read()

            try:
                res = subprocess.run(ExeGen_ptn.format(optimization, TEST_PATH, LL_PATH), shell=True, stderr=subprocess.PIPE)
                if res.returncode != 0:
                    dir_succ = False
                    print(res.stderr.decode(), end='')
                    print('\t\033[31mClangExecute Fail\033[0m')
                    continue
                result = subprocess.run(Exe_ptn.format(TEST_PATH), shell=True, input=input_option, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                out = result.stdout.split(b'\n')
                if result.returncode != b'':
                    out.append(str(result.returncode).encode())
                for i in range(len(out)-1, -1, -1):
                    out[i] = out[i].strip(b'\r')
                    if out[i] == b'':
                        out.remove(b'')
                case_succ = True
                with open(OUTPUT_PATH, "rb") as fout:
                    i = 0
                    for line in fout.readlines():
                        line = line.strip(b'\r').strip(b'\n')
                        if line == '':
                            continue
                        if out[i] != line:
                            dir_succ = False
                            case_succ = False
                        i = i + 1
                    if case_succ:
                        print('\t\033[32mPass\033[0m')
                    else:
                        print('\t\033[31mWrong Answer\033[0m')
            except Exception as _:
                dir_succ = False
                print(_, end='')
                print('\t\033[31mCodeGen or CodeExecute Fail\033[0m')
            finally:
                subprocess.call(["rm", "-rf", TEST_PATH, TEST_PATH])
                subprocess.call(["rm", "-rf", TEST_PATH, TEST_PATH + ".o"])
                subprocess.call(["rm", "-rf", TEST_PATH, TEST_PATH + ".ll"])

        else:
            dir_succ = False
            print('\t\033[31mIRBuild Fail\033[0m')
    if dir_succ:
        print('\t\033[32mSuccess\033[0m in dir {}'.format(TEST_BASE_PATH))
    else:
        print('\t\033[31mFail\033[0m in dir {}'.format(TEST_BASE_PATH))

    print('============TEST END============')
    return dir_succ


if __name__ == "__main__":

    # you can only modify this to add your testcase
    TEST_DIRS = [
                './Test_H/Easy_H/',
                './Test_H/Medium_H/',
                './Test_H//Hard_H/',
                './Test/Easy/',
                './Test/Medium/',
                './Test/Hard/'
                ]
    # you can only modify this to add your testcase

    optimization = "-O0"     # -O0 -O1 -O2 -O3 -O4 -Ofast
    fail_dirs = set()
    for TEST_BASE_PATH in TEST_DIRS:
        testcases = {}  # { name: need_input }
        EXE_PATH = os.path.abspath('../build/compiler')
        if not os.path.isfile(EXE_PATH):
            print("compiler does not exist")
            exit(1)
        for Dir in TEST_DIRS:
            if not os.path.isdir(Dir):
                print("folder {} does not exist".format(Dir))
                exit(1)
        testcase_list = list(map(lambda x: x.split('.'), os.listdir(TEST_BASE_PATH)))
        testcase_list.sort()
        for i in range(len(testcase_list)-1, -1, -1):
            if len(testcase_list[i]) == 1:
                testcase_list.remove(testcase_list[i])
        for i in range(len(testcase_list)):
            testcases[testcase_list[i][0]] = False
        for i in range(len(testcase_list)):
            testcases[testcase_list[i][0]] = testcases[testcase_list[i][0]] | (testcase_list[i][1] == 'in')
        if not eval(EXE_PATH, TEST_BASE_PATH, optimization=optimization):
            fail_dirs.add(TEST_BASE_PATH)
    if len(fail_dirs) > 0:
        fail_dir_str = ''
        for Dir in fail_dirs:
            fail_dir_str += (Dir + "\t")
        print("\t\033[31mTest Fail\033[0m in dirs {}".format(fail_dir_str))
    else:
        print("\t\033[32mAll Tests Passed\033[0m")
        
