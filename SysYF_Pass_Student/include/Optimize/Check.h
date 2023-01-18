#ifndef CHECK_H
#define CHECK_H

#include "Pass.h"
#include <string>

class Check : public Pass {
  private:
    const std::string name = "Check";
    bool Broken;
    std::set<GlobalVariable *> define_global;
    std::set<Instruction *> define_inst;
    std::set<Function *> functions;
    std::set<BasicBlock *> blocks;

  public:
    explicit Check(Module *m) : Pass(m) {}
    ~Check() = default;
    void execute() final;
    [[nodiscard]] std::string get_name() const override { return name; }

    // Debug Info Function
    template <typename T1, typename... Ts>
    void checkFailed(const std::string &message, T1 v1, Ts... vs);

    // Check Function
    void valueDefineCounting(Function *fun);
    void checkFunction(Function *fun);
    void checkBasicBlock(BasicBlock *bb);
    void checkPhiInstruction(Instruction *inst);
    void checkCallInstruction(Instruction *inst);
    void checkInstruction(Instruction *inst);
    void checkUse(User *user);
};

#endif