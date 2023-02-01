#ifndef SYSYF_DEADCODE_H
#define SYSYF_DEADCODE_H

#include "Pass.h"
#include "PureFunction.h"
#include <memory>
#include <set>

class Marker;
class Cleaner;

bool is_critical_inst(Instruction *inst);
void delete_basic_block(BasicBlock *i, BasicBlock *j);

class DeadCode : public Pass {
  public:
    explicit DeadCode(Module *module) : Pass(module) {
        marker = std::make_unique<Marker>(marked);
        cleaner = std::make_unique<Cleaner>();
    }
    void rewriteBranchToPostDomJmp(BranchInst *inst);
    void execute() final;
    void mark(Function *f);
    void sweep(Function *f);
    bool clean(Function *f);
    [[nodiscard]] std::string get_name() const override { return name; }

  private:
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
    void markInst(Instruction *inst);
    void markBaseCase(Function *f);
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
    void postTraverseBasicBlocks();
    void visitBasicBlock(BasicBlock *i);
    static bool eliminateRedundantBranches(Instruction *br_inst);
    void eliminateEmptyBlock(BasicBlock *i, BasicBlock *j);
    void combineBasicBlocks(BasicBlock *i, BasicBlock *j);
    static void connectHoisting(BasicBlock *i, BasicBlock *j, BasicBlock *dst);
    void hoistBranches(BasicBlock *i, BasicBlock *j);
    void cleanUnreachable();

  private:
    Function *f_;
    std::set<BasicBlock *> visited;
    bool cleaned = false;
    bool ever_cleaned = false;
};

#endif // SYSYF_DEADCODE_H
