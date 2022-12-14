# 严禁修改该脚本
SELECT=`cat select.txt`    # 1表示选择进阶优化，2表示选择运行时和后端生成，3表示自由选择
if [ ${SELECT} -eq 1 ]; then
    cd SysYF_Pass_Student/test/student
    bash eval.sh
elif [ ${SELECT} -eq 2 ]; then
    cd SysYF_Backend_Student/test/student
    bash eval.sh
elif [ ${SELECT} -eq 3 ]; then
    cd FreeStyle/test/eval.sh
    bash eval.sh
else
    echo "What's your choice???"
fi
