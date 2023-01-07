#ifndef SYSYF_CONSTANTANDREWRITE_H
#define SYSYF_CONSTANTANDREWRITE_H

#include "Module.h"
#include "Pass.h"

class ConstantAndRewrite : public Pass {
  public:
    ConstantAndRewrite(Module *module) : Pass(module) {}
    void execute() final;
    const std::string get_name() const override { return name; }

  private:
    const std::string name = "ConstantAndRewrite";
};

#endif // SYSYF_CONSTANTANDREWRITE_H
