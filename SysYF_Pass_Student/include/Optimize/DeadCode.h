#ifndef SYSYF_DEADCODE_H
#define SYSYF_DEADCODE_H

#include "Pass.h"
#include "PureFunction.h"
#include <set>

class DeadCode : public Pass {
  public:
    explicit DeadCode(Module *module) : Pass(module) {}
    void execute() final;
    static bool is_critical_inst(Instruction *inst);
    void mark(Function *f);
    void sweep(Function *f);
    static bool clean(Function *f);
    [[nodiscard]] std::string get_name() const override { return name; }

  private:
    const std::string name = "DeadCode";
    std::set<Instruction *> marked;
};

#endif // SYSYF_DEADCODE_H
