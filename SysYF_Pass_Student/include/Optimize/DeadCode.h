#ifndef SYSYF_DEADCODE_H
#define SYSYF_DEADCODE_H

#include "Pass.h"
#include "PureFunction.h"
#include <memory>
#include <set>

class Marker;
class Cleaner;

class DeadCode : public Pass {
  public:
    explicit DeadCode(Module *module) : Pass(module) {
        marker = std::make_unique<Marker>(marked);
        cleaner = std::make_unique<Cleaner>();
    }
    void execute() final;
    [[nodiscard]] std::string get_name() const override { return name; }

  private:
    void rewriteBranchToPostDomJmp(BranchInst *inst);
    void mark(Function *f);
    void sweep(Function *f);
    bool clean(Function *f);

    const std::string name = "DeadCode";
    std::set<Instruction *> marked;
    std::unique_ptr<Marker> marker;
    std::unique_ptr<Cleaner> cleaner;
};

class Marker {
  public:
    explicit Marker(std::set<Instruction *> &marked) : marked(marked){};
    void mark(Function *f);

  private:
    void markBaseCase(Function *f);
    static bool isCriticalInst(Instruction *inst);
    void markInst(Instruction *inst);
    void markLocalArrayStore(AllocaInst *inst);
    void markOperand(Value *op);
    void markRDF(BasicBlock *bb);

    std::set<Instruction *> &marked;
    std::map<AllocaInst *, std::set<Instruction *>> lval_store;
    std::list<Instruction *> worklist;
};

class Cleaner {
  public:
    Cleaner() = default;
    bool clean(Function *f);

  private:
    void postTraverseBasicBlocks();
    void visitBasicBlock(BasicBlock *i);
    static bool eliminateRedundantBranches(Instruction *br_inst);
    bool blockMergeable(BasicBlock *i, BasicBlock *j);
    void mergeBlocks(BasicBlock *i, BasicBlock *j);
    static void replaceBlockWith(BasicBlock *i, BasicBlock *j);
    static bool BranchHoistable(BasicBlock *j);
    void hoistBranch(BasicBlock *i, BasicBlock *j);
    static void connectHoisting(BasicBlock *i, BasicBlock *j, BasicBlock *dst);
    void cleanUnreachable();

    Function *f_;
    std::set<BasicBlock *> visited;
    bool cleaned = false;
    bool ever_cleaned = false;
};

#endif // SYSYF_DEADCODE_H
