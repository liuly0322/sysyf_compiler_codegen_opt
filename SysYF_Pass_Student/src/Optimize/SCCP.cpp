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

const std::unordered_set<std::string> binary_ops{"add",  "sub",  "mul",  "sdiv",
                                                 "srem", "fadd", "fsub", "fmul",
                                                 "fdiv", "cmp",  "fcmp"};

const std::unordered_set<std::string> unary_ops{"fptosi", "sitofp", "zext"};

struct ValueStatus {
    enum Status { BOT = 0, CONST, TOP };
    Status status;
    Constant *value;
    void operator^=(ValueStatus &b) {
        if (b.status < status) {
            status = b.status;
            value = b.value;
        } else if (status == b.status && status == CONST) {
            if (dynamic_cast<ConstantInt *>(value) != nullptr) {
                auto x = static_cast<ConstantInt *>(value)->get_value();
                auto y = static_cast<ConstantInt *>(b.value)->get_value();
                if (x != y) {
                    status = BOT;
                    value = nullptr;
                }
            } else {
                auto x = static_cast<ConstantFloat *>(value)->get_value();
                auto y = static_cast<ConstantFloat *>(b.value)->get_value();
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
            auto x = static_cast<ConstantInt *>(value)->get_value();
            auto y = static_cast<ConstantInt *>(b.value)->get_value();
            return x != y;
        }
        auto x = static_cast<ConstantFloat *>(value)->get_value();
        auto y = static_cast<ConstantFloat *>(b.value)->get_value();
        return x != y;
    }
};

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

void branch_to_jmp(Instruction *inst, BasicBlock *jmp_bb,
                   BasicBlock *invalid_bb) {
    // 无条件跳转至 jmp_bb
    auto *bb = inst->get_parent();
    bb->remove_succ_basic_block(invalid_bb);
    invalid_bb->remove_pre_basic_block(bb);
    inst->remove_operands(0, 2);
    inst->add_operand(jmp_bb);
    // invalid_bb 如果开头有当前 bb 来的 phi，则进行精简
    auto delete_list = std::vector<Instruction *>{};
    for (auto *inst : invalid_bb->get_instructions()) {
        if (!inst->is_phi())
            break;
        for (auto i = 1; i < inst->get_operands().size();) {
            auto *op = inst->get_operand(i);
            if (op == bb) {
                inst->remove_operands(i - 1, i);
            } else {
                i += 2;
            }
        }
        if (inst->get_num_operand() == 2) {
            inst->replace_all_use_with(inst->get_operand(0));
            delete_list.push_back(inst);
        }
    }
    for (auto *inst : delete_list) {
        invalid_bb->delete_instr(inst);
    }
}

ValueStatus get_mapped(std::unordered_map<Value *, ValueStatus> &value_map,
                       Value *key) {
    if (auto *constant = dynamic_cast<Constant *>(key))
        return {ValueStatus::CONST, constant};
    return value_map[key];
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

void SCCP::execute() {
    // 遍历每个函数
    // std::cout << module->print();
    for (auto *f : module->get_functions()) {
        if (!f->is_declaration())
            sccp(f, module);
    }
    // std::cout << module->print();
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
    for (auto *arg : f->get_args()) {
        value_map.insert({arg, ValueStatus{ValueStatus::BOT}});
    }
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *expr : bb->get_instructions()) {
            value_map.insert({expr, ValueStatus{ValueStatus::TOP}});
        }
    }

    auto visit_phi = [&](Instruction *inst) {
        auto prev_status = value_map[inst];
        auto cur_status = prev_status;
        auto *phi_inst = dynamic_cast<PhiInst *>(inst);
        auto *bb = phi_inst->get_parent();

        const int phi_size = phi_inst->get_num_operand() / 2;
        for (int i = 0; i < phi_size; i++) {
            auto *pre_bb =
                static_cast<BasicBlock *>(phi_inst->get_operand(2 * i + 1));
            if (marked.count({pre_bb, bb}) != 0) {
                auto *op = phi_inst->get_operand(2 * i);
                auto op_status = get_mapped(value_map, op);
                cur_status ^= op_status;
            }
        }

        if (cur_status != prev_status) {
            value_map[inst] = cur_status;
            for (auto use : inst->get_use_list()) {
                auto *use_inst = dynamic_cast<Instruction *>(use.val_);
                ssa_work_list.push_back(use_inst);
            }
        }
    };

    auto visit_inst = [&](Instruction *inst) {
        auto *bb = inst->get_parent();
        if (inst->is_br()) {
            // 取决于 operand[0] 定值
            if (static_cast<BranchInst *>(inst)->is_cond_br()) {
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
            auto prev_status = value_map[inst];
            auto cur_status = prev_status;
            auto *folded = const_fold(value_map, inst, module);
            if (folded != nullptr) {
                cur_status = ValueStatus{ValueStatus::CONST, folded};
            } else {
                // 否则是 top 或 bot，取决于 operands 是否出现 bot
                cur_status = ValueStatus{ValueStatus::TOP};
                for (auto *op : inst->get_operands()) {
                    auto *op_global = dynamic_cast<GlobalVariable *>(op);
                    auto *op_arg = dynamic_cast<Argument *>(op);
                    auto *op_inst = dynamic_cast<Instruction *>(op);
                    if (op_global != nullptr || op_arg != nullptr ||
                        (op_inst != nullptr &&
                         value_map[op_inst].status == ValueStatus::BOT)) {
                        cur_status = ValueStatus{ValueStatus::BOT};
                    }
                }
            }
            if (cur_status != prev_status) {
                value_map[inst] = cur_status;
                for (auto use : inst->get_use_list()) {
                    auto *use_inst = dynamic_cast<Instruction *>(use.val_);
                    ssa_work_list.push_back(use_inst);
                }
            }
        } else {
            auto prev_status = value_map[inst];
            auto cur_status = ValueStatus{ValueStatus::BOT};
            if (cur_status != prev_status) {
                value_map[inst] = cur_status;
                for (auto use : inst->get_use_list()) {
                    auto *use_inst = dynamic_cast<Instruction *>(use.val_);
                    ssa_work_list.push_back(use_inst);
                }
            }
        }
    };

    int i = 0;
    int j = 0;
    while (i < cfg_work_list.size() || j < ssa_work_list.size()) {
        while (i < cfg_work_list.size()) {
            auto [pre_bb, bb] = cfg_work_list[i++];

            if (marked.count({pre_bb, bb}) != 0)
                continue;
            marked.insert({pre_bb, bb});

            for (auto *inst : bb->get_instructions()) {
                if (inst->is_phi())
                    visit_phi(inst);
                else
                    visit_inst(inst);
            }
        }
        while (j < ssa_work_list.size()) {
            auto *inst = ssa_work_list[j++];

            // 如果指令已经是 bot 了，不需要再传播
            if (get_mapped(value_map, inst).status == ValueStatus::BOT)
                continue;

            if (inst->is_phi())
                visit_phi(inst);
            else
                visit_inst(inst);
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