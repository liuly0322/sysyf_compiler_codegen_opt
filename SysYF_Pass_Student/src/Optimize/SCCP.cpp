#include "SCCP.h"
#include "BasicBlock.h"
#include "Constant.h"
#include "GlobalVariable.h"
#include "Instruction.h"
#include "Value.h"
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define CONST_INT(num) ConstantInt::get(num, module)
#define CONST_FLOAT(num) ConstantFloat::get(num, module)

static inline int get_const_int(Value *constant) {
    return static_cast<ConstantInt *>(constant)->get_value();
}

static inline float get_const_float(Value *constant) {
    return static_cast<ConstantFloat *>(constant)->get_value();
}

struct ValueStatus {
    enum Status { BOT = 0, CONST, TOP };
    Status status;
    Constant *value;
    [[nodiscard]] bool is_bot() const { return status == BOT; }
    [[nodiscard]] bool is_const() const { return status == CONST; }
    [[nodiscard]] bool is_top() const { return status == TOP; }
    void operator^=(ValueStatus &b) {
        if (b.status < status) {
            status = b.status;
            value = b.value;
        } else if (status == b.status && status == CONST) {
            if (dynamic_cast<ConstantInt *>(value) != nullptr) {
                auto x = get_const_int(value);
                auto y = get_const_int(b.value);
                if (x != y) {
                    status = BOT;
                    value = nullptr;
                }
            } else {
                auto x = get_const_float(value);
                auto y = get_const_float(b.value);
                if (x != y) {
                    status = BOT;
                    value = nullptr;
                }
            }
        }
    }
    bool operator!=(ValueStatus &b) const {
        if (status != b.status)
            return true;
        if (status != CONST)
            return false;
        if (dynamic_cast<ConstantInt *>(value) != nullptr) {
            auto x = get_const_int(value);
            auto y = get_const_int(b.value);
            return x != y;
        }
        auto x = get_const_float(value);
        auto y = get_const_float(b.value);
        return x != y;
    }
};

ValueStatus get_mapped(std::unordered_map<Value *, ValueStatus> &value_map,
                       Value *key) {
    if (auto *constant = dynamic_cast<Constant *>(key))
        return {ValueStatus::CONST, constant};
    return value_map[key];
}

const std::unordered_set<std::string> binary_ops{"add",  "sub",  "mul",  "sdiv",
                                                 "srem", "fadd", "fsub", "fmul",
                                                 "fdiv", "cmp",  "fcmp"};

const std::unordered_set<std::string> unary_ops{"fptosi", "sitofp", "zext"};

Constant *const_fold(Instruction *inst, Constant *v1, Constant *v2,
                     Module *module) {
    // BinaryInst, CmpInst, FCmpInst
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

Constant *const_fold(Instruction *inst, Constant *v, Module *module) {
    // ZextInst, SiToFpInst, FpToSiInst
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

Constant *const_fold(std::unordered_map<Value *, ValueStatus> &value_map,
                     Instruction *inst, Module *module) {
    if (binary_ops.count(inst->get_instr_op_name()) != 0) {
        auto *const_v1 = get_mapped(value_map, inst->get_operand(0)).value;
        auto *const_v2 = get_mapped(value_map, inst->get_operand(1)).value;
        if (const_v1 != nullptr && const_v2 != nullptr) {
            return const_fold(inst, const_v1, const_v2, module);
        }
    } else if (unary_ops.count(inst->get_instr_op_name()) != 0) {
        auto *const_v = get_mapped(value_map, inst->get_operand(0)).value;
        if (const_v != nullptr) {
            return const_fold(inst, const_v, module);
        }
    }
    return nullptr;
}

void branch_to_jmp(Instruction *inst, BasicBlock *jmp_bb,
                   BasicBlock *invalid_bb) {
    auto *bb = inst->get_parent();
    // 指令无条件跳转至 jmp_bb
    inst->remove_operands(0, 2);
    inst->add_operand(jmp_bb);
    // 调整前驱后继关系
    if (jmp_bb == invalid_bb)
        return;
    bb->remove_succ_basic_block(invalid_bb);
    invalid_bb->remove_pre_basic_block(bb);
}

void SCCP::execute() {
    // 遍历每个函数
    for (auto *f : module->get_functions()) {
        if (!f->is_declaration())
            sccp(f, module);
    }
}

struct hashFunction {
    size_t operator()(const std::pair<BasicBlock *, BasicBlock *> &x) const {
        return reinterpret_cast<u_int64_t>(x.first) ^
               reinterpret_cast<u_int64_t>(x.second);
    }
};

void SCCP::sccp(Function *f, Module *module) {
    auto value_map = std::unordered_map<Value *, ValueStatus>{};
    auto cfg_work_list = std::vector<std::pair<BasicBlock *, BasicBlock *>>{
        {nullptr, f->get_entry_block()}};
    auto ssa_work_list = std::vector<Instruction *>{};
    auto marked = std::unordered_set<std::pair<BasicBlock *, BasicBlock *>,
                                     hashFunction>{};
    // 初始化
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *expr : bb->get_instructions()) {
            value_map.insert({expr, {ValueStatus::TOP}});
        }
    }

    auto visit_inst = [&](Instruction *inst) {
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
                    auto op_status = get_mapped(value_map, op);
                    cur_status ^= op_status;
                }
            }
        } else if (inst->is_br()) {
            // 分支或跳转
            if (static_cast<BranchInst *>(inst)->is_cond_br()) {
                // 取决于 operand[0] 是否为 Constant*
                auto *const_cond = static_cast<ConstantInt *>(
                    get_mapped(value_map, inst->get_operand(0)).value);
                auto *true_bb = static_cast<BasicBlock *>(inst->get_operand(1));
                auto *false_bb =
                    static_cast<BasicBlock *>(inst->get_operand(2));
                if (const_cond != nullptr) {
                    auto cond = const_cond->get_value();
                    if (cond != 0) {
                        cfg_work_list.emplace_back(bb, true_bb);
                    } else {
                        cfg_work_list.emplace_back(bb, false_bb);
                    }
                } else {
                    // put both sides
                    cfg_work_list.emplace_back(bb, true_bb);
                    cfg_work_list.emplace_back(bb, false_bb);
                }
            } else {
                auto *jmp_bb = static_cast<BasicBlock *>(inst->get_operand(0));
                cfg_work_list.emplace_back(bb, jmp_bb);
            }
        } else if (binary_ops.count(inst->get_instr_op_name()) != 0 ||
                   unary_ops.count(inst->get_instr_op_name()) != 0) {
            auto *folded = const_fold(value_map, inst, module);
            if (folded != nullptr) {
                cur_status = {ValueStatus::CONST, folded};
            } else {
                // 否则是 top 或 bot，取决于 operands 是否出现 bot
                cur_status = {ValueStatus::TOP};
                for (auto *op : inst->get_operands()) {
                    if (value_map[op].is_bot()) {
                        cur_status = {ValueStatus::BOT};
                    }
                }
            }
        } else {
            cur_status = {ValueStatus::BOT};
        }
        if (cur_status != prev_status) {
            value_map[inst] = cur_status;
            for (auto use : inst->get_use_list()) {
                auto *use_inst = dynamic_cast<Instruction *>(use.val_);
                ssa_work_list.push_back(use_inst);
            }
        }
    };

    auto i = 0U;
    auto j = 0U;
    while (i < cfg_work_list.size() || j < ssa_work_list.size()) {
        while (i < cfg_work_list.size()) {
            auto [pre_bb, bb] = cfg_work_list[i++];

            if (marked.count({pre_bb, bb}) != 0)
                continue;
            marked.insert({pre_bb, bb});

            for (auto *inst : bb->get_instructions())
                visit_inst(inst);
        }
        while (j < ssa_work_list.size()) {
            auto *inst = ssa_work_list[j++];
            auto *bb = inst->get_parent();

            // 仅当指令可达才更新状态
            for (auto *pre_bb : bb->get_pre_basic_blocks()) {
                if (marked.count({pre_bb, bb}) != 0) {
                    visit_inst(inst);
                    break;
                }
            }
        }
    }

    // 收尾工作，指令替换为常量
    std::vector<Instruction *> delete_list;
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (auto *constant = get_mapped(value_map, inst).value) {
                inst->replace_all_use_with(constant);
                delete_list.push_back(inst);
            }
        }
    }
    for (auto *inst : delete_list) {
        inst->get_parent()->delete_instr(inst);
    }

    // BR 指令处理
    for (auto *bb : f->get_basic_blocks()) {
        auto *terminator = dynamic_cast<BranchInst *>(bb->get_terminator());
        if (terminator == nullptr || !terminator->is_cond_br())
            continue;
        auto *const_cond =
            dynamic_cast<ConstantInt *>(terminator->get_operand(0));
        if (const_cond == nullptr)
            continue;
        auto *true_bb = static_cast<BasicBlock *>(terminator->get_operand(1));
        auto *false_bb = static_cast<BasicBlock *>(terminator->get_operand(2));
        if (const_cond->get_value() != 0) {
            // 无条件跳转至真基本块
            branch_to_jmp(terminator, true_bb, false_bb);
        } else {
            // 无条件跳转至假基本块
            branch_to_jmp(terminator, false_bb, true_bb);
        }
    }
}