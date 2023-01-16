#include "Mem2Reg.h"
#include "IRBuilder.h"

void Mem2Reg::execute() {
    // 对module中所有的函数都执行这个优化遍
    for (auto fun : module->get_functions()) {
        // 如果函数没有基本块，则跳过此函数，不执行优化
        if (fun->get_basic_blocks().empty()) continue;
        func_ = fun;
        // 下面这两个是Mem2Reg的全局变量，在对新的函数进行优化之前需要清空
        lvalue_connection.clear();
        no_union_set.clear();
        // 在基本块内前进
        insideBlockForwarding();
        // 放置φ函数
        genPhi();
        // 为每个基本块设置名字
        module->set_print_name();
        // 记录定值
        valueDefineCounting();
        // 传播？是从函数的入口基本块开始的
        valueForwarding(func_->get_entry_block());
        // 移除alloca指令
        removeAlloc();
    }
}

void Mem2Reg::insideBlockForwarding() {
    // 对函数的所有基本块进行此操作
    for (auto bb : func_->get_basic_blocks()) {
        // 对某左值定值的指令列表
        std::map<Value*, Instruction*> defined_list;
        // load指令及其定值的列表
        std::map<Instruction*, Value*> forward_list;
        std::map<Value*, Value*> new_value;
        std::set<Instruction*> delete_list;

        // 对此基本块中每一条指令
        for (auto inst : bb->get_instructions()) {
            // 对非局部变量不做操作
            if (!isLocalVarOp(inst)) continue;
            // 如果是store指令：store x, ptr，这里x参数在前，但是类内部定义指定了lvalue是ptr，rvalue是x
            // 效果也就是[ptr] = x
            if (inst->get_instr_type() == Instruction::OpID::store) {
                Value* lvalue = static_cast<StoreInst*>(inst)->get_lval();
                Value* rvalue = static_cast<StoreInst*>(inst)->get_rval();
                auto load_inst = dynamic_cast<Instruction*>(rvalue);
                // 如果右值是某个load指令，且有关于此load指令的右值信息
                if (load_inst && forward_list.find(load_inst) != forward_list.end()) {
                    // 直接将指令的右值（load指令）修改为load指令的右值
                    rvalue = forward_list.find(load_inst)->second;
                }

                // 如果左值在定值列表中，也就是说有某个指令对此左值定值了
                // 更新对左值定值的指令（也就是更新为当前指令），并且把之前的定值指令插入delete_list，表示其定值已经被注销了
                // 否则，向定值列表中插入新的

                // 为什么左值如果有定值，就可以删除这条store指令？
                // 因为我们现在是以某种顺序进行操作，当前的指令 “看到” 之前的数据，定值之类的，已经被临时保存了
                // 所以这里相当于把显式的 store 转换成临时的记录，后面使用就行了
                auto defined_pair = defined_list.find(lvalue);
                if (defined_pair != defined_list.end()) {
                    delete_list.insert(defined_pair->second);// 删除旧的
                    defined_pair->second = inst;// 新增新的，不删除，后面可能还要用
                }
                else {
                    defined_list.insert({ lvalue, inst });
                }
                // new_value应该是一个收集左值和右值的映射关系的列表
                // 如果找到了，则更新右值，否则插入新的左值-右值对
                auto new_value_pair = new_value.find(lvalue);
                if (new_value_pair != new_value.end()) {
                    new_value_pair->second = rvalue;
                }
                else {
                    new_value.insert({ lvalue, rvalue });
                }
            }
            // 如果是load指令，我们希望可能通过寻找define_list中某个定值，来替换后面
            //  出现的所有load的左值，进而达到删除load指令的目的
            else if (inst->get_instr_type() == Instruction::OpID::load) {
                Value* lvalue = static_cast<LoadInst*>(inst)->get_lval();
                // 如果没有对此左值的定值指令，则跳过
                if (defined_list.find(lvalue) == defined_list.end()) continue;
                // 有对左值的定值，找到当前值，并插入forward_list中
                Value* value = new_value.find(lvalue)->second;
                forward_list.insert({ inst, value });
            }
        }
        // 对每一条指令都进行完了上述操作
        // 对指令来说好像没有特定的顺序，是由基本块的instr_list决定的

        // 对forward_list中的所有<instruction, value>对
        for (auto submap : forward_list) {
            Instruction* inst = submap.first;
            Value* value = submap.second;
            // 对当前指令的所有引用来说
            for (auto use : inst->get_use_list()) {
                Instruction* use_inst = dynamic_cast<Instruction*>(use.val_);
                // 修改指令的操作数，arg_no_指示序号，value指示值，也就是用值去替换引用
                use_inst->set_operand(use.arg_no_, value);
            }
            // 删除这条load指令
            bb->delete_instr(inst);
        }
        // 删除所有delete_list中的指令（）
        for (auto inst : delete_list) {
            bb->delete_instr(inst);
        }
    }
}

// 放置φ函数
void Mem2Reg::genPhi() {
    // 全局名字集合
    std::set<Value*> globals;
    // 变量在哪些块中被定值
    std::map<Value*, std::set<BasicBlock*>> defined_in_block;
    // 遍历所有基本块
    for (auto bb : func_->get_basic_blocks()) {
        // 遍历基本块中所有指令
        for (auto inst : bb->get_instructions()) {
            // 如果指令涉及非局部变量，则忽略
            if (!isLocalVarOp(inst)) continue;
            // 如果是load指令
            if (inst->get_instr_type() == Instruction::OpID::load) {
                // 把左值插入全局名字集合中
                Value* lvalue = static_cast<LoadInst*>(inst)->get_lval();
                globals.insert(lvalue);
            }
            // 如果是store指令
            else if (inst->get_instr_type() == Instruction::OpID::store) {
                Value* lvalue = static_cast<StoreInst*>(inst)->get_lval();
                // 把当前基本块插入左值的定值块列表
                //   到这我才反应过来store应该是把值存进一个存储单元，也就是指针，而不是放回内存2333
                //   那load对应的也就是把值从一个内存单元（指针）读出来
                auto defined_block = defined_in_block.find(lvalue);
                if (defined_block != defined_in_block.end()) {
                    defined_block->second.insert(bb);
                }
                else {
                    defined_in_block.insert({ lvalue, {bb} });
                }
            }
        }
    }

    // 此时已经脱离上一个对bb循环的for

    std::map<BasicBlock*, std::set<Value*>> bb_phi_list;

    // 对所有有定值的变量
    for (auto var : globals) {
        // 获取对变量定值的基本块集合
        auto define_bbs = defined_in_block.find(var)->second;
        std::vector<BasicBlock*> queue;
        queue.assign(define_bbs.begin(), define_bbs.end());
        auto iter_pointer = 0U;
        for (; iter_pointer < queue.size(); iter_pointer++) {
            // 对每个基本块迭代
            for (auto bb_domfront : queue[iter_pointer]->get_dom_frontier()) {
                // 对每个基本块的支配边界，在其中插入当前变量的φ函数
                auto phis = bb_phi_list.find(bb_domfront);
                if (phis != bb_phi_list.end()) {
                    if (phis->second.find(var) == phis->second.end()) {
                        phis->second.insert(var);
                        auto newphi = PhiInst::create_phi(var->get_type()->get_pointer_element_type(),
                            bb_domfront);
                        newphi->set_lval(var);
                        bb_domfront->add_instr_begin(newphi);
                        queue.push_back(bb_domfront);
                    }
                }
                else {
                    auto newphi = PhiInst::create_phi(var->get_type()->get_pointer_element_type(),
                        bb_domfront);
                    newphi->set_lval(var);
                    bb_domfront->add_instr_begin(newphi);
                    queue.push_back(bb_domfront);
                    bb_phi_list.insert({ bb_domfront, {var} });
                }
            }
        }
    }
}

// 计算变量定值相关的信息
void Mem2Reg::valueDefineCounting() {
    // 清空
    define_var = std::map<BasicBlock*, std::vector<Value*>>();
    // 对每个基本块计算定值
    for (auto bb : func_->get_basic_blocks()) {
        define_var.insert({ bb, {} });
        for (auto inst : bb->get_instructions()) {
            // φ指令，是一种对内存的定值
            if (inst->get_instr_type() == Instruction::OpID::phi) {
                auto lvalue = dynamic_cast<PhiInst*>(inst)->get_lval();
                define_var.find(bb)->second.push_back(lvalue);
            }
            // store指令，也是一种对内存的定值
            else if (inst->get_instr_type() == Instruction::OpID::store) {
                // 非局部名字不考虑
                if (!isLocalVarOp(inst)) continue;
                auto lvalue = dynamic_cast<StoreInst*>(inst)->get_lval();
                define_var.find(bb)->second.push_back(lvalue);
            }
        }
    }
}

std::map<Value *, std::vector<Value *>> value_status;
std::set<BasicBlock *> visited;

// 对于传入的一个基本块做操作
void Mem2Reg::valueForwarding(BasicBlock* bb) {
    std::set<Instruction*> delete_list;
    // 将本基本块标记为已访问（感觉可以用vector<bool>优化）
    visited.insert(bb);
    // 处理φ指令
    for (auto inst : bb->get_instructions()) {
        if (inst->get_instr_type() != Instruction::OpID::phi) break;
        auto lvalue = dynamic_cast<PhiInst*>(inst)->get_lval();
        auto value_list = value_status.find(lvalue);
        if (value_list != value_status.end()) {
            value_list->second.push_back(inst);
        }
        else {
            value_status.insert({ lvalue, {inst} });
        }
    }

    // 基本块中不涉及φ、非局部名字的指令
    for (auto inst : bb->get_instructions()) {
        if (inst->get_instr_type() == Instruction::OpID::phi) continue;
        if (!isLocalVarOp(inst)) continue;
        // 传播，用记录的值替换不必要的load和store
        if (inst->get_instr_type() == Instruction::OpID::load) {
            Value* lvalue = static_cast<LoadInst*>(inst)->get_lval();
            Value* new_value = *(value_status.find(lvalue)->second.rbegin());
            inst->replace_all_use_with(new_value);
        }
        else if (inst->get_instr_type() == Instruction::OpID::store) {
            Value* lvalue = static_cast<StoreInst*>(inst)->get_lval();
            Value* rvalue = static_cast<StoreInst*>(inst)->get_rval();
            if (value_status.find(lvalue) != value_status.end()) {
                value_status.find(lvalue)->second.push_back(rvalue);
            }
            else {
                value_status.insert({ lvalue, {rvalue} });
            }
        }
        delete_list.insert(inst);
    }

    // 给bb的后继基本块确定φ值
    // bb的后继基本块
    for (auto succbb : bb->get_succ_basic_blocks()) {
        for (auto inst : succbb->get_instructions()) {
            // 如果有φ指令
            if (inst->get_instr_type() == Instruction::OpID::phi) {
                auto phi = dynamic_cast<PhiInst*>(inst);
                auto lvalue = phi->get_lval();
                // 取到φ的左值，然后把定值表中最后一个定值给他，注意参数bb是pre_bb
                if (value_status.find(lvalue) != value_status.end()) {
                    if (value_status.find(lvalue)->second.size() > 0) {
                        Value* new_value = *(value_status.find(lvalue)->second.rbegin());
                        phi->add_phi_pair_operand(new_value, bb);
                    }
                    else {
                        //std::cout << "undefined value used: " << lvalue->get_name() << "\n";
                        // exit(-1);
                    }
                }
                else {
                    //std::cout << "undefined value used: " << lvalue->get_name() << "\n";
                    // exit(-1);
                }
            }
        }
    }

    // 如果没访问，就递归访问后续bb
    for (auto succbb : bb->get_succ_basic_blocks()) {
        if (visited.find(succbb) != visited.end()) continue;
        valueForwarding(succbb);
    }

    // 删除定值
    auto var_set = define_var.find(bb)->second;
    for (auto var : var_set) {
        if (value_status.find(var) == value_status.end()) continue;
        if (value_status.find(var)->second.size() == 0) continue;
        value_status.find(var)->second.pop_back();
    }

    for (auto inst : delete_list) {
        bb->delete_instr(inst);
    }
}

// 删除Alloca语句
void Mem2Reg::removeAlloc() {
    for (auto bb : func_->get_basic_blocks()) {
        std::set<Instruction*> delete_list;
        for (auto inst : bb->get_instructions()) {
            if (inst->get_instr_type() != Instruction::OpID::alloca) continue;
            auto alloc_inst = dynamic_cast<AllocaInst*>(inst);
            if (alloc_inst->get_alloca_type()->is_integer_type() ||
                alloc_inst->get_alloca_type()->is_float_type() ||
                alloc_inst->get_alloca_type()->is_pointer_type())
            {
                delete_list.insert(inst);
            }
        }
        for (auto inst : delete_list) {
            bb->delete_instr(inst);
        }
    }
}

void Mem2Reg::phiStatistic() {
    std::map<Value*, Value*> value_map;
    for (auto bb : func_->get_basic_blocks()) {
        for (auto inst : bb->get_instructions()) {
            if (!inst->is_phi())continue;
            auto phi_value = dynamic_cast<Value*>(inst);
            if (value_map.find(phi_value) == value_map.end()) {
                value_map.insert({ phi_value, dynamic_cast<PhiInst*>(inst)->get_lval() });
            }
        }
    }
    for (auto bb : func_->get_basic_blocks()) {
        for (auto inst : bb->get_instructions()) {
            if (!inst->is_phi())continue;
            auto phi_value = dynamic_cast<Value*>(inst);
#ifdef DEBUG
            std::cout << "phi find: " << phi_value->print() << "\n";
#endif
            Value* reduced_value;
            if (value_map.find(phi_value) != value_map.end()) {
                reduced_value = value_map.find(phi_value)->second;
            }
            else {
                reduced_value = dynamic_cast<PhiInst*>(inst)->get_lval();
                value_map.insert({ phi_value, reduced_value });
            }
            for (auto opr : inst->get_operands()) {
                if (dynamic_cast<BasicBlock*>(opr))continue;
                if (dynamic_cast<Constant*>(opr))continue;
                if (no_union_set.find(opr) != no_union_set.end())continue;
                if (value_map.find(opr) != value_map.end()) {
                    auto opr_reduced_value = value_map.find(opr)->second;
                    if (opr_reduced_value != reduced_value) {
#ifdef DEBUG
                        std::cout << "conflict! " << opr->get_name() << " -> " << opr_reduced_value->get_name();
                        std::cout << " " << phi_value->get_name() << " -> " << reduced_value->get_name() << "\n";
#endif
                    }
                }
                else {
                    if (lvalue_connection.find(opr) != lvalue_connection.end()) {
                        auto bounded_lval = lvalue_connection.find(opr)->second;
                        if (bounded_lval != reduced_value) {
#ifdef DEBUG
                            std::cout << "conflict! " << opr->get_name() << " -> " << bounded_lval->get_name();
                            std::cout << " " << phi_value->get_name() << " -> " << reduced_value->get_name() << "\n";
#endif
                        }
                        else {
                            value_map.insert({ opr, reduced_value });
                        }
                    }
                    else {
                        // value_map.insert({opr, reduced_value});
                    }
                }
            }
        }
    }

    std::map<Value*, std::set<Value*>> reversed_value_map;

    for (auto map_item : value_map) {
        Value* vreg = map_item.first;
        Value* lvalue = map_item.second;
        if (reversed_value_map.find(lvalue) != reversed_value_map.end()) {
            reversed_value_map.find(lvalue)->second.insert(vreg);
        }
        else {
            reversed_value_map.insert({ lvalue, {vreg} });
        }
    }

    for (const auto& iter : reversed_value_map) {
        func_->get_vreg_set().push_back(iter.second);
    }
}

