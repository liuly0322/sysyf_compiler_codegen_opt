#ifndef SYSYF_MEM2REG_H
#define SYSYF_MEM2REG_H

#include "IRBuilder.h"
#include "Pass.h"
#include "PureFunction.h"

class Mem2Reg : public Pass {
  private:
    Function *func_;
    IRBuilder *builder;
    std::map<BasicBlock *, std::vector<Value *>> define_var;
    const std::string name = "Mem2Reg";
    std::map<Value *, Value *> lvalue_connection;
    std::set<Value *> no_union_set;

  public:
    explicit Mem2Reg(Module *m) : Pass(m) {}
    ~Mem2Reg() = default;
    void execute() final;
    void genPhi(); // 生成φ函数
    void insideBlockForwarding();
    void valueDefineCounting();
    void valueForwarding(BasicBlock *bb);
    void removeAlloc();
    void phiStatistic();
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
};

#endif // SYSYF_MEM2REG_H