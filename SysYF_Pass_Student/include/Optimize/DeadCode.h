#ifndef SYSYF_DEADCODE_H
#define SYSYF_DEADCODE_H

#include "Module.h"
#include "Pass.h"
#include <unordered_map>
#include <unordered_set>

class DeadCode : public Pass {
  public:
    DeadCode(Module *module) : Pass(module) {}
    void execute() final;
    void markPure();
    bool is_critical_inst(Instruction *inst);
    std::unordered_set<Instruction *> mark(Function *f);
    void sweep(Function *f, const std::unordered_set<Instruction *> &marked);
    void clean(Function *f);
    static bool markPureInside(Function *f);
    const std::string get_name() const override { return name; }

  private:
    std::unordered_map<Function *, bool> is_pure;
    const std::string name = "DeadCode";
};

#endif // SYSYF_DEADCODE_H
