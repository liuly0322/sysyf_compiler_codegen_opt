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
    static bool is_critical_inst(Instruction *inst);
    static std::unordered_set<Instruction *> mark(Function *f);
    static void sweep(Function *f,
                      const std::unordered_set<Instruction *> &marked);
    static void clean(Function *f);
    const std::string get_name() const override { return name; }

  private:
    const std::string name = "DeadCode";
};

#endif // SYSYF_DEADCODE_H
