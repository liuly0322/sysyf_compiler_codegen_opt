#ifndef SYSYF_DOMINATETREE_H
#define SYSYF_DOMINATETREE_H

#include "Pass.h"
#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

class DominateTree : public Pass {
  public:
    explicit DominateTree(Module *module) : Pass(module) {}
    void execute() final;
    void get_revserse_post_order(Function *f);
    void get_post_order(BasicBlock *bb, std::set<BasicBlock *> &visited);
    void get_bb_idom(Function *f);
    void get_bb_dom_front(Function *f);
    BasicBlock *intersect(BasicBlock *b1, BasicBlock *b2);
    [[nodiscard]] std::string get_name() const override { return name; }

  private:
    std::list<BasicBlock *> reverse_post_order;
    std::map<BasicBlock *, int> bb2int;
    std::vector<BasicBlock *> doms;
    const std::string name = "DominateTree";
};

#endif // SYSYF_DOMINATETREE_H
