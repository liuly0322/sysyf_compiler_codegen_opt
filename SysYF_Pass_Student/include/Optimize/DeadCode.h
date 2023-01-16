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
    void mark(Function *f);
    void sweep(Function *f);
    static bool clean(Function *f);
    const std::string get_name() const override { return name; }

  private:
    const std::string name = "DeadCode";
    std::unordered_set<Instruction *> marked;
};

#endif // SYSYF_DEADCODE_H
