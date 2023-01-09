#include "CSE.h"
#include "BasicBlock.h"
#include "Function.h"
#include "Pass.h"
#include <algorithm>
#include <iostream>
#include <ostream>
#include <vector>

void CSE::execute() {
    for (auto *fun : module->get_functions()) {
        if (fun->get_basic_blocks().empty()) {
            continue;
        }
        do {
            localCSE(fun);
            globalCSE(fun);
        } while (!delete_list.empty());
    }
}

Instruction *CSE::isAppear(BasicBlock *bb, Instruction *inst) {
    for (auto *instr : bb->get_instructions()) {
        if (!isOptmized(inst)) {
            continue;
        }
        if (instr == inst) {
            return nullptr;
        }
        if (Expression(inst) == Expression(instr)) {
            return instr;
        }
    }
    return nullptr;
}

void CSE::localCSE(Function *fun) {
    do {
        delete_list.clear();
        for (auto *bb : fun->get_basic_blocks()) {
            for (auto *inst : bb->get_instructions()) {
                auto *preInst = isAppear(bb, inst);
                if (preInst != nullptr) {
                    delete_list.push_back(inst);
                    inst->replace_all_use_with(preInst);
                }
            }
        }
        delete_instr();
    } while (!delete_list.empty());
}

void CSE::globalCSE(Function *fun) {
    delete_list.clear();
    calcGenKill(fun);
    calcInOut(fun);
    findSource(fun);
    replaceSubExpr(fun);
}

void CSE::calcGenKill(Function *fun) {
    // Get available expressions
    available.clear();
    for (auto bb : fun->get_basic_blocks()) {
        for (auto inst : bb->get_instructions()) {
            if (!isOptmized(inst)) {
                continue;
            }
            Expression expr(inst);
            if (std::find(available.begin(), available.end(), expr) != available.end()) {
                continue;
            }
            available.push_back(expr);
        }
    }

    // Get Gen
    GEN.clear();
    for (auto *bb : fun->get_basic_blocks()) {
        std::vector<bool> gen(available.size(), false);
        for (auto *inst : bb->get_instructions()) {
            if (!isOptmized(inst)) {
                continue;
            }
            auto index = std::find(available.begin(), available.end(), Expression(inst)) - available.begin();
            gen[index] = true;
        }
        GEN.insert({bb, gen});
    }

    // Get Kill
    KILL.clear();
    for (auto *bb : fun->get_basic_blocks()) {
        std::vector<bool> kill(available.size(), false);
        for (auto *inst : bb->get_instructions()) {
            if (inst->is_void()) {
                continue;
            }
            for (auto i = 0; i < available.size(); ++i) {
                auto &operands = available[i].operands;
                if (std::find(operands.begin(), operands.end(), inst) != operands.end()) {
                    kill[i] = true;
                }
            }
        }
        KILL.insert({bb, kill});
    }
}

void CSE::calcInOut(Function *fun) {
    // initialize
    std::vector<bool> phi(available.size(), false);
    std::vector<bool> U(available.size(), true);
    IN.clear();
    OUT.clear();
    IN.insert({fun->get_entry_block(), phi});
    for (auto bb : fun->get_basic_blocks()) {
        OUT.insert({bb, U});
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto *bb : fun->get_basic_blocks()) {
            // calculate IN
            if (bb != fun->get_entry_block()) {
                std::vector<bool> in(U);
                for (auto *pre : bb->get_pre_basic_blocks()) {
                    for (auto i = 0; i < available.size(); i++) {
                        in[i] = in[i] && OUT[pre][i];
                    }
                }
                IN.insert_or_assign(bb, in);
            }

            // calculate OUT
            std::vector<bool> preOut = OUT[bb];
            for (auto i = 0; i < available.size(); ++i) {
                OUT[bb][i] = GEN[bb][i] || (IN[bb][i] && !KILL[bb][i]);
            }
            if (preOut != OUT[bb]) {
                changed = true;
            }
        }
    }
}

void CSE::findSource(Function *fun) {
    for (auto bb : fun->get_basic_blocks()) {
        for (auto inst : bb->get_instructions()) {
            if (!isOptmized(inst)) {
                continue;
            }
            auto it = std::find(available.begin(), available.end(), Expression(inst));
            auto index = it - available.begin();
            auto expr = *it;
            if (expr.source.count(inst) == 0) {
                if (!IN[bb][index]) {
                    expr.source.insert(inst);
                }
            }
        }
    }
}

BasicBlock *traverse(BasicBlock *curbb, BasicBlock *dstbb, BasicBlock *prebb, std::set<BasicBlock *> &visited) {
    if (visited.count(curbb)) {
        return nullptr;
    }
    visited.insert(curbb);
    if (curbb = dstbb) {
        return prebb;
    }
    prebb = curbb;
    for (auto *succ_bb : curbb->get_succ_basic_blocks()) {
        auto *ret = traverse(succ_bb, dstbb, prebb, visited);
        return ret;
    }
    return nullptr;
}

BasicBlock *CSE::isReach(BasicBlock *bb, Instruction *inst) {
    auto *curbb = inst->get_parent();
    std::set<BasicBlock *> visited;
    return traverse(curbb, bb, curbb, visited);
};

void CSE::replaceSubExpr(Function *fun) {
    for (auto *bb : fun->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (!isOptmized(inst)) {
                continue;
            }
            auto it = std::find(available.begin(), available.end(), Expression(inst));
            auto expr = *it;
            auto index = it - available.begin();
            if (!IN[bb][index] || expr.source.count(inst)) {
                continue;
            }
            if (expr.source.size() == 1) {
                auto repInst = *expr.source.begin();
                delete_list.push_back(inst);
                inst->replace_all_use_with(repInst);
            } else {
                delete_list.push_back(inst);
                auto *phiInst = PhiInst::create_phi(inst->get_type(), bb);
                bb->add_instr_begin(phiInst);
                inst->replace_all_use_with(phiInst);
                for (auto *instr : expr.source) {
                    auto *srcbb = isReach(bb, instr);
                    if (srcbb != nullptr) {
                        phiInst->add_phi_pair_operand(instr, srcbb);
                    }
                }
            }
        }
    }
    delete_instr();
}

void CSE::delete_instr() {
    for (auto *inst : delete_list) {
        inst->get_parent()->delete_instr(inst);
    }
}