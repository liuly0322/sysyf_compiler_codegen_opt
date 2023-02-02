#ifndef SYSYF_CSE_H
#define SYSYF_CSE_H

#include "Pass.h"
#include "PureFunction.h"
#include <map>
#include <set>
#include <vector>

struct Expression {
    std::set<Instruction *> source;
    Instruction::OpID type;
    Instruction *inst;
    std::vector<Value *> &operands;
    explicit Expression(Instruction *inst)
        : type(inst->get_instr_type()), inst(inst),
          operands(inst->get_operands()) {
        source.insert(inst);
    }
    bool operator==(const Expression &expr) const {
        if (type != expr.type) {
            return false;
        }
        if (operands.size() != expr.operands.size()) {
            return false;
        }
        // Cmp
        if (inst->is_cmp()) {
            auto *cmpInst1 = dynamic_cast<CmpInst *>(inst);
            auto *cmpInst2 = dynamic_cast<CmpInst *>(expr.inst);
            if (cmpInst1->get_cmp_op() != cmpInst2->get_cmp_op()) {
                return false;
            }
        }
        if (inst->is_fcmp()) {
            auto *fcmpInst1 = dynamic_cast<FCmpInst *>(inst);
            auto *fcmpInst2 = dynamic_cast<FCmpInst *>(expr.inst);
            if (fcmpInst1->get_cmp_op() != fcmpInst2->get_cmp_op()) {
                return false;
            }
        }
        for (auto i = 0U; i < operands.size(); ++i) {
            auto *op1 = operands[i];
            auto *op2 = expr.operands[i];
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
  public:
    explicit CSE(Module *m) : Pass(m){};
    void execute() final;
    [[nodiscard]] std::string get_name() const override { return name; }

  private:
    static Value *findOrigin(Value *val);
    static Instruction *isAppear(Instruction *inst,
                                 std::vector<Instruction *> &insts, int index);
    static bool isKill(Instruction *inst1, Instruction *inst2);
    static bool isKill(Instruction *inst, std::vector<Instruction *> &insts,
                       unsigned index);
    static bool isStoreWithDifferentIndex(Instruction *inst1,
                                          Instruction *inst2);

    void forwardStore();
    void deleteForward();

    void localCSE(Function *fun);
    void globalCSE(Function *fun);
    void calcGenKill(Function *fun);
    void calcAvailable(Function *fun);
    void calcGen(Function *fun);
    void calcKill(Function *fun);
    void calcInOut(Function *fun);
    void findSource(Function *fun);
    void replaceSubExpr(Function *fun);
    static Instruction *findReplacement(BasicBlock *bb,
                                        std::set<Instruction *> &source);
    void deleteInstr();
    static bool isOptimizable(Instruction *inst) {
        if (inst->is_void() || inst->is_alloca())
            return false;
        if (inst->is_call()) {
            auto *callee = dynamic_cast<Function *>(inst->get_operand(0));
            return PureFunction::is_pure[callee];
        }
        return true;
    }

    const std::string name = "CSE";
    std::map<BasicBlock *, std::vector<bool>> GEN;
    std::map<BasicBlock *, std::vector<bool>> KILL;
    std::map<BasicBlock *, std::vector<bool>> IN;
    std::map<BasicBlock *, std::vector<bool>> OUT;
    std::vector<Expression> available;
    std::vector<Instruction *> delete_list;
    std::vector<std::pair<Instruction *, Value *>> forwarded_load;
};

#endif // SYSYF_CSE_H