#ifndef SYSYF_ACTIVEVAR_H
#define SYSYF_ACTIVEVAR_H

#include "Pass.h"

class ActiveVar : public Pass {
  public:
    explicit ActiveVar(Module *module) : Pass(module) {}
    void execute() final;
    void checkFun(Function *f);
    [[nodiscard]] std::string get_name() const override { return name; }
    void dump();

  private:
    const std::string name = "ActiveVar";

    bool changed;
    std::set<Value *> def;
    std::set<Value *> use;
    std::map<BasicBlock *, std::set<Value *>> IN;
    std::map<BasicBlock *, std::set<Value *>> OUT;

    static bool localOp(Value *value) {
        return dynamic_cast<Instruction *>(value) != nullptr ||
               dynamic_cast<Argument *>(value) != nullptr;
    }
    static void addPhiFrom(std::set<Value *> &set, BasicBlock *dst,
                           BasicBlock *src = nullptr);
    void calcDefUse(BasicBlock *bb);
    std::set<Value *> calcIn(BasicBlock *bb);
    std::set<Value *> calcOut(BasicBlock *bb);
    void calcInOut(Function *f);
};

bool ValueCmp(Value *a, Value *b);
std::vector<Value *> sort_by_name(std::set<Value *> &val_set);
const std::string avdump = "active_var.out";

#endif // SYSYF_ACTIVEVAR_H
