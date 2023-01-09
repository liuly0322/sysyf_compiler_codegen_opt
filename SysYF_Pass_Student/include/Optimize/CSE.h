#ifndef SYSYF_CSE_H
#define SYSYF_CSE_H

#include "BasicBlock.h"
#include "Constant.h"
#include "Function.h"
#include "GlobalVariable.h"
#include "Instruction.h"
#include "Module.h"
#include "Pass.h"
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Expression {
  public:
    std::set<Instruction *> source;
    Instruction::OpID type;
    std::vector<Value *> &operands;
    Expression(Instruction *inst) : type(inst->get_instr_type()), operands(inst->get_operands()) {
        source.insert(inst);
    }
    bool operator==(const Expression &expr) const {
        if (type != expr.type) {
            return false;
        }
        if (operands.size() != expr.operands.size()) {
            return false;
        }
        for (auto i = 0; i < operands.size(); ++i) {
            auto *op1 = operands[i], *op2 = expr.operands[i];
            // Int Constant
            auto *iconst1 = dynamic_cast<ConstantInt *>(op1);
            auto *iconst2 = dynamic_cast<ConstantInt *>(op2);
            if (iconst1 != nullptr && iconst2 != nullptr) {
                if (iconst1->get_value() != iconst2->get_value()) {
                    return false;
                }
                continue;
            }
            // Float Constant
            auto *fconst1 = dynamic_cast<ConstantFloat *>(op1);
            auto *fconst2 = dynamic_cast<ConstantFloat *>(op2);
            if (fconst1 != nullptr && fconst2 != nullptr) {
                if (fconst1->get_value() != fconst2->get_value()) {
                    return false;
                }
                continue;
            }
            // Other Case: Function / BasicBlock / Instruction
            if (op1 != op2) {
                return false;
            }
        }
        return true;
    }
};

class CSE : public Pass {
  private:
    std::map<BasicBlock *, std::vector<bool>> GEN;
    std::map<BasicBlock *, std::vector<bool>> KILL;
    std::map<BasicBlock *, std::vector<bool>> IN;
    std::map<BasicBlock *, std::vector<bool>> OUT;
    std::vector<Expression> available;
    std::vector<Instruction *> delete_list;
    const std::string name = "CSE";

  public:
    CSE(Module *m) : Pass(m){};
    void execute() final;
    Instruction *isAppear(BasicBlock *, Instruction *);
    void localCSE(Function *fun);
    void globalCSE(Function *fun);
    void calcGenKill(Function *fun);
    void calcInOut(Function *fun);
    void findSource(Function *fun);
    void replaceSubExpr(Function *fun);
    void delete_instr();
    BasicBlock *isReach(BasicBlock *bb, Instruction *inst);

    const std::string get_name() const override { return name; }
    bool isOptmized(Instruction *inst) {
        return !(inst->is_void() || inst->is_alloca() || inst->is_load() || inst->is_cmp() || inst->is_call() ||
                 inst->is_zext());
    }
};

#endif // SYSYF_CSE_H