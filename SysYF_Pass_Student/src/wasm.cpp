#include "ActiveVar.h"
#include "CSE.h"
#include "Check.h"
#include "DeadCode.h"
#include "DominateTree.h"
#include "IRBuilder.h"
#include "Mem2Reg.h"
#include "Pass.h"
#include "RDominateTree.h"
#include "SCCP.h"
#include "SysYFDriver.h"
#include <iostream>
#include <string>

std::string result{};

extern "C" {
const char *compile(const char *str, int sccp, int cse, int dce) {
    IRBuilder builder;
    SysYFDriver driver;

    std::string filename{"tmp.sy"};
    FILE *fptr = fopen(filename.c_str(), "w");
    fputs(str, fptr);
    fclose(fptr);

    auto *root = driver.parse(filename);

    root->accept(builder);
    auto m = builder.getModule();
    m->set_file_name(filename);
    m->set_print_name();

    PassMgr passmgr(m.get());
    passmgr.addPass<DominateTree>();
    passmgr.addPass<RDominateTree>();
    passmgr.addPass<Mem2Reg>();
    passmgr.addPass<Check>();

    if (cse) {
        passmgr.addPass<CSE>();
        passmgr.addPass<Check>();
    }

    if (sccp) {
        passmgr.addPass<SCCP>();
        passmgr.addPass<Check>();
    }

    if (cse) {
        passmgr.addPass<CSE>();
        passmgr.addPass<Check>();
    }

    if (dce) {
        passmgr.addPass<DeadCode>();
        passmgr.addPass<Check>();
    }

    passmgr.execute();
    m->set_print_name();

    result = m->print();
    return result.c_str();
}
}

int main() { return 0; }