#ifndef SYSYF_CSE_H
#define SYSYF_CSE_H

#include "Module.h"
#include "Pass.h"

class CSE : public Pass {
  public:
    CSE(Module *module) : Pass(module) {}
    void execute() final;
    const std::string get_name() const override { return name; }

  private:
    const std::string name = "CSE";
};

#endif // SYSYF_CSE_H
