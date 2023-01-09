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
cd ../test
echo "start -av test"
python3 test.py -av
echo "start -dce test"
python3 test.py -dce
echo "start -sccp test"
python3 test.py -sccp
echo "start -av & -dce test"
python3 test.py -av -dce
echo "start -av & -sccp test"
python3 test.py -av -sccp
echo "start -dce & -sccp test"
python3 test.py -dce -sccp
echo "start -av & -dce & -sccp test"
python3 test.py -av -dce -sccp
echo -e "all tests finished\n"
