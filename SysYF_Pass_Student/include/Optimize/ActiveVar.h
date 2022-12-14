#ifndef SYSYF_ACTIVEVAR_H
#define SYSYF_ACTIVEVAR_H

#include "Pass.h"
#include "Module.h"

class ActiveVar : public Pass
{
public:
    ActiveVar(Module *module) : Pass(module) {}
    void execute() final;
    const std::string get_name() const override {return name;}
    void dump();
private:
    Function *func_;
    const std::string name = "ActiveVar";
};

bool ValueCmp(Value* a, Value* b);
std::vector<Value*> sort_by_name(std::set<Value*> &val_set);
const std::string avdump = "active_var.out";

#endif  // SYSYF_ACTIVEVAR_H
