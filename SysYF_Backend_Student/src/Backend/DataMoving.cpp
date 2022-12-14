#include<CodeGen.h>
#include<string>

/*
    code for moving data from src list to dst list:
        - parameter `src` contains data sources, they can be constant (immediate),
          register or address on stack (address specified by base register and offset)
        - parameter `dst` contains data destinations, they can be register 
          or address on stack (address specified by base register and offset)
        - parameter `cmpop` is used to generate instructions with conditional excution method
    
    you should generate code for data moving, the method for moving single data from a source location (or const)
    to a destination location is given, parameters:
        - `src_loc` is data source location, can be constant (immediate),
          register or address on stack (address specified by base register and offset)
        - parameter `dst` contains data destinations, they can be register 
          or address on stack (address specified by base register and offset)
        - parameter `reg_tmp` is the temporary register used for data moving under situations like
            - moving data from stack location to another (this function doesn't distinguish if the location is the same)
            - moving constant value (immediate) to a stack location
            - ...
        - parameter `cmpop` is used to generate instructions with conditional excution method
    the function may generate more than one instruction.
*/

    std::string CodeGen::data_move(std::vector<IR2asm::Location*> &src,
                                   std::vector<IR2asm::Location*> &dst,
                                   std::string cmpop){
        std::string code;
        
        /* TODO: put your code here */
        
        return code;
    }

    std::string CodeGen::single_data_move(IR2asm::Location* src_loc,
                                 IR2asm::Location* target_loc,
                                 IR2asm::Reg *reg_tmp,
                                 std::string cmpop){
        std::string code;
        if(dynamic_cast<IR2asm::RegLoc *>(src_loc)){
            auto regloc = dynamic_cast<IR2asm::RegLoc *>(src_loc);
            if(regloc->is_constant()){
                if(dynamic_cast<IR2asm::RegLoc*>(target_loc)){
                    auto target_reg_loc = dynamic_cast<IR2asm::RegLoc*>(target_loc);
                    code += IR2asm::space;
                    code += "Ldr" + cmpop + " ";
                    code += target_reg_loc->get_code();
                    code += ", =";
                    code += std::to_string(regloc->get_constant());
                    code += IR2asm::endl;
                }
                else{
                    code += IR2asm::space;
                    code += "Ldr" + cmpop + " ";
                    code += reg_tmp->get_code();
                    code += ", =";
                    code += std::to_string(regloc->get_constant());
                    code += IR2asm::endl;
                    code += IR2asm::safe_store(reg_tmp, target_loc, sp_extra_ofst, long_func, cmpop);
                }
            }
            else{
                if(dynamic_cast<IR2asm::RegLoc*>(target_loc)){
                    auto target_reg_loc = dynamic_cast<IR2asm::RegLoc*>(target_loc);
                    code += IR2asm::space;
                    code += "Mov" + cmpop + " ";
                    code += target_reg_loc->get_code();
                    code += ", ";
                    code += regloc->get_code();
                    code += IR2asm::endl;
                }
                else{
                    code += IR2asm::safe_store(new IR2asm::Reg(regloc->get_reg_id()),
                                                target_loc, sp_extra_ofst, long_func, cmpop);
                }
            }
        }
        else{
            auto stackloc = dynamic_cast<IR2asm::Regbase *>(src_loc);
            if(dynamic_cast<IR2asm::RegLoc*>(target_loc)){
                auto target_reg_loc = dynamic_cast<IR2asm::RegLoc*>(target_loc);
                code += IR2asm::safe_load(new IR2asm::Reg(target_reg_loc->get_reg_id()),
                                            stackloc, sp_extra_ofst, long_func, cmpop);
            }
            else{
                code += IR2asm::safe_load(reg_tmp, stackloc, sp_extra_ofst, long_func, cmpop);
                code += IR2asm::safe_store(reg_tmp, target_loc, sp_extra_ofst, long_func, cmpop);
            }
        }
        return code;
    }