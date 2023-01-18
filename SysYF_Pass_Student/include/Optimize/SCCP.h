#ifndef SYSYF_SCCP_H
#define SYSYF_SCCP_H

#include "Pass.h"

class SCCP : public Pass {
  public:
    explicit SCCP(Module *module) : Pass(module) {}
    void execute() final;
    static void sccp(Function *f, Module *module);
    [[nodiscard]] std::string get_name() const override { return name; }

  private:
    const std::string name = "SCCP";
};

#endif // SYSYF_SCCP_H
