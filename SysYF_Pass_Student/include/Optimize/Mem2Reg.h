#ifndef SYSYF_MEM2REG_H
#define SYSYF_MEM2REG_H

#include "IRBuilder.h"
#include "Pass.h"
#include "PureFunction.h"

class Mem2Reg : public Pass {
  private:
    Function *func_;
    IRBuilder *builder;
    const std::string name = "Mem2Reg";

  public:
    explicit Mem2Reg(Module *m) : Pass(m) {}
    ~Mem2Reg() = default;
    void execute() final;
    void genPhi();
    void insideBlockForwarding();
    void valueForwarding(BasicBlock *bb);
    void removeAlloc();
    [[nodiscard]] std::string get_name() const override { return name; }

    static bool isVarOp(Instruction *inst, bool includeGlobal = false) {
        Value *lvalue{};
        if (inst->is_store()) {
            lvalue = static_cast<StoreInst *>(inst)->get_lval();
        } else if (inst->is_load()) {
            lvalue = static_cast<LoadInst *>(inst)->get_lval();
        } else {
            return false;
        }
        auto isVar = dynamic_cast<GetElementPtrInst *>(lvalue) == nullptr;
        if (!includeGlobal) {
            isVar &= dynamic_cast<GlobalVariable *>(lvalue) == nullptr;
        }
        return isVar;
    }

    static std::vector<Value *> collectValueDefine(BasicBlock *bb) {
        auto define_var = std::vector<Value *>{};
        for (auto *inst : bb->get_instructions()) {
            if (inst->is_phi()) {
                auto *lvalue = dynamic_cast<PhiInst *>(inst)->get_lval();
                define_var.push_back(lvalue);
            } else if (inst->is_store()) {
                if (!isVarOp(inst))
                    continue;
                auto *lvalue = dynamic_cast<StoreInst *>(inst)->get_lval();
                define_var.push_back(lvalue);
            }
        }
        return define_var;
    }
};

#endif // SYSYF_MEM2REG_H