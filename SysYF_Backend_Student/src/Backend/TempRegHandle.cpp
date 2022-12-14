#include<CodeGen.h>
#include<RegAlloc.h>
#include<queue>
#include<stack>
#include<algorithm>
#include<sstream>
#include<string>


    std::string CodeGen::tmp_reg_restore(Instruction *inst) {
        std::string code;
        for(auto pair:tmp_regs_loc){
            code += IR2asm::safe_load(new IR2asm::Reg(pair.first),
                                      pair.second,
                                      sp_extra_ofst,
                                      long_func);
        }
        tmp_regs_loc.clear();
        cur_tmp_regs.clear();
        free_tmp_pos.clear();
        free_tmp_pos = all_free_tmp_pos;
        return code;
    }

    std::string CodeGen::push_tmp_instr_regs(Instruction *inst) {
        std::string code;
        store_list.clear();
        to_store_set.clear();
        interval_set.clear();
        std::set<Value*> to_ld_set = {};
        std::set<int> used_tmp_regs = {};
        std::set<int> inst_reg_num_set = {};
        std::set<int> opr_reg_num_set = {};
        bool can_use_inst_reg = false;
        for (auto opr : inst->get_operands()) {
            if(dynamic_cast<Constant*>(opr) ||
            dynamic_cast<BasicBlock *>(opr) ||
            dynamic_cast<GlobalVariable *>(opr) ||
            dynamic_cast<AllocaInst *>(opr)){
                continue;
            }
            if (reg_map[opr]->reg_num >= 0) {
                inst_reg_num_set.insert(reg_map[opr]->reg_num);
                opr_reg_num_set.insert(reg_map[opr]->reg_num);
            }
        }
        if(!inst->is_void() && !dynamic_cast<AllocaInst *>(inst)){
            auto reg_inter = reg_map[inst];
            if(reg_inter->reg_num<0){
                if(!cur_tmp_regs.empty()){
                    reg_inter->reg_num = *cur_tmp_regs.begin();
                    cur_tmp_regs.erase(reg_inter->reg_num);
                    used_tmp_regs.insert(reg_inter->reg_num);
                }
                else{
                    if(!opr_reg_num_set.empty()){
                        for(int i = 0;i <= 12;i++){
                            if(i == 11){
                                continue;
                            }
                            if(opr_reg_num_set.find(i) == opr_reg_num_set.end()){
                                reg_inter->reg_num = i;
                                auto it = std::find(store_list.begin(),store_list.end(),i);
                                if(it==store_list.end()){
                                    store_list.push_back(i);
                                }
                                break;
                            }
                        }
                    }
                    else{
                        reg_inter->reg_num = store_list.size();
                        auto it = std::find(store_list.begin(),store_list.end(),reg_inter->reg_num);
                        if(it==store_list.end()){
                            store_list.push_back(reg_inter->reg_num);
                        }
                    }
                }
                interval_set.insert(reg_inter);
                to_store_set.insert(inst);
                can_use_inst_reg = true;
            }
            inst_reg_num_set.insert(reg_inter->reg_num);
        }
        for(auto opr:inst->get_operands()){
            if(dynamic_cast<Constant*>(opr) ||
            dynamic_cast<BasicBlock *>(opr) ||
            dynamic_cast<GlobalVariable *>(opr) ||
            dynamic_cast<AllocaInst *>(opr)){
                continue;
            }
            auto reg_inter = reg_map[opr];
            if(reg_inter->reg_num<0){
                if(can_use_inst_reg){
                    can_use_inst_reg = false;
                    if(opr_reg_num_set.find(reg_map[inst]->reg_num)==opr_reg_num_set.end()){
                        reg_inter->reg_num = reg_map[inst]->reg_num;
                        inst_reg_num_set.insert(reg_inter->reg_num);
                    }
                    else{
                        for(int i = 0;i <= 12;i++){
                            if(i==11){
                                continue;
                            }
                            if(inst_reg_num_set.find(i)==inst_reg_num_set.end()&&used_tmp_regs.find(i)==used_tmp_regs.end()){
                                reg_inter->reg_num = i;
                                inst_reg_num_set.insert(i);
                                auto it = std::find(store_list.begin(),store_list.end(),i);
                                if(it==store_list.end()){
                                    store_list.push_back(i);
                                }
                                break;
                            }
                        }
                    }
                }
                else{
                    if(!cur_tmp_regs.empty()){
                        reg_inter->reg_num = *cur_tmp_regs.begin();
                        cur_tmp_regs.erase(reg_inter->reg_num);
                        used_tmp_regs.insert(reg_inter->reg_num);
                        inst_reg_num_set.insert(reg_inter->reg_num);
                    }
                    else{
                        for(int i = 0;i <= 12;i++){
                            if(i==11){
                                continue;
                            }
                            if(inst_reg_num_set.find(i)==inst_reg_num_set.end()&&used_tmp_regs.find(i)==used_tmp_regs.end()){
                                reg_inter->reg_num = i;
                                inst_reg_num_set.insert(i);
                                auto it = std::find(store_list.begin(),store_list.end(),i);
                                if(it==store_list.end()){
                                    store_list.push_back(i);
                                }
                                break;
                            }
                        }
                    }
                }
                interval_set.insert(reg_inter);
                to_ld_set.insert(opr);
            }
        }
        int tmp_reg_base = have_func_call?20:0;
        for(auto reg_id:store_list){
            if(free_tmp_pos.empty()){
                std::cerr << "free pos not empty, something was wrong with inst: "<< inst->print() << std::endl;
                exit(15);
            }
            int free_pos = *free_tmp_pos.begin();
            free_tmp_pos.erase(free_pos);
            int cur_ofset = free_pos;
            auto loc = new IR2asm::Regbase(IR2asm::Reg(13),cur_ofset*4+tmp_reg_base);
            tmp_regs_loc[reg_id] = loc;
            code += IR2asm::safe_store(new IR2asm::Reg(reg_id),
                                       loc,
                                       sp_extra_ofst,
                                       long_func);
            cur_tmp_regs.insert(reg_id);
        }
        for(auto tmp_reg:used_tmp_regs){
            cur_tmp_regs.insert(tmp_reg);
        }
        for(auto opr:to_ld_set){
            code += IR2asm::safe_load(new IR2asm::Reg(reg_map[opr]->reg_num),
                                          stack_map[opr],
                                          sp_extra_ofst,
                                          long_func);
        }
        return code;
    }


    std::string CodeGen::pop_tmp_instr_regs(Instruction *inst) {
        std::string code;
        for(auto opr:to_store_set){
            code += IR2asm::safe_store(new IR2asm::Reg(reg_map[opr]->reg_num),
                                           stack_map[opr],
                                           sp_extra_ofst,
                                           long_func);
        }

        for(auto inter:interval_set){
            inter->reg_num = -1;
        }
        to_store_set.clear();
        store_list.clear();
        interval_set.clear();
        return code;
    }

    std::string CodeGen::ld_tmp_regs(Instruction *inst) {
        std::string code;
        if(!inst->is_alloca() && !inst->is_phi()){
            std::set<int> to_del_set;
            std::set<int> to_ld_set;
            for(auto opr:inst->get_operands()){
                if(dynamic_cast<Constant *>(opr) ||
                dynamic_cast<BasicBlock *>(opr) ||
                dynamic_cast<GlobalVariable *>(opr) ||
                dynamic_cast<AllocaInst *>(opr) ||
                dynamic_cast<Function* >(opr)){
                    continue;
                }
                int opr_reg = reg_map[opr]->reg_num;
                if(opr_reg >= 0){
                    for(auto reg:cur_tmp_regs){
                        if(reg==opr_reg){
                            to_ld_set.insert(reg);
                            to_del_set.insert(reg);
                        }
                    }
                }
            }
            if(!inst->is_void()){
                int inst_reg = reg_map[inst]->reg_num;
                if(inst_reg >= 0){
                    if(cur_tmp_regs.find(inst_reg)!=cur_tmp_regs.end()){
                        to_del_set.insert(inst_reg);
                    }
                }
            }
            for(auto ld_reg:to_ld_set){
                code += IR2asm::safe_load(new IR2asm::Reg(ld_reg),
                                          tmp_regs_loc[ld_reg],
                                          sp_extra_ofst,
                                          long_func);
            }
            for(auto del_reg:to_del_set){
                auto del_pos = tmp_regs_loc[del_reg];
                int del_free_pos = del_pos->get_offset();
                if(have_func_call){
                    del_free_pos -= 20;
                }
                del_free_pos /= 4;
                free_tmp_pos.insert(del_free_pos);
                cur_tmp_regs.erase(del_reg);
                tmp_regs_loc.erase(del_reg);
            }
        }
        return code;
    }