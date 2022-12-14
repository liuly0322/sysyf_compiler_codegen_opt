#ifndef SYSYF_MOVCONST_H
#define SYSYF_MOVCONST_H

#include "Module.h"
#include "Pass.h"


class MovConst : public Pass{
public:
    explicit MovConst(Module* module):Pass(module){}
    void execute()override;
    const std::string get_name() const override{return name;}

private:
    static void mov_const(BasicBlock* bb);
    const std::string name = "MovConst";
};

#endif //SYSYF_MOVCONST_H