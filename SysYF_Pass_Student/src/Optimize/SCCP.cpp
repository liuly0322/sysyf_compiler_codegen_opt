#include "SCCP.h"

#define CONST_INT(num) ConstantInt::get(num, module)
#define CONST_FLOAT(num) ConstantFloat::get(num, module)

Constant *SCCP::constFold(Instruction *inst, Constant *v1, Constant *v2) {
    auto op = inst->get_instr_type();
    switch (op) {
    case Instruction::add:
        return CONST_INT(get_const_int(v1) + get_const_int(v2));
    case Instruction::sub:
        return CONST_INT(get_const_int(v1) - get_const_int(v2));
    case Instruction::mul:
        return CONST_INT(get_const_int(v1) * get_const_int(v2));
    case Instruction::sdiv:
        return CONST_INT(get_const_int(v1) / get_const_int(v2));
    case Instruction::srem:
        return CONST_INT(get_const_int(v1) % get_const_int(v2));
    case Instruction::fadd:
        return CONST_FLOAT(get_const_float(v1) + get_const_float(v2));
    case Instruction::fsub:
        return CONST_FLOAT(get_const_float(v1) - get_const_float(v2));
    case Instruction::fmul:
        return CONST_FLOAT(get_const_float(v1) * get_const_float(v2));
    case Instruction::fdiv:
        return CONST_FLOAT(get_const_float(v1) / get_const_float(v2));

    case Instruction::cmp:
        switch (dynamic_cast<CmpInst *>(inst)->get_cmp_op()) {
        case CmpInst::EQ:
            return CONST_INT(get_const_int(v1) == get_const_int(v2));
        case CmpInst::NE:
            return CONST_INT(get_const_int(v1) != get_const_int(v2));
        case CmpInst::GT:
            return CONST_INT(get_const_int(v1) > get_const_int(v2));
        case CmpInst::GE:
            return CONST_INT(get_const_int(v1) >= get_const_int(v2));
        case CmpInst::LT:
            return CONST_INT(get_const_int(v1) < get_const_int(v2));
        case CmpInst::LE:
            return CONST_INT(get_const_int(v1) <= get_const_int(v2));
        }

    case Instruction::fcmp:
        switch (dynamic_cast<FCmpInst *>(inst)->get_cmp_op()) {
        case FCmpInst::EQ:
            return CONST_INT(get_const_float(v1) == get_const_float(v2));
        case FCmpInst::NE:
            return CONST_INT(get_const_float(v1) != get_const_float(v2));
        case FCmpInst::GT:
            return CONST_INT(get_const_float(v1) > get_const_float(v2));
        case FCmpInst::GE:
            return CONST_INT(get_const_float(v1) >= get_const_float(v2));
        case FCmpInst::LT:
            return CONST_INT(get_const_float(v1) < get_const_float(v2));
        case FCmpInst::LE:
            return CONST_INT(get_const_float(v1) <= get_const_float(v2));
        }
    default:
        return nullptr;
    }
}

Constant *SCCP::constFold(Instruction *inst, Constant *v) {
    auto op = inst->get_instr_type();
    switch (op) {
    case Instruction::zext:
        return CONST_INT(get_const_int(v));
    case Instruction::sitofp:
        return CONST_FLOAT((float)get_const_int(v));
    case Instruction::fptosi:
        return CONST_INT((int)get_const_float(v));
    default:
        return nullptr;
    }
}

Constant *SCCP::constFold(Instruction *inst) {
    if (binary_ops.count(inst->get_instr_op_name()) != 0) {
        auto *const_v1 = getMapped(inst->get_operand(0)).value;
        auto *const_v2 = getMapped(inst->get_operand(1)).value;
        if (const_v1 != nullptr && const_v2 != nullptr)
            return constFold(inst, const_v1, const_v2);
    } else if (unary_ops.count(inst->get_instr_op_name()) != 0) {
        auto *const_v = getMapped(inst->get_operand(0)).value;
        if (const_v != nullptr)
            return constFold(inst, const_v);
    }
    return nullptr;
}

static void cond_br_to_jmp(Instruction *inst, BasicBlock *jmp_bb,
                           BasicBlock *invalid_bb) {
    auto *bb = inst->get_parent();
    inst->remove_operands(0, 2);
    inst->add_operand(jmp_bb);
    if (jmp_bb == invalid_bb)
        return;
    bb->remove_succ_basic_block(invalid_bb);
    invalid_bb->remove_pre_basic_block(bb);
}

void SCCP::execute() {
    for (auto *f : module->get_functions())
        sccp(f);
}

void SCCP::visitInst(Instruction *inst) {
    auto *bb = inst->get_parent();
    auto prev_status = value_map[inst];
    auto cur_status = prev_status;
    if (inst->is_phi()) {
        const int phi_size = inst->get_num_operand() / 2;
        for (int i = 0; i < phi_size; i++) {
            auto *pre_bb =
                static_cast<BasicBlock *>(inst->get_operand(2 * i + 1));
            if (marked.count({pre_bb, bb}) != 0) {
                auto *op = inst->get_operand(2 * i);
                auto op_status = getMapped(op);
                cur_status ^= op_status;
            }
        }
    } else if (inst->is_br()) {
        if (!static_cast<BranchInst *>(inst)->is_cond_br()) {
            auto *jmp_bb = static_cast<BasicBlock *>(inst->get_operand(0));
            cfg_worklist.emplace_back(bb, jmp_bb);
        } else {
            auto *true_bb = static_cast<BasicBlock *>(inst->get_operand(1));
            auto *false_bb = static_cast<BasicBlock *>(inst->get_operand(2));
            auto *const_cond = static_cast<ConstantInt *>(
                getMapped(inst->get_operand(0)).value);
            if (const_cond != nullptr) {
                const_cond->get_value() != 0
                    ? cfg_worklist.emplace_back(bb, true_bb)
                    : cfg_worklist.emplace_back(bb, false_bb);
            } else {
                cfg_worklist.emplace_back(bb, true_bb);
                cfg_worklist.emplace_back(bb, false_bb);
            }
        }
    } else if (binary_ops.count(inst->get_instr_op_name()) != 0 ||
               unary_ops.count(inst->get_instr_op_name()) != 0) {
        auto *folded = constFold(inst);
        if (folded != nullptr) {
            cur_status = {ValueStatus::CONST, folded};
        } else {
            cur_status = {ValueStatus::TOP};
            for (auto *op : inst->get_operands())
                if (value_map[op].is_bot())
                    cur_status = {ValueStatus::BOT};
        }
    } else {
        cur_status = {ValueStatus::BOT};
    }
    if (cur_status != prev_status) {
        value_map[inst] = cur_status;
        for (auto use : inst->get_use_list()) {
            auto *use_inst = dynamic_cast<Instruction *>(use.val_);
            ssa_worklist.push_back(use_inst);
        }
    }
}

void SCCP::sccp(Function *f) {
    if (f->is_declaration())
        return;

    value_map.clear();
    marked.clear();
    cfg_worklist.clear();
    ssa_worklist.clear();

    cfg_worklist.emplace_back(nullptr, f->get_entry_block());

    for (auto *bb : f->get_basic_blocks())
        for (auto *expr : bb->get_instructions())
            value_map[expr] = {ValueStatus::TOP};

    auto i = 0U;
    auto j = 0U;
    while (i < cfg_worklist.size() || j < ssa_worklist.size()) {
        while (i < cfg_worklist.size()) {
            auto [pre_bb, bb] = cfg_worklist[i++];

            if (marked.count({pre_bb, bb}) != 0)
                continue;
            marked.insert({pre_bb, bb});

            for (auto *inst : bb->get_instructions())
                visitInst(inst);
        }
        while (j < ssa_worklist.size()) {
            auto *inst = ssa_worklist[j++];
            auto *bb = inst->get_parent();

            for (auto *pre_bb : bb->get_pre_basic_blocks()) {
                if (marked.count({pre_bb, bb}) != 0) {
                    visitInst(inst);
                    break;
                }
            }
        }
    }
    replaceConstant(f);
}

void SCCP::replaceConstant(Function *f) {
    std::vector<Instruction *> delete_list;
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (auto *constant = getMapped(inst).value) {
                inst->replace_all_use_with(constant);
                delete_list.push_back(inst);
            }
        }
    }
    for (auto *inst : delete_list)
        inst->get_parent()->delete_instr(inst);

    // cond_br with const cond
    for (auto *bb : f->get_basic_blocks()) {
        auto *branch_inst = dynamic_cast<BranchInst *>(bb->get_terminator());
        if (branch_inst == nullptr || !branch_inst->is_cond_br())
            continue;
        auto *const_cond =
            dynamic_cast<ConstantInt *>(branch_inst->get_operand(0));
        if (const_cond == nullptr)
            continue;
        auto *true_bb = static_cast<BasicBlock *>(branch_inst->get_operand(1));
        auto *false_bb = static_cast<BasicBlock *>(branch_inst->get_operand(2));
        if (const_cond->get_value() != 0)
            cond_br_to_jmp(branch_inst, true_bb, false_bb);
        else
            cond_br_to_jmp(branch_inst, false_bb, true_bb);
    }
}