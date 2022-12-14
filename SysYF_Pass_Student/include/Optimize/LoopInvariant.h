#ifndef MHSJ_LOOPINVARIANT_H
#define MHSJ_LOOPINVARIANT_H

#include "Module.h"
#include "Pass.h"
#include <vector>
#include <map>
#include <stack>
#include <set>
#include <memory>

class LoopInvariant : public Pass
{
public:
    explicit LoopInvariant(Module* module): Pass(module){}
    void execute() final;
    const std::string get_name() const override {return name;}

private:
    std::string name = "LoopInvariant";
};




#endif