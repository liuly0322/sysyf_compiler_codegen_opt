#ifndef SYSYF_LIR_H
#define SYSYF_LIR_H

#include "Module.h"
#include "Pass.h"

class LIR:public Pass
{
public:
    explicit LIR(Module* module): Pass(module){}
    void execute() final;
    void merge_cmp_br(BasicBlock* bb);
    void split_gep(BasicBlock* bb);
    void split_srem(BasicBlock* bb);
    const std::string get_name() const override {return name;}
private:
    std::string name = "LIR";
};

#endif // SYSYF_LIR_H