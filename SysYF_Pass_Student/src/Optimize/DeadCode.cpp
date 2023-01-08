#include "DeadCode.h"
#include "Instruction.h"
#include "PureFunction.h"
#include "Value.h"
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using PureFunction::is_pure;

static inline void delete_basic_block(BasicBlock *i, BasicBlock *j) {
    const std::vector<Use> uses{i->get_use_list().begin(), i->get_use_list().end()};
    for (const auto &use : uses) {
        // 可能是 phi 指令或者跳转指令
        // 跳转指令直接替换成 j 即可
        auto *inst = static_cast<Instruction *>(use.val_);
        if (inst->is_br()) {
            inst->set_operand(use.arg_no_, j);
            continue;
        }
        // phi 指令需要统计 i 的所有前驱
        auto *op = inst->get_operand(use.arg_no_ - 1);
        inst->remove_operands(use.arg_no_ - 1, use.arg_no_);
        for (auto *pre : i->get_pre_basic_blocks()) {
            dynamic_cast<PhiInst *>(inst)->add_phi_pair_operand(op, pre);
        }
    }
    // 调整前驱后继关系
    for (auto *pre : i->get_pre_basic_blocks()) {
        pre->add_succ_basic_block(j);
        j->add_pre_basic_block(pre);
    }
    // 删除基本块 i
    i->get_parent()->remove(i);
}

bool DeadCode::is_critical_inst(Instruction *inst) {
    if (inst->is_ret() ||
        (inst->is_store() && PureFunction::store_to_alloca(inst) == nullptr)) {
        return true;
    }
    if (inst->is_br()) {
        auto *br_inst = dynamic_cast<BranchInst *>(inst);
        // 一开始只有 jmp 需要标记
        return !br_inst->is_cond_br();
    }
    if (inst->is_call()) {
        // 只标记非纯函数的调用
        auto *call_inst = dynamic_cast<CallInst *>(inst);
        auto *callee = dynamic_cast<Function *>(call_inst->get_operand(0));
        return !is_pure[callee];
    }
    return false;
}

void DeadCode::execute() {
    // 标记纯函数
    PureFunction::markPure(module);
    // 逐个函数处理
    for (auto *f : module->get_functions()) {
        // 标记重要变量
        auto marked = mark(f);
        // 清理空指令
        sweep(f, marked);
        // 清理控制流
        clean(f);
    }
}

std::unordered_set<Instruction *> DeadCode::mark(Function *f) {
    std::unordered_set<Instruction *> marked{};
    std::vector<Instruction *> work_list{};

    // 初始标记 critical 变量
    std::unordered_map<AllocaInst *, std::unordered_set<StoreInst *>>
        lval_store{};
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (is_critical_inst(inst)) {
                marked.insert(inst);
                work_list.push_back(inst);
            } else if (inst->is_store()) {
                auto *lval = PureFunction::store_to_alloca(inst);
                if (lval_store.count(lval) != 0) {
                    lval_store[lval].insert(static_cast<StoreInst *>(inst));
                } else {
                    lval_store[lval] = {static_cast<StoreInst *>(inst)};
                }
            }
        }
    }

    // 遍历 worklist
    for (auto i = 0; i < work_list.size(); i++) {
        auto *inst = work_list[i];
        // 如果数组左值 critical，则标记所有对数组的 store 也为 critical
        if (inst->is_alloca()) {
            auto *lval = static_cast<AllocaInst *>(inst);
            for (auto *store_inst : lval_store[lval]) {
                marked.insert(store_inst);
                work_list.push_back(store_inst);
            }
            lval_store.erase(lval);
        }
        // mark 所有 operand
        for (auto *op : inst->get_operands()) {
            auto *def = dynamic_cast<Instruction *>(op);
            if (def == nullptr || marked.count(def) != 0)
                continue;
            marked.insert(def);
            work_list.push_back(def);
        }
        // mark rdf 的有条件跳转
        auto *bb = inst->get_parent();
        for (auto *rdomed_bb : bb->get_rdom_frontier()) {
            auto *terminator = rdomed_bb->get_terminator();
            if (marked.count(terminator) != 0)
                continue;
            marked.insert(terminator);
            work_list.push_back(terminator);
        }
    }

    return marked;
}

void DeadCode::sweep(Function *f,
                     const std::unordered_set<Instruction *> &marked) {
    std::vector<Instruction *> delete_list{};
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *op : bb->get_instructions()) {
            if (marked.count(op) != 0)
                continue;
            if (!op->is_br()) {
                delete_list.push_back(op);
                continue;
            }
            // rewrite with a jump to i's nearest useful post-dominator
            auto *bb = op->get_parent();
            auto *runner = bb;
            while ((runner = runner->get_irdom()) != nullptr) {
                // until irdom is useful
                bool useful = false;
                for (auto *inst : runner->get_instructions()) {
                    if (marked.count(inst) != 0) {
                        useful = true;
                        break;
                    }
                }
                if (!useful)
                    continue;
                // br 替换为向 runnner 的无条件跳转
                auto *br_inst = static_cast<BranchInst *>(op);

                // 首先维护 bb 间关系
                // 删除旧的
                auto *true_bb =
                    static_cast<BasicBlock *>(br_inst->get_operand(1));
                auto *false_bb =
                    static_cast<BasicBlock *>(br_inst->get_operand(2));
                true_bb->remove_pre_basic_block(bb);
                false_bb->remove_pre_basic_block(bb);
                bb->remove_succ_basic_block(true_bb);
                bb->remove_succ_basic_block(false_bb);
                // 增加新的
                runner->add_pre_basic_block(bb);
                bb->add_succ_basic_block(runner);

                // 更新跳转指令的操作数
                br_inst->remove_operands(0, 2);
                br_inst->add_operand(runner);
                break;
            }
        }
    }
    for (auto *inst : delete_list)
        inst->get_parent()->delete_instr(inst);
}

void DeadCode::clean(Function *f) {
    if (f->is_declaration())
        return;

    bool changed{};
    std::vector<BasicBlock *> work_list;

    std::unordered_set<BasicBlock *> visited;
    std::function<void(BasicBlock *)> post_traverse;
    post_traverse = [&](BasicBlock *i) {
        // 后序遍历
        if (visited.count(i) != 0)
            return;
        visited.insert(i);
        for (auto *bb : i->get_succ_basic_blocks()) {
            post_traverse(bb);
        }
        work_list.push_back(i);
    };

    do {
        changed = false;
        work_list.clear();
        post_traverse(f->get_entry_block());
        for (auto *i : work_list) {
            // 检查每个基本块的终止语句
            auto *terminator = i->get_terminator();
            if (!terminator->is_br())
                return;
            auto *br_inst = static_cast<BranchInst *>(terminator);
            if (br_inst->is_cond_br()) {
                if (br_inst->get_operand(1) == br_inst->get_operand(2)) {
                    // 相同的跳转目的地
                    auto *target = br_inst->get_operand(1);
                    br_inst->remove_operands(0, 2);
                    br_inst->add_operand(target);
                }
            }
            if (!br_inst->is_cond_br()) {
                changed = true;
                // i jump 到 j，可能会删除 cfg 结点，
                // 规定：只能删除 i（保证迭代有效）
                auto *j = i->get_succ_basic_blocks().front();
                if (i->get_instructions().size() == 1) {
                    // i 是空的
                    // 到 i 的跳转直接改为到 j 的跳转
                    delete_basic_block(i, j);
                } else if (j->get_pre_basic_blocks().size() == 1) {
                    // j 只有 i 一个前驱
                    // 合并这两个基本块
                    // 相当于把 i 「删除」，原有指令移到 j
                    auto &i_insts = i->get_instructions();
                    for (auto it = ++i_insts.rbegin(); it != i_insts.rend();
                         ++it) {
                        j->add_instr_begin(*it);
                    }
                    delete_basic_block(i, j);
                } else if (j->get_instructions().size() == 1 &&
                           j->get_terminator()->is_br() &&
                           static_cast<BranchInst *>(j->get_terminator())
                               ->is_cond_br()) {
                    // j 是空的，且以条件跳转结束
                    // 删除 i 和 j 之前的前驱后继关系
                    i->remove_succ_basic_block(j);
                    j->remove_pre_basic_block(i);
                    // 建立新的 i 和 j 的后继的关系
                    auto *true_bb = static_cast<BasicBlock *>(
                        j->get_terminator()->get_operand(1));
                    auto *false_bb = static_cast<BasicBlock *>(
                        j->get_terminator()->get_operand(2));
                    true_bb->add_pre_basic_block(i);
                    false_bb->add_pre_basic_block(i);
                    i->add_succ_basic_block(true_bb);
                    i->add_succ_basic_block(false_bb);
                    // 替换跳转指令
                    auto *cond = j->get_terminator()->get_operand(0);
                    i->get_terminator()->remove_operands(0, 0);
                    i->get_terminator()->add_operand(cond);
                    i->get_terminator()->add_operand(true_bb);
                    i->get_terminator()->add_operand(false_bb);
                }
            }
        }
    } while (changed);
}