#include "DeadCode.h"
#include <functional>
#include <vector>

using PureFunction::is_pure;

static inline void delete_basic_block(BasicBlock *i, BasicBlock *j) {
    auto &use_list = i->get_use_list();
    while (!use_list.empty()) {
        auto &use = use_list.front();
        use_list.pop_front();
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
    i->erase_from_parent();
}

bool DeadCode::is_critical_inst(Instruction *inst) {
    if (inst->is_ret() ||
        (inst->is_store() && PureFunction::store_to_alloca(inst) == nullptr)) {
        return true;
    }
    if (inst->is_br() &&
        inst->get_parent() == inst->get_function()->get_entry_block())
        return true;
    if (inst->is_call()) {
        // 只标记非纯函数的调用
        auto *call_inst = dynamic_cast<CallInst *>(inst);
        auto *callee = dynamic_cast<Function *>(call_inst->get_operand(0));
        return !is_pure[callee];
    }
    return false;
}

void DeadCode::execute() {
    PureFunction::markPure(module);
    for (auto *f : module->get_functions()) {
        do {
            mark(f);
            sweep(f);
        } while (clean(f));
    }
}

void DeadCode::mark(Function *f) {
    marked.clear();
    std::vector<Instruction *> worklist{};

    // 初始标记 critical 变量
    std::map<AllocaInst *, std::set<StoreInst *>> lval_store{};
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (is_critical_inst(inst)) {
                marked.insert(inst);
                worklist.push_back(inst);
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
    for (auto i = 0U; i < worklist.size(); i++) {
        auto *inst = worklist[i];
        // 如果数组左值 critical，则标记所有对数组的 store 也为 critical
        if (inst->is_alloca()) {
            auto *lval = static_cast<AllocaInst *>(inst);
            for (auto *store_inst : lval_store[lval]) {
                marked.insert(store_inst);
                worklist.push_back(store_inst);
            }
            lval_store.erase(lval);
        }
        // mark 所有 operand
        for (auto *op : inst->get_operands()) {
            auto *def = dynamic_cast<Instruction *>(op);
            auto *bb = dynamic_cast<BasicBlock *>(op);
            if (def != nullptr && marked.count(def) == 0) {
                marked.insert(def);
                worklist.push_back(def);
            }
            if (bb != nullptr) {
                auto *terminator = bb->get_terminator();
                if (marked.count(terminator) == 0) {
                    marked.insert(terminator);
                    worklist.push_back(terminator);
                }
            }
        }
        // mark rdf 的跳转
        auto *bb = inst->get_parent();
        for (auto *rdomed_bb : bb->get_rdom_frontier()) {
            auto *terminator = rdomed_bb->get_terminator();
            if (marked.count(terminator) != 0)
                continue;
            marked.insert(terminator);
            worklist.push_back(terminator);
        }
    }
}

void DeadCode::sweep(Function *f) {
    std::vector<Instruction *> delete_list{};
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *op : bb->get_instructions()) {
            if (marked.count(op) != 0)
                continue;
            if (!op->is_br() || op->get_num_operand() != 3) {
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

bool DeadCode::clean(Function *f) {
    if (f->is_declaration())
        return false;

    bool changed{};
    bool ever_changed{};

    auto visit = [&](BasicBlock *i) {
        // 先确保存在 br 语句
        auto *terminator = i->get_terminator();
        if (!terminator->is_br())
            return;
        auto *br_inst = static_cast<BranchInst *>(terminator);
        if (br_inst->is_cond_br()) {
            // 先尝试替换为 jmp
            if (br_inst->get_operand(1) != br_inst->get_operand(2)) {
                return;
            }
            // 相同的跳转目的地
            auto *target = br_inst->get_operand(1);
            br_inst->remove_operands(0, 2);
            br_inst->add_operand(target);
        }

        // 下面是对 i jmp 到 j 的优化
        auto *j = i->get_succ_basic_blocks().front();
        if (i->get_num_of_instr() == 1) {
            // i 是空的，到 i 的跳转直接改为到 j 的跳转

            // 会删除 i, 所以保证 i 不是入口基本块
            if (i == f->get_entry_block())
                return;

            // 这里要求 j 不能有同时含有 i 和 i 前驱作为操作数的 phi 指令
            const std::set<Value *> pres{i->get_pre_basic_blocks().begin(),
                                         i->get_pre_basic_blocks().end()};
            bool has_both_pre_and_i = false;

            for (auto *j_inst : j->get_instructions()) {
                auto *phi_inst = dynamic_cast<PhiInst *>(j_inst);
                if (phi_inst == nullptr)
                    break;

                auto has_pre = false;
                auto has_i = false;
                for (auto *phi_operand : phi_inst->get_operands()) {
                    if (pres.count(phi_operand) != 0)
                        has_pre = true;
                    if (phi_operand == i)
                        has_i = true;
                }

                if (has_pre && has_i) {
                    has_both_pre_and_i = true;
                    break;
                }
            }
            if (has_both_pre_and_i)
                return;

            changed = true, ever_changed = true;
            delete_basic_block(i, j);
        } else if (j->get_pre_basic_blocks().size() == 1) {
            // j 只有 i 一个前驱，合并这两个基本块

            // 会删除 i, 所以保证 i 不是入口基本块
            if (i == f->get_entry_block())
                return;

            changed = true, ever_changed = true;
            // 相当于把 i 「删除」，原有指令移到 j
            // 这里 j 可能有无用的 phi 指令，删除即可
            while (j->get_instructions().front()->is_phi()) {
                auto *phi_inst = j->get_instructions().front();
                phi_inst->replace_all_use_with(phi_inst->get_operand(0));
                j->delete_instr(phi_inst);
            }
            auto &i_insts = i->get_instructions();
            for (auto it = ++i_insts.rbegin(); it != i_insts.rend(); ++it) {
                j->add_instr_begin(*it);
                (*it)->set_parent(j);
            }
            delete_basic_block(i, j);
        } else if (j->get_num_of_instr() == 1) {
            // j 是空基本块
            // 如果 j 末尾是分支，则可以提升分支
            auto *j_terminator = j->get_terminator();
            auto *j_br = dynamic_cast<BranchInst *>(j_terminator);
            if (j_br == nullptr)
                return;
            if (!j_br->is_cond_br())
                return;

            changed = true, ever_changed = true;
            auto *cond = j_br->get_operand(0);
            auto *true_bb = static_cast<BasicBlock *>(j_br->get_operand(1));
            auto *false_bb = static_cast<BasicBlock *>(j_br->get_operand(2));
            terminator->remove_operands(0, 0);
            terminator->add_operand(cond);
            terminator->add_operand(true_bb);
            terminator->add_operand(false_bb);
            i->remove_succ_basic_block(j);
            j->remove_pre_basic_block(i);

            auto connect = [&](BasicBlock *dst) {
                i->add_succ_basic_block(dst);
                dst->add_pre_basic_block(i);

                // 如果有 j 的 phi operand，给 i 也来一份
                for (auto *inst : dst->get_instructions()) {
                    if (!inst->is_phi())
                        break;
                    for (auto index = 1U; index < inst->get_num_operand();
                         index += 2) {
                        if (inst->get_operand(index) == j) {
                            inst->add_operand(inst->get_operand(index - 1));
                            inst->add_operand(i);
                            break;
                        }
                    }
                }
            };
            connect(true_bb);
            connect(false_bb);
        }
    };

    auto visited = std::set<BasicBlock *>{};
    auto post_traverse = [&]() {
        auto worklist = std::vector<BasicBlock *>{};
        std::function<void(BasicBlock *)> post_traverse_recur;
        post_traverse_recur = [&](BasicBlock *i) {
            if (visited.count(i) != 0)
                return;
            visited.insert(i);
            for (auto *bb : i->get_succ_basic_blocks()) {
                post_traverse_recur(bb);
            }
            worklist.push_back(i);
        };
        post_traverse_recur(f->get_entry_block());
        for (auto *bb : worklist) {
            visit(bb);
        }
    };

    do {
        changed = false;
        visited.clear();
        post_traverse();
    } while (changed);

    auto delete_list = std::vector<BasicBlock *>{};
    for (auto *bb : f->get_basic_blocks()) {
        if (visited.count(bb) != 0)
            continue;
        // 不可达
        delete_list.push_back(bb);
        // 删除 bb 后继与它的前驱后继关系
        for (auto *succ : bb->get_succ_basic_blocks()) {
            succ->remove_pre_basic_block(bb);
        }
        bb->get_succ_basic_blocks().clear();
    }

    for (auto *bb : delete_list) {
        f->remove(bb);
    }

    return ever_changed || !delete_list.empty();
}