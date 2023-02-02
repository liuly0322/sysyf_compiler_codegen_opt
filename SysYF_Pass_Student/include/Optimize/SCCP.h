#ifndef SYSYF_SCCP_H
#define SYSYF_SCCP_H

#include "Pass.h"
#include <memory>

class InstructionVisitor;

static inline int get_const_int(Value *constant) {
    return static_cast<ConstantInt *>(constant)->get_value();
}

static inline float get_const_float(Value *constant) {
    return static_cast<ConstantFloat *>(constant)->get_value();
}

struct ValueStatus {
    enum Status { BOT = 0, CONST, TOP };
    Status status;
    Constant *value;
    [[nodiscard]] bool is_bot() const { return status == BOT; }
    [[nodiscard]] bool is_const() const { return status == CONST; }
    [[nodiscard]] bool is_top() const { return status == TOP; }
    void operator^=(ValueStatus &b) {
        if (b.status < status) {
            status = b.status;
            value = b.value;
        } else if (status == b.status && status == CONST) {
            if (dynamic_cast<ConstantInt *>(value) != nullptr) {
                auto x = get_const_int(value);
                auto y = get_const_int(b.value);
                if (x != y) {
                    status = BOT;
                    value = nullptr;
                }
            } else {
                auto x = get_const_float(value);
                auto y = get_const_float(b.value);
                if (x != y) {
                    status = BOT;
                    value = nullptr;
                }
            }
        }
    }
    bool operator!=(ValueStatus &b) const {
        if (status != b.status)
            return true;
        if (status != CONST)
            return false;
        if (dynamic_cast<ConstantInt *>(value) != nullptr) {
            auto x = get_const_int(value);
            auto y = get_const_int(b.value);
            return x != y;
        }
        auto x = get_const_float(value);
        auto y = get_const_float(b.value);
        return x != y;
    }
};

class ValueMap {
  public:
    void clear() { value_map.clear(); }
    ValueStatus get(Value *key) {
        if (auto *constant = dynamic_cast<Constant *>(key))
            return {ValueStatus::CONST, constant};
        return value_map[key];
    }
    void set(Value *key, ValueStatus value) { value_map[key] = value; }

  private:
    std::map<Value *, ValueStatus> value_map;
};

class SCCP : public Pass {
  public:
    explicit SCCP(Module *module) : Pass(module) {
        instruction_visitor = std::make_unique<InstructionVisitor>(*this);
    }
    void execute() final;
    Constant *constFold(Instruction *inst);

    [[nodiscard]] std::string get_name() const override { return name; }
    std::set<std::pair<BasicBlock *, BasicBlock *>> &get_marked() {
        return marked;
    }
    ValueMap &get_value_map() { return value_map; }
    std::vector<std::pair<BasicBlock *, BasicBlock *>> &get_cfg_worklist() {
        return cfg_worklist;
    }
    std::vector<Instruction *> &get_ssa_worklist() { return ssa_worklist; }

  private:
    void sccp(Function *f);
    ValueStatus getMapped(Value *key) { return value_map.get(key); }
    Constant *constFold(CmpInst *inst, Constant *v1, Constant *v2);
    Constant *constFold(FCmpInst *inst, Constant *v1, Constant *v2);
    Constant *constFold(Instruction *inst, Constant *v1, Constant *v2);
    Constant *constFold(Instruction *inst, Constant *v);
    void replaceConstant(Function *f);
    static void condBrToJmp(Instruction *inst, BasicBlock *jmp_bb,
                            BasicBlock *invalid_bb);

    const std::string name = "SCCP";
    ValueMap value_map;
    std::set<std::pair<BasicBlock *, BasicBlock *>> marked;
    std::vector<std::pair<BasicBlock *, BasicBlock *>> cfg_worklist;
    std::vector<Instruction *> ssa_worklist;
    std::unique_ptr<InstructionVisitor> instruction_visitor;
};

class InstructionVisitor {
  public:
    explicit InstructionVisitor(SCCP &sccp_pass)
        : sccp(sccp_pass), value_map(sccp_pass.get_value_map()),
          cfg_worklist(sccp_pass.get_cfg_worklist()),
          ssa_worklist(sccp_pass.get_ssa_worklist()) {}
    void visit(Instruction *inst);

  private:
    void visit_phi(PhiInst *inst);
    void visit_br(BranchInst *inst);
    void visit_foldable(Instruction *inst);

    SCCP &sccp;
    ValueMap &value_map;
    std::vector<std::pair<BasicBlock *, BasicBlock *>> &cfg_worklist;
    std::vector<Instruction *> &ssa_worklist;

    Instruction *inst_;
    BasicBlock *bb;
    ValueStatus prev_status;
    ValueStatus cur_status;
};

#endif // SYSYF_SCCP_H
