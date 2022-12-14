#include<CodeGen.h>
#include<stack>
#include<string>
    
    std::string CodeGen::caller_reg_store(Function* fun,CallInst* call){
        std::string code;
        caller_saved_pos.clear();
        to_save_reg.clear();
        int arg_num = 4;
        if(!used_reg.first.empty()){
            for(int i = 0;i < arg_num;i++){
                if(used_reg.first.find(i) != used_reg.first.end()){
                    bool not_to_save = true;
                    for(auto val:reg2val[i]){
                        not_to_save = not_to_save && !reg_map[val]->covers(call);
                    }
                    if(!not_to_save){
                        caller_saved_pos[i] = to_save_reg.size() * 4;
                        to_save_reg.push_back(i);
                    }
                }
            }
        }
        
        if(used_reg.second.find(12) != used_reg.second.end()){
            bool not_to_save = true;
            for(auto val:reg2val[12]){
                not_to_save = not_to_save && !reg_map[val]->covers(call);
            }
            if(!not_to_save){
                caller_saved_pos[12] = to_save_reg.size() * 4;
                to_save_reg.push_back(12);
            }
        }
        if(!to_save_reg.empty()){
            code += IR2asm::space;
            code += "STM SP, {";
            int save_size = to_save_reg.size();
            for(int i = 0; i < save_size - 1; i++){
                code += IR2asm::Reg(to_save_reg[i]).get_code();
                code += ", ";
            }
            code += IR2asm::Reg(to_save_reg[save_size-1]).get_code();
            code += "}";
            code += IR2asm::endl;
            //code += push_regs(to_save_reg);
        }
        return code;
    }

    std::string CodeGen::caller_reg_restore(Function* fun, CallInst* call){
        std::string code = "";
        int arg_num = fun->get_num_of_args();
        if(func_param_extra_offset>0){
            code += IR2asm::space;
            code += "ADD sp, sp, #";
            code += std::to_string(func_param_extra_offset*4);
            code += IR2asm::endl;
            sp_extra_ofst -= func_param_extra_offset*4;
        }
        if(call->is_void()){
            if(!to_save_reg.empty()){
                code += IR2asm::space;
                code += "LDM sp, {";
                int pop_size = to_save_reg.size()-1;
                for(int i=0;i<pop_size;i++){
                    code += IR2asm::Reg(to_save_reg[i]).get_code();
                    code += ", ";
                }
                code += IR2asm::Reg(to_save_reg[pop_size]).get_code();
                code += "}";
                code += IR2asm::endl;
            }
            return code;
        }

        else{
            int ret_id = reg_map[call]->reg_num;
            int pop_size = caller_saved_pos.size();
            int init_id = 0;

            if(caller_saved_pos.find(0)!=caller_saved_pos.end()){
                init_id = 1;
                if((pop_size - init_id)> 0){
                    code += IR2asm::space;
                    code += "LDMIB SP, {";
                    for(int i=init_id;i<pop_size-1;i++){
                        code += IR2asm::Reg(to_save_reg[i]).get_code();
                        code += ", ";
                    }
                    code += IR2asm::Reg(to_save_reg[pop_size-1]).get_code();
                    code += "}";
                    code += IR2asm::endl;
                }
            }

            else{
                if(!to_save_reg.empty()){
                    code += IR2asm::space;
                    code += "LDM SP, {";
                    for(int i=0;i<pop_size-1;i++){
                        code += IR2asm::Reg(to_save_reg[i]).get_code();
                        code += ", ";
                    }
                    code += IR2asm::Reg(to_save_reg[pop_size-1]).get_code();
                    code += "}";
                    code += IR2asm::endl;
                }
            }
            if(ret_id!=0){
                if(ret_id > 0){
                    code += IR2asm::space;
                    code += "mov " + IR2asm::Reg(ret_id).get_code();
                    code += ", ";
                    code += IR2asm::Reg(0).get_code();
                    code += IR2asm::endl;
                }else{
                    code += IR2asm::space;
                    code += "str ";
                    code += IR2asm::Reg(0).get_code();
                    code += ", ";
                    code += stack_map[call]->get_ofst_code(sp_extra_ofst);
                    code += IR2asm::endl;
                }
            }

            if(init_id && ret_id != 0){
                code += IR2asm::space;
                code += "ldr r0, [SP]";
                code += IR2asm::endl;
            }
        }
        return code;
    }

        std::string CodeGen::arg_move(CallInst* call){
        //arg on stack in reversed sequence
        std::string regcode;
        std::string memcode;
        std::stack<Value *> push_queue;//for sequence changing
        auto fun = dynamic_cast<Function *>(call->get_operand(0));
        int i = 0;
        int num_of_arg = call->get_num_operand()-1;
        if(num_of_arg>4){
            sp_extra_ofst += (call->get_num_operand() - 1 - 4) * reg_size;
        }
        for(auto arg: call->get_operands()){
            if(dynamic_cast<Function *>(arg))continue;
            if(i < 4){
                if(dynamic_cast<ConstantInt *>(arg)){
                    regcode += IR2asm::space;
                    regcode += "ldr ";
                    regcode += IR2asm::Reg(i).get_code();
                    regcode += ", =";
                    regcode += std::to_string(dynamic_cast<ConstantInt *>(arg)->get_value());
                    regcode += IR2asm::endl;
                    i++;
                    continue;
                }
                auto reg = (reg_map).find(arg)->second->reg_num;
                IR2asm::Reg* preg;
                if(reg >= 0){
                    if(reg<=3){
                        regcode += IR2asm::safe_load(new IR2asm::Reg(i), 
                                                new IR2asm::Regbase(IR2asm::Reg(IR2asm::sp),caller_saved_pos[reg]),
                                                sp_extra_ofst, long_func);
                        i++;
                        continue;
                    }
                    else if(reg!=12){
                        regcode += IR2asm::space;
                        regcode += "mov ";
                        regcode += IR2asm::Reg(i).get_code();
                        regcode += ", ";
                        regcode += IR2asm::Reg(reg).get_code();
                        regcode += IR2asm::endl;
                        i++;
                        continue;
                    }else{
                        regcode += IR2asm::safe_load(new IR2asm::Reg(i), 
                                                new IR2asm::Regbase(IR2asm::Reg(IR2asm::sp),caller_saved_pos[12]),
                                                sp_extra_ofst, long_func);
                        i++;
                        continue;
                    }
                }
                else{
                    regcode += IR2asm::safe_load(new IR2asm::Reg(i),
                                                 stack_map.find(arg)->second,
                                                 sp_extra_ofst,
                                                 long_func);
                }
            }
            else{
                push_queue.push(arg);
            }
            i++;
        }
        if(num_of_arg>4){
            sp_extra_ofst -= (call->get_num_operand() - 1 - 4) * reg_size;
        }
        std::vector<int> to_push_regs = {};
        const int tmp_reg_id[] = {12};
        const int tmp_reg_size = 1;
        int remained_off_reg_num = 1;
        func_param_extra_offset = 0;
        while(!push_queue.empty()){
            Value* arg = push_queue.top();
            push_queue.pop();
            func_param_extra_offset ++;
            if(dynamic_cast<ConstantInt *>(arg)){
                memcode += IR2asm::space;
                memcode += "ldr ";
                memcode += IR2asm::Reg(tmp_reg_id[tmp_reg_size-remained_off_reg_num]).get_code();
                memcode += " ,=";
                memcode += std::to_string(dynamic_cast<ConstantInt *>(arg)->get_value());
                memcode += IR2asm::endl;
                to_push_regs.push_back(tmp_reg_id[tmp_reg_size-remained_off_reg_num]);
                remained_off_reg_num--;
            }else{
                auto reg = (reg_map).find(arg)->second->reg_num;
                if(reg >= 0){
                    if(reg>=4&&reg<12){
                        to_push_regs.push_back(reg);
                        remained_off_reg_num--;
                    }else{
                        memcode += IR2asm::safe_load(new IR2asm::Reg(tmp_reg_id[tmp_reg_size-remained_off_reg_num]),
                                                     new IR2asm::Regbase(IR2asm::Reg(IR2asm::sp),caller_saved_pos[reg]),
                                                     sp_extra_ofst, 
                                                     long_func);
                        to_push_regs.push_back(tmp_reg_id[tmp_reg_size-remained_off_reg_num]);
                        remained_off_reg_num--;
                    }
                }
                else{
                    auto srcaddr = stack_map.find(arg)->second;
                    memcode += IR2asm::safe_load(new IR2asm::Reg(tmp_reg_id[tmp_reg_size-remained_off_reg_num]),
                                                 srcaddr,
                                                 sp_extra_ofst,
                                                 long_func);
                    to_push_regs.push_back(tmp_reg_id[tmp_reg_size-remained_off_reg_num]);
                    remained_off_reg_num--;
                }
            }
            if(remained_off_reg_num==0){
                memcode += push_regs(to_push_regs);
                to_push_regs.clear();
                remained_off_reg_num = tmp_reg_size;
            }
        }
        if(!to_push_regs.empty()){
            memcode += push_regs(to_push_regs);
        }
        return memcode + regcode;
    }

    std::string CodeGen::callee_arg_move(Function* fun){
        std::string save_code;
        std::string code;
        std::set<int> conflict_src_reg;
        std::map<int, IR2asm::Regbase*> conflict_reg_loc;
        int size = 0;
        int arg_num = fun->get_args().size();
        if(!arg_num)return code;
        if(arg_num > 4)arg_num = 4;
        for(auto arg: fun->get_args()){
            if(reg_map.find(arg) != reg_map.end()){
                int target = reg_map[arg]->reg_num;
                if(target == arg->get_arg_no())continue;
                if(target >= 0 && target < arg_num)conflict_src_reg.insert(target);
            }
        }
        if(!conflict_src_reg.empty()){
            save_code += IR2asm::space + "STMDB SP, {";
            for(auto reg: conflict_src_reg){
                if(reg == *(conflict_src_reg.rbegin()))break;
                save_code += IR2asm::Reg(reg).get_code();
                save_code += ", ";
            }
            save_code += IR2asm::Reg(*(conflict_src_reg.rbegin())).get_code() + "}" + IR2asm::endl;
        }
        int conflict_store_size = conflict_src_reg.size() * reg_size;
        for(auto reg: conflict_src_reg){
            conflict_reg_loc.insert({reg, new IR2asm::Regbase(IR2asm::sp, size - conflict_store_size)});
            size += reg_size;
        }
        for(auto arg: fun->get_args()){
            int reg;
            if(reg_map.find(arg) != reg_map.end()){
                reg = reg_map[arg]->reg_num;
            }
            else continue;
            if(arg->get_arg_no() < 4){
                if(arg->get_arg_no() == reg)continue;
                if(reg >= 0){
                    if(conflict_src_reg.find(arg->get_arg_no()) == conflict_src_reg.end()){
                        code += IR2asm::space;
                        code += "Mov ";
                        code += IR2asm::Reg(reg).get_code();
                        code += ", ";
                        code += IR2asm::Reg(arg->get_arg_no()).get_code();
                        code += IR2asm::endl;
                    }
                    else{
                        code += IR2asm::space;
                        code += "Ldr ";
                        code += IR2asm::Reg(reg).get_code();
                        code += ", ";
                        code += conflict_reg_loc[arg->get_arg_no()]->get_code();
                        code += IR2asm::endl;
                    }
                }
                else{
                    code += IR2asm::safe_store(new IR2asm::Reg(arg->get_arg_no()),
                                               stack_map[arg],
                                               sp_extra_ofst,
                                               long_func);
                }
            }
            else{
                if(reg < 0)continue;
                code += IR2asm::safe_load(new IR2asm::Reg(reg),
                                        //   arg_on_stack[arg->get_arg_no() - 4],
                                          stack_map[arg],
                                          sp_extra_ofst,
                                          long_func);
            }
        }
        return save_code + code;
    }