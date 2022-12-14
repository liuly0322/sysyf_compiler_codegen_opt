#include<CodeGen.h>
#include<RegAlloc.h>
#include<queue>
#include<stack>
#include<algorithm>
#include<sstream>
#include<string>
    
    std::string CodeGen::callee_reg_store(Function* fun){
        std::string code;
        if(used_reg.second.empty())return IR2asm::space + "push {lr}" + IR2asm::endl;
        code += IR2asm::space;
        code += "push {";
        for(auto reg: used_reg.second){
            if(reg <= max_func_reg)continue;
            code += IR2asm::reg_name[reg];
            if(reg == *used_reg.second.rbegin())break;
            code += ", ";
        }
        /******always save lr for temporary use********/
        code += ", lr}";
        code += IR2asm::endl;
        return code;
    }

    std::string CodeGen::callee_reg_restore(Function* fun){
        std::string code;
        if(!used_reg.second.size())return IR2asm::space + "pop {lr}" + IR2asm::endl;
        code += IR2asm::space;
        code += "pop {";
        for(auto reg: used_reg.second){
            if(reg <= max_func_reg)continue;
            code += IR2asm::reg_name[reg];
            if(reg == *used_reg.second.rbegin())break;
            code += ", ";
        }
        /******always save lr for temporary use********/
        code += ", lr}";
        code += IR2asm::endl;
        return code;
    }

    std::string CodeGen::callee_stack_operation_in(Function* fun, int stack_size){
        int remain_stack_size = stack_size;
        std::string code;
        code += IR2asm::space;
        if(have_func_call){
            code += "mov ";
            code += IR2asm::Reg(IR2asm::frame_ptr).get_code();
            code += ", sp";
            code += IR2asm::endl;
            code += IR2asm::space;
        }
        if(remain_stack_size <= 127 && remain_stack_size > -128){
            code += "sub sp, sp, #";
            code += std::to_string(remain_stack_size);
            code += IR2asm::endl;
        }
        else{
            code += "ldr lr, =";
            code += std::to_string(remain_stack_size);
            code += IR2asm::endl;
            code += IR2asm::space;
            code += "sub sp, sp, lr";
            code += IR2asm::endl;
        }
        return code;
    }

    std::string CodeGen::callee_stack_operation_out(Function* fun, int stack_size){
        std::string code;
        code += IR2asm::space;
        if(have_func_call){
            code += "mov sp, ";
            code += IR2asm::Reg(IR2asm::frame_ptr).get_code();
            // code += std::to_string(2 * int_size);
            code += IR2asm::endl;
            return code;
        }
        int remain_stack_size = stack_size;
        if(remain_stack_size <= 127 && remain_stack_size > -128){
            code += "add sp, sp, #";
            code += std::to_string(remain_stack_size);
            code += IR2asm::endl;
        }
        else{
            code += "ldr lr, =";
            code += std::to_string(remain_stack_size);
            code += IR2asm::endl;
            code += IR2asm::space;
            code += "add sp, sp, lr";
            code += IR2asm::endl;
        }
        return code;
    }
