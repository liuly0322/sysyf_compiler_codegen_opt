#include <CodeGen.h>
#include <RegAlloc.h>
#include <string>

/*
    Code for allocating spaces for the varibles in the function (specified by parameter)
        - parameter `fun`: pointer to the function to be handled
    
    You need to implement this function, according to the stack frame layout stated in the documentation
    
    Here are some tips:
        - Be careful with the order of the arguments to be passed through stack. They are pushed in reversed order,
          which means that a parameter in front will be closer to the stack pointer. 
          For example, the fourth parameter will be closer to the stack pointer than the fifth parameter.
        - Don't forget to reserve the space for callee-saved registers. The number of callee-saved registers
          can be obtained with `used_reg.second.size()`. 
        - ...
*/

int CodeGen::stack_space_allocation(Function *fun)
{
    int size = 0;

    // std::map<Value *, Interval *> CodeGen::reg_map
    auto _reg_map = &reg_map;   // Hint: use this to get register for values

    // std::map<Value *, IR2asm::Regbase *> CodeGen::stack_map
    stack_map.clear();          // You need to fill in this container to finish allocation

    // std::vector<IR2asm::Regbase *> CodeGen::arg_on_stack
    arg_on_stack.clear();       // You need to maintain this information, the order is the same as parameter

    /* TODO：put your code here */
    
    return size;
}