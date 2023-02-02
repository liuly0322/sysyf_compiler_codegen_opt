#include "ActiveVar.h"
#include <fstream>

#include <algorithm>

void ActiveVar::addPhiFrom(std::set<Value *> &set, BasicBlock *dst,
                           BasicBlock *src) {
    for (auto *inst : dst->get_instructions()) {
        if (!inst->is_phi())
            break;
        const int phi_size = inst->get_num_operand() / 2;
        for (int i = 0; i < phi_size; i++) {
            auto *block =
                dynamic_cast<BasicBlock *>(inst->get_operand(2 * i + 1));
            if (src != nullptr && block != src)
                continue;
            auto *active_var = inst->get_operand(2 * i);
            if (localOp(active_var))
                set.insert(active_var);
        }
    }
}

std::set<Value *> ActiveVar::calcOut(BasicBlock *bb) {
    auto out_bb = OUT[bb];
    for (auto *succ_bb : bb->get_succ_basic_blocks()) {
        for (auto *value : IN[succ_bb]) {
            out_bb.insert(value);
        }
        addPhiFrom(out_bb, succ_bb, bb);
    }
    return out_bb;
}

void ActiveVar::calcDefUse(BasicBlock *bb) {
    def.clear();
    use.clear();
    for (auto *inst : bb->get_instructions()) {
        if (inst->is_phi()) {
            def.insert(inst);
            continue;
        }
        auto operands = inst->get_operands();
        for (auto *op : operands) {
            if (localOp(op) && def.count(op) == 0)
                use.insert(op);
        }
        if (use.count(inst) == 0)
            def.insert(inst);
    }
}

std::set<Value *> ActiveVar::calcIn(BasicBlock *bb) {
    calcDefUse(bb);
    std::set<Value *> in_bb(OUT[bb]);
    for (auto *instr : def)
        if (in_bb.count(instr) != 0)
            in_bb.erase(instr);
    for (auto *instr : use)
        in_bb.insert(instr);
    if (sort_by_name(in_bb) != sort_by_name(IN[bb]))
        changed = true;
    return in_bb;
}

void ActiveVar::calcInOut(Function *f) {
    auto blocks = f->get_basic_blocks();
    for (auto *bb : blocks) {
        OUT[bb] = calcOut(bb);
        IN[bb] = calcIn(bb);
    }
}

void ActiveVar::checkFun(Function *f) {
    IN.clear();
    OUT.clear();
    do {
        changed = false;
        calcInOut(f);
    } while (changed);

    auto blocks = f->get_basic_blocks();
    for (auto *bb : blocks) {
        addPhiFrom(IN[bb], bb);
        bb->set_live_in(std::move(IN[bb]));
        bb->set_live_out(std::move(OUT[bb]));
    }
}

void ActiveVar::execute() {
    module->set_print_name();

    for (auto *f : module->get_functions()) {
        if (!f->is_declaration())
            checkFun(f);
    }

    dump();
}

void ActiveVar::dump() {
    std::fstream f;
    f.open(avdump, std::ios::out);
    for (auto &func : module->get_functions()) {
        for (auto &bb : func->get_basic_blocks()) {
            f << bb->get_name() << std::endl;
            auto &in = bb->get_live_in();
            auto &out = bb->get_live_out();
            auto sorted_in = sort_by_name(in);
            auto sorted_out = sort_by_name(out);
            f << "in:\n";
            for (auto *in_v : sorted_in) {
                f << in_v->get_name() << " ";
            }
            f << "\n";
            f << "out:\n";
            for (auto *out_v : sorted_out) {
                f << out_v->get_name() << " ";
            }
            f << "\n";
        }
    }
    f.close();
}

bool ValueCmp(Value *a, Value *b) { return a->get_name() < b->get_name(); }

std::vector<Value *> sort_by_name(std::set<Value *> &val_set) {
    std::vector<Value *> result;
    result.assign(val_set.begin(), val_set.end());
    std::sort(result.begin(), result.end(), ValueCmp);
    return result;
}