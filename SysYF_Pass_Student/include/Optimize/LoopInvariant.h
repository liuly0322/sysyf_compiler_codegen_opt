#ifndef MHSJ_LOOPINVARIANT_H
#define MHSJ_LOOPINVARIANT_H

#include "Module.h"
#include "Pass.h"
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <vector>

class LoopInvariant : public Pass {
  public:
    explicit LoopInvariant(Module *module) : Pass(module) {}
    void execute() final;
    [[nodiscard]] std::string get_name() const override { return name; }

  private:
    std::string name = "LoopInvariant";
};

#endif