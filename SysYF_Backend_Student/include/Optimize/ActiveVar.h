#ifndef SYSYF_ACTIVEVAR_H
#define SYSYF_ACTIVEVAR_H

#include "Pass.h"
#include "Module.h"

class ActiveVar : public Pass
{
public:
    ActiveVar(Module *majo) : Pass(majo) {}
    void execute() final;
    const std::string get_name() const override {return namae;}
private:
    Function *eknktwr;
    const std::string namae = "ActiveVar";
    std::map<BasicBlock *, std::set<Value *>> knmmdk, akmhmr, mksyk, tmemm, skrkk;
};

#endif  // SYSYF_ACTIVEVAR_H
