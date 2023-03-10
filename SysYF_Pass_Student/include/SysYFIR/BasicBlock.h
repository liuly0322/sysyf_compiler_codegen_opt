#ifndef _SYSYF_BASICBLOCK_H_
#define _SYSYF_BASICBLOCK_H_

#include "Instruction.h"
#include <list>
#include <set>
#include <string>

class Function;
class Module;

class BasicBlock : public Value {
  public:
    static BasicBlock *create(Module *m, const std::string &name,
                              Function *parent) {
        const auto *prefix = name.empty() ? "" : "label_";
        return new BasicBlock(m, prefix + name, parent);
    }

    // return parent, or null if none.
    Function *get_parent() { return parent_; }

    Module *get_module();

    /****************api about cfg****************/

    std::list<BasicBlock *> &get_pre_basic_blocks() { return pre_bbs_; }
    std::list<BasicBlock *> &get_succ_basic_blocks() { return succ_bbs_; }
    void add_pre_basic_block(BasicBlock *bb) {
        pre_bbs_.remove(bb);
        pre_bbs_.push_back(bb);
    }
    void add_succ_basic_block(BasicBlock *bb) {
        succ_bbs_.remove(bb);
        succ_bbs_.push_back(bb);
    }

    void remove_pre_basic_block(BasicBlock *bb) {
        remove_phi_from(bb);
        pre_bbs_.remove(bb);
    }
    void remove_succ_basic_block(BasicBlock *bb) { succ_bbs_.remove(bb); }

    void remove_phi_from(BasicBlock *bb);

    /****************api about cfg****************/

    /// Returns the terminator instruction if the block is well formed or null
    /// if the block is not well formed.
    [[nodiscard]] const Instruction *get_terminator() const;
    Instruction *get_terminator() {
        return const_cast<Instruction *>(
            static_cast<const BasicBlock *>(this)->get_terminator());
    }

    void add_instruction(Instruction *instr);
    void add_instruction(std::list<Instruction *>::iterator instr_pos,
                         Instruction *instr);
    void add_instr_begin(Instruction *instr);

    std::list<Instruction *>::iterator find_instruction(Instruction *instr);

    void delete_instr(Instruction *instr);

    bool empty() { return instr_list_.empty(); }

    int get_num_of_instr() { return instr_list_.size(); }
    std::list<Instruction *> &get_instructions() { return instr_list_; }

    void erase_from_parent();

    std::string print() override;

    /****************api about dominate tree****************/
    void set_idom(BasicBlock *bb) { idom_ = bb; }
    BasicBlock *get_idom() { return idom_; }
    void set_irdom(BasicBlock *bb) { irdom_ = bb; }
    BasicBlock *get_irdom() { return irdom_; }
    void add_dom_frontier(BasicBlock *bb) { dom_frontier_.insert(bb); }
    void add_rdom_frontier(BasicBlock *bb) { rdom_frontier_.insert(bb); }
    void clear_rdom_frontier() { rdom_frontier_.clear(); }
    auto add_rdom(BasicBlock *bb) { return rdoms_.insert(bb); }
    void clear_rdom() { rdoms_.clear(); }
    std::set<BasicBlock *> &get_dom_frontier() { return dom_frontier_; }
    std::set<BasicBlock *> &get_rdom_frontier() { return rdom_frontier_; }
    std::set<BasicBlock *> &get_rdoms() { return rdoms_; }
    void set_live_in(std::set<Value *> in) { live_in = std::move(in); }
    void set_live_out(std::set<Value *> out) { live_out = std::move(out); }
    std::set<Value *> &get_live_in() { return live_in; }
    std::set<Value *> &get_live_out() { return live_out; }

  private:
    explicit BasicBlock(Module *m, std::string name, Function *parent);
    std::list<BasicBlock *> pre_bbs_;
    std::list<BasicBlock *> succ_bbs_;
    std::list<Instruction *> instr_list_;
    std::set<BasicBlock *> dom_frontier_;
    std::set<BasicBlock *> rdom_frontier_;
    std::set<BasicBlock *> rdoms_;
    BasicBlock *idom_;
    BasicBlock *irdom_;
    std::set<Value *> live_in;
    std::set<Value *> live_out;
    Function *parent_;
};

#endif // _SYSYF_BASICBLOCK_H_