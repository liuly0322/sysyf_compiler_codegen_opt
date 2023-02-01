#include "DeadCode.h"
#include <functional>
#include <vector>

using PureFunction::is_pure;
using PureFunction::markPure;
using PureFunction::store_to_alloca;

bool is_critical_inst(Instruction *inst) {
    if (inst->is_ret() ||
        (inst->is_store() && store_to_alloca(inst) == nullptr)) {
        return true;
    }
    if (inst->is_br() &&
        inst->get_parent() == inst->get_function()->get_entry_block())
        return true;
    if (inst->is_call()) {
        auto *call_inst = dynamic_cast<CallInst *>(inst);
        auto *callee = dynamic_cast<Function *>(call_inst->get_operand(0));
        return !is_pure[callee];
    }
    return false;
}

void delete_basic_block(BasicBlock *i, BasicBlock *j) {
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

void DeadCode::execute() {
    markPure(module);
    for (auto *f : module->get_functions()) {
        do {
            mark(f);
            sweep(f);
        } while (clean(f));
    }
}

void DeadCode::mark(Function *f) { marker->mark(f); }

void DeadCode::sweep(Function *f) {
    std::vector<Instruction *> delete_list{};
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (marked.count(inst) != 0)
                continue;
            if (!inst->is_br() || inst->get_num_operand() != 3) {
                delete_list.push_back(inst);
                continue;
            }
            rewriteBranchToPostDomJmp(static_cast<BranchInst *>(inst));
        }
    }
    for (auto *inst : delete_list)
        inst->get_parent()->delete_instr(inst);
}

void DeadCode::rewriteBranchToPostDomJmp(BranchInst *inst) {
    auto *bb = inst->get_parent();
    auto *bb_runner = bb;
    while ((bb_runner = bb_runner->get_irdom()) != nullptr) {
        bool runner_useful = false;
        for (auto *inst : bb_runner->get_instructions()) {
            if (marked.count(inst) != 0) {
                runner_useful = true;
                break;
            }
        }
        if (!runner_useful)
            continue;

        auto *true_bb = static_cast<BasicBlock *>(inst->get_operand(1));
        auto *false_bb = static_cast<BasicBlock *>(inst->get_operand(2));

        true_bb->remove_pre_basic_block(bb);
        false_bb->remove_pre_basic_block(bb);
        bb->remove_succ_basic_block(true_bb);
        bb->remove_succ_basic_block(false_bb);

        bb_runner->add_pre_basic_block(bb);
        bb->add_succ_basic_block(bb_runner);

        inst->remove_operands(0, 2);
        inst->add_operand(bb_runner);
        break;
    }
}

bool DeadCode::clean(Function *f) { return cleaner->clean(f); }

void Marker::mark(Function *f) {
    marked.clear();
    lval_store.clear();
    worklist.clear();

    markBaseCase(f);

    while (!worklist.empty()) {
        auto *inst = worklist.front();
        worklist.pop_front();

        if (inst->is_alloca())
            markLocalArrayStore(static_cast<AllocaInst *>(inst));

        for (auto *op : inst->get_operands())
            markOperand(op);

        markRDF(inst->get_parent());
    }
}

void Marker::markInst(Instruction *inst) {
    if (marked.count(inst) == 0) {
        marked.insert(inst);
        worklist.push_back(inst);
    }
}

void Marker::markBaseCase(Function *f) {
    for (auto *bb : f->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (is_critical_inst(inst))
                markInst(inst);
            else if (inst->is_store())
                lval_store[store_to_alloca(inst)].insert(inst);
        }
    }
}

void Marker::markLocalArrayStore(AllocaInst *inst) {
    for (auto *store_inst : lval_store[inst])
        markInst(store_inst);
    lval_store.erase(inst);
}

void Marker::markOperand(Value *op) {
    if (auto *op_inst = dynamic_cast<Instruction *>(op))
        markInst(op_inst);
    if (auto *op_bb = dynamic_cast<BasicBlock *>(op))
        markInst(op_bb->get_terminator());
}

void Marker::markRDF(BasicBlock *bb) {
    for (auto *rdomed_bb : bb->get_rdom_frontier())
        markInst(rdomed_bb->get_terminator());
}

bool Cleaner::clean(Function *f) {
    if (f->is_declaration())
        return false;

    f_ = f;
    ever_cleaned = false;

    do {
        cleaned = false;
        postTraverseBasicBlocks();
    } while (cleaned);
    cleanUnreachable();

    return ever_cleaned;
}

void Cleaner::postTraverseBasicBlocks() {
    auto worklist = std::vector<BasicBlock *>{};
    visited.clear();

    std::function<void(BasicBlock *)> collectPostOrder = [&](BasicBlock *i) {
        if (visited.count(i) != 0)
            return;
        visited.insert(i);
        for (auto *bb : i->get_succ_basic_blocks()) {
            collectPostOrder(bb);
        }
        worklist.push_back(i);
    };
    collectPostOrder(f_->get_entry_block());

    for (auto *bb : worklist)
        visitBasicBlock(bb);
}

void Cleaner::visitBasicBlock(BasicBlock *i) {
    auto *terminator = i->get_terminator();
    if (!terminator->is_br())
        return;
    auto *br_inst = static_cast<BranchInst *>(terminator);

    if (br_inst->is_cond_br() && !eliminateRedundantBranches(br_inst))
        return;

    auto *j = i->get_succ_basic_blocks().front();
    if (i->get_num_of_instr() == 1) {
        eliminateEmptyBlock(i, j);
    } else if (j->get_pre_basic_blocks().size() == 1) {
        combineBasicBlocks(i, j);
    } else if (auto *j_br =
                   dynamic_cast<BranchInst *>(j->get_instructions().front())) {
        if (j_br->is_cond_br())
            hoistBranches(i, j);
    }
}

bool Cleaner::eliminateRedundantBranches(Instruction *br_inst) {
    if (br_inst->get_operand(1) != br_inst->get_operand(2))
        return false;

    auto *target = br_inst->get_operand(1);
    br_inst->remove_operands(0, 2);
    br_inst->add_operand(target);
    return true;
}

void Cleaner::eliminateEmptyBlock(BasicBlock *i, BasicBlock *j) {
    if (i == f_->get_entry_block())
        return;

    // 要求 j 不能有同时含有 i 和 i 前驱作为操作数的 phi 指令
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

    cleaned = true, ever_cleaned = true;
    delete_basic_block(i, j);
}

void Cleaner::combineBasicBlocks(BasicBlock *i, BasicBlock *j) {
    if (i == f_->get_entry_block())
        return;

    cleaned = true, ever_cleaned = true;

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
}

void Cleaner::connectHoisting(BasicBlock *i, BasicBlock *j, BasicBlock *dst) {
    i->add_succ_basic_block(dst);
    dst->add_pre_basic_block(i);

    // 如果有 j 的 phi operand，给 i 也来一份
    for (auto *inst : dst->get_instructions()) {
        if (!inst->is_phi())
            break;
        for (auto index = 1U; index < inst->get_num_operand(); index += 2) {
            if (inst->get_operand(index) == j) {
                inst->add_operand(inst->get_operand(index - 1));
                inst->add_operand(i);
                break;
            }
        }
    }
}

void Cleaner::hoistBranches(BasicBlock *i, BasicBlock *j) {
    cleaned = true, ever_cleaned = true;

    auto *i_jmp = i->get_terminator();
    auto *j_br = dynamic_cast<BranchInst *>(j->get_terminator());

    auto *cond = j_br->get_operand(0);
    auto *true_bb = static_cast<BasicBlock *>(j_br->get_operand(1));
    auto *false_bb = static_cast<BasicBlock *>(j_br->get_operand(2));

    i_jmp->remove_operands(0, 0);
    i_jmp->add_operand(cond);
    i_jmp->add_operand(true_bb);
    i_jmp->add_operand(false_bb);

    i->remove_succ_basic_block(j);
    j->remove_pre_basic_block(i);

    connectHoisting(i, j, true_bb);
    connectHoisting(i, j, false_bb);
}

void Cleaner::cleanUnreachable() {
    auto delete_list = std::vector<BasicBlock *>{};
    for (auto *bb : f_->get_basic_blocks()) {
        if (visited.count(bb) == 0)
            delete_list.push_back(bb);
    }
    for (auto *bb : delete_list)
        f_->remove(bb);
    ever_cleaned |= !delete_list.empty();
}
