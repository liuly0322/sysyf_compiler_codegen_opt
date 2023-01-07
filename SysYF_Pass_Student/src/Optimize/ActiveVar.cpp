#include "ActiveVar.h"
#include <fstream>

#include <algorithm>

void ActiveVar::execute() {
    //  请不要修改该代码。在被评测时不要在中间的代码中重新调用set_print_name
    module->set_print_name();
    //

    for (auto& func : this->module->get_functions()) {
        if (func->get_basic_blocks().empty()) continue;

        func_ = func;

        // 寻找出口块
        BasicBlock* exit_block = nullptr;
        for (auto bb : func_->get_basic_blocks()) {
            auto terminate_instr = bb->get_terminator();
            if (terminate_instr->is_ret()) {
                exit_block = bb;
                break;
            }
        }

        // 设置IN、OUT集合
        std::map<BasicBlock*, std::set<Value*>> IN_list;
        std::map<BasicBlock*, std::set<Value*>> OUT_list;
        bool changed = true;
        while (changed) {
            changed = false;

            auto blocks = func_->get_basic_blocks();
            for (auto& bb : blocks) {
                if (bb = exit_block) continue;
                // 计算OUT[bb]
                auto succ_blocks = bb->get_succ_basic_blocks();
                for (auto& succ_bb : succ_blocks) {
                    // 先加入IN[succ_bb]
                    for (auto& value : IN_list[succ_bb]) {
                        OUT_list[bb].insert(value);
                    }
                    // 再根据 φ 指令设置活跃性
                    for (auto& inst : succ_bb->get_instructions()) {
                        if (inst->get_instr_type() != Instruction::OpID::phi) break;
                        auto phi_inst = dynamic_cast<PhiInst*>(inst);
                        int phi_size = phi_inst->get_num_operand() / 2;
                        for (int i = 0; i < phi_size; i++) {
                            auto block = dynamic_cast<BasicBlock*>(phi_inst->get_operand(2 * i + 1));
                            if (block && block == bb) {
                                auto active_var = dynamic_cast<Value*>(phi_inst->get_operand(2 * i));
                                OUT_list[bb].insert(active_var);
                            }
                        }
                    }
                }
                // 计算IN[bb]
                std::set<Instruction*> def; // 定值先于引用
                std::set<Instruction*> use; // 引用先于定值
                for (auto& inst : bb->get_instructions()) {
                    auto real_inst = dynamic_cast<Instruction*>(inst);
                    if (real_inst) {
                        // 先处理操作数
                        auto operands = real_inst->get_operands();
                        for (auto& op : operands) {
                            auto real_op = dynamic_cast<Instruction*>(op);
                            // 之前未定义，说明是引用先于定义
                            if (real_op && def.find(real_op) != def.end()) {
                                use.insert(real_op);
                            }
                            else continue;
                        }
                        // 再处理左侧
                        if (def.find(real_inst) == def.end()) {
                            def.insert(real_inst);
                        }
                    }
                }
                // 计算OUT[bb]-def_B
                std::set<Value*> tmp_out_bb(OUT_list[bb]);
                for (auto& instr : def) {
                    auto find_in_out = tmp_out_bb.find(instr);
                    if (find_in_out != tmp_out_bb.end()) {
                        tmp_out_bb.erase(find_in_out);
                    }
                }
                // 计算use_B∪(OUT[bb]-def_B)
                for (auto& instr : use) {
                    tmp_out_bb.insert(instr);
                }
                // 将tmp转化为IN，先看是否有修改
                if (tmp_out_bb.size() != IN_list[bb].size()) changed = true;
                else {
                    for (auto& tmp_instr : tmp_out_bb) {
                        if (IN_list[bb].find(tmp_instr) == IN_list[bb].end()) {
                            changed = true;
                            break;
                        }
                    }
                }
                IN_list[bb].clear();
                for (auto& instr : tmp_out_bb) {
                    IN_list[bb].insert(instr);
                }
            }
        }

        auto blocks = func_->get_basic_blocks();
        for (auto& bb : blocks) {
            bb->set_live_in(IN_list[bb]);
            bb->set_live_out(OUT_list[bb]);
        }
        /*you need to finish this function*/
    }

    //  请不要修改该代码，在被评测时不要删除该代码
    dump();
    //
    return;
}

void ActiveVar::dump() {
    std::fstream f;
    f.open(avdump, std::ios::out);
    for (auto &func: module->get_functions()) {
        for (auto &bb: func->get_basic_blocks()) {
            f << bb->get_name() << std::endl;
            auto &in = bb->get_live_in();
            auto &out = bb->get_live_out();
            auto sorted_in = sort_by_name(in);
            auto sorted_out = sort_by_name(out);
            f << "in:\n";
            for (auto in_v: sorted_in) {
                f << in_v->get_name() << " ";
            }
            f << "\n";
            f << "out:\n";
            for (auto out_v: sorted_out) {
                f << out_v->get_name() << " ";
            }
            f << "\n";
        }
    }
    f.close();
}

bool ValueCmp(Value* a, Value* b) {
    return a->get_name() < b->get_name();
}

std::vector<Value*> sort_by_name(std::set<Value*> &val_set) {
    std::vector<Value*> result;
    result.assign(val_set.begin(), val_set.end());
    std::sort(result.begin(), result.end(), ValueCmp);
    return result;
}