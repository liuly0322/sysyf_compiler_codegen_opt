echo "enter Optimization, current dir is:"
pwd
# write your evaluation script here, please use relative path instead of absolute path!!!

## example: compile your project
## 1. build project
cd ../../
mkdir -p build
cd build
cmake ../
make

## 2. execute and collect result
echo "start -av test"
python3 ../test/test.py -av > ../test/student/run_result.txt
echo "start -dce test"
python3 ../test/test.py -dce >> ../test/student/run_result.txt
echo "start -sccp test"
python3 ../test/test.py -sccp >> ../test/student/run_result.txt
echo "start -av & -dce test"
python3 ../test/test.py -av -dce >> ../test/student/run_result.txt
echo "start -av & -sccp test"
python3 ../test/test.py -av -sccp >> ../test/student/run_result.txt
echo "start -dce & -sccp test"
python3 ../test/test.py -dce -sccp >> ../test/student/run_result.txt
echo "start -av & -dce & -sccp test"
python3 ../test/test.py -av -dce -sccp >> ../test/student/run_result.txt
echo "all tests finished"
