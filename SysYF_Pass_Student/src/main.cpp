#include "ActiveVar.h"
#include "SCCP.h"
#include "Check.h"
#include "DeadCode.h"
#include "DominateTree.h"
#include "ErrorReporter.h"
#include "IRBuilder.h"
#include "Mem2Reg.h"
#include "Pass.h"
#include "RDominateTree.h"
#include "SyntaxTreeChecker.h"
#include "SyntaxTreePrinter.h"
#include "SysYFDriver.h"
#include <iostream>

void print_help(const std::string &exe_name) {
    std::cout << "Usage: " << exe_name
              << " [ -h | --help ] [ -p | --trace_parsing ] [ -s | --trace_scanning ] [ -emit-ast ] [ -check ]"
              << " [ -emit-ir ] [ -O2 ] [ -O ] [ -av ] [ -o <output-file> ]"
              << " <input-file>" << std::endl;
}

int main(int argc, char* argv[]) {
    IRBuilder builder;
    SysYFDriver driver;
    SyntaxTreePrinter printer;
    ErrorReporter reporter(std::cerr);
    SyntaxTreeChecker checker(reporter);

    bool print_ast = false;
    bool emit_ir = false;
    bool check = false;
    bool optimize_all = false;
    bool optimize = false;

    // 适用于评估的优化Pass开关
    bool av = false; // 活跃变量分析
    bool dce = false; // 死代码消除
    bool sccp = false; // 稀疏条件常量传播

    std::string filename = "-";
    std::string output_llvm_file = "-";
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == std::string("-h") || argv[i] == std::string("--help")) {
            print_help(argv[0]);
            return 0;
        }
        else if (argv[i] == std::string("-p") || argv[i] == std::string("--trace_parsing")) {
            driver.trace_parsing = true;
        }
        else if (argv[i] == std::string("-s") || argv[i] == std::string("--trace_scanning")) {
            driver.trace_scanning = true;
        }
        else if (argv[i] == std::string("-emit-ast")) {
            print_ast = true;
        }
        else if (argv[i] == std::string("-emit-ir")) {
            emit_ir = true;
        }
        else if (argv[i] == std::string("-o")) {
            output_llvm_file = argv[++i];
        }
        else if (argv[i] == std::string("-check")) {
            check = true;
        }
        else if (argv[i] == std::string("-O2")) {
            av = true;
            dce = true;
            sccp = true;
            optimize = true;
        }
        else if (argv[i] == std::string("-O")) {
            optimize = true;
        }
        else if (argv[i] == std::string("-av")) {
            av = true;
            optimize = true;
        }
        else if (argv[i] == std::string("-dce")) {
            dce = true;
            optimize = true;
        }
        else if (argv[i] == std::string("-sccp")) {
            sccp = true;
            optimize = true;
        }
        //  ...
        else {
            filename = argv[i];
        }
    }
    auto root = driver.parse(filename);
    if (print_ast)
        root->accept(printer);
    if (check)
        root->accept(checker);
    if (emit_ir) {
        root->accept(builder);
        auto m = builder.getModule();
        m->set_file_name(filename);
        m->set_print_name();
        if (optimize) {
            PassMgr passmgr(m.get());
            passmgr.addPass<DominateTree>();
            passmgr.addPass<RDominateTree>();
            passmgr.addPass<Mem2Reg>();
            passmgr.addPass<Check>();
            if (av) {
                passmgr.addPass<ActiveVar>();
                passmgr.addPass<Check>();
            }
            if (dce) {
                passmgr.addPass<DeadCode>();
                passmgr.addPass<Check>();
            }
            if (sccp) {
                passmgr.addPass<SCCP>();
                passmgr.addPass<Check>();
            }
            passmgr.execute();
            m->set_print_name();
        }
        auto IR = m->print();
        if (output_llvm_file == "-") {
            std::cout << IR;
        }
        else {
            std::ofstream output_stream;
            output_stream.open(output_llvm_file, std::ios::out);
            output_stream << IR;
            output_stream.close();
        }
    }
    return 0;
}
