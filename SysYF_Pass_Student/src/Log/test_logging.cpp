// 引入头文件
#include "logging.hpp"

void test() {
    LOG_DISABLE //  向Log栈顶push false
    LOG(DEBUG) << "won't be output";    //  因为Log栈栈顶是false，所以不会打印
    LOG_POP //  Log栈pop一个值。当前函数返回之后就会使用调用该函数时Log栈的栈顶状态了，方便在某一函数中
}

int main(){
    LOG_ENABLE  //  启用Log日志的宏，会向Log栈中push一个true
    LOG(DEBUG) << "This is DEBUG log item.";    //  根据Log栈顶状态以及环境变量LOGV判断是否要输出
    // 使用关键字LOG，括号中填入要输出的日志等级
    // 紧接着就是<<以及日志的具体信息，就跟使用std::cout一样
    LOG(INFO) << "This is INFO log item";
    test();
    LOG(WARNING) << "This is WARNING log item";
    LOG(ERROR) << "This is ERROR log item";
    LOG_POP
    return 0;
}
