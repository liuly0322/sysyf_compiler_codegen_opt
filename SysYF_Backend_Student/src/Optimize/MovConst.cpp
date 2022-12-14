#include "MovConst.h"

void MovConst::execute() {
    for(auto fun:module->get_functions()){
        if(fun->get_basic_blocks().empty()){
            continue;
        }
        for(auto bb:fun->get_basic_blocks()){
            mov_const(bb);
        }
    }
}

void MovConst::mov_const(BasicBlock *bb) {
    auto &instructions = bb->get_instructions();
    for (auto iter = instructions.begin(); iter != instructions.end(); iter++){
        auto instr = *iter;
        if (instr->is_store()) {
            auto op1 = instr->get_operand(0);
            auto const_op1 = dynamic_cast<ConstantInt*>(op1);
            if (const_op1) {
                auto mov_const_instr = MovConstInst::create_mov_const(const_op1, bb);
                instructions.pop_back();
                bb->add_instruction(iter, mov_const_instr);
                (instr->get_operand(0))->remove_use(instr);
                instr->set_operand(0, mov_const_instr);
            }
        }
        if (instr->is_add() || instr->is_sub()) {
            auto op1 = instr->get_operand(0);
            auto op2 = instr->get_operand(1);
            auto const_op1 = dynamic_cast<ConstantInt*>(op1);
            auto const_op2 = dynamic_cast<ConstantInt*>(op2);
            if (const_op1 && const_op2) {
                auto const_op1_val = const_op1->get_value();
                auto mov_const_instr = MovConstInst::create_mov_const(const_op1, bb);
                instructions.pop_back();
                bb->add_instruction(iter, mov_const_instr);
                (instr->get_operand(0))->remove_use(instr);
                instr->set_operand(0, mov_const_instr);
                auto const_op2_val = const_op2->get_value();
                if (const_op2_val >= (1<<8) || const_op2_val < 0) {
                    auto mov_const_instr = MovConstInst::create_mov_const(const_op2, bb);
                    instructions.pop_back();
                    bb->add_instruction(iter, mov_const_instr);
                    (instr->get_operand(1))->remove_use(instr);
                    instr->set_operand(1, mov_const_instr);
                }
            } else {
                if (const_op1) {
                    auto const_op1_val = const_op1->get_value();
                    if (const_op1_val >= (1<<8) || const_op1_val < 0) {
                        auto mov_const_instr = MovConstInst::create_mov_const(const_op1, bb);
                        instructions.pop_back();
                        bb->add_instruction(iter, mov_const_instr);
                        (instr->get_operand(0))->remove_use(instr);
                        instr->set_operand(0, mov_const_instr);
                    }
                }
                if (const_op2) {
                    auto const_op2_val = const_op2->get_value();
                    if (const_op2_val >= (1<<8) || const_op2_val < 0) {
                        auto mov_const_instr = MovConstInst::create_mov_const(const_op2, bb);
                        instructions.pop_back();
                        bb->add_instruction(iter, mov_const_instr);
                        (instr->get_operand(1))->remove_use(instr);
                        instr->set_operand(1, mov_const_instr);
                    }
                }
            }
        }
        if (instr->is_cmp() || instr->is_cmpbr() || instr->is_mul() || instr->is_div() || instr->is_rem()) {
            auto op1 = instr->get_operand(0);
            auto op2 = instr->get_operand(1);
            auto const_op1 = dynamic_cast<ConstantInt*>(op1);
            auto const_op2 = dynamic_cast<ConstantInt*>(op2);
            if (const_op1) {
                auto mov_const_instr = MovConstInst::create_mov_const(const_op1, bb);
                instructions.pop_back();
                bb->add_instruction(iter, mov_const_instr);
                (instr->get_operand(0))->remove_use(instr);
                instr->set_operand(0, mov_const_instr);
            }
            if (const_op2) {
                auto mov_const_instr = MovConstInst::create_mov_const(const_op2, bb);
                instructions.pop_back();
                bb->add_instruction(iter, mov_const_instr);
                (instr->get_operand(1))->remove_use(instr);
                instr->set_operand(1, mov_const_instr);
            }
        }
        if (instr->is_gep()) {
            int offset_position;
            if (instr->get_num_operand() == 2) {
                offset_position = 1;
            } else if (instr->get_num_operand() == 3) {
                offset_position = 2;
            }
            auto offset = instr->get_operand(offset_position);
            auto const_offset = dynamic_cast<ConstantInt*>(offset);
            if (const_offset) {
                auto const_offset_val = const_offset->get_value();
                if ((const_offset_val << 2) >= (1<<8) || const_offset_val < 0) {
                    auto mov_const_instr = MovConstInst::create_mov_const(const_offset, bb);
                    instructions.pop_back();
                    bb->add_instruction(iter, mov_const_instr);
                    (instr->get_operand(offset_position))->remove_use(instr);
                    instr->set_operand(offset_position, mov_const_instr);
                }
            }
        }
    }
}
