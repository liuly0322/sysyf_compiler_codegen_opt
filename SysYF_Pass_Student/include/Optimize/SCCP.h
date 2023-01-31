#ifndef SYSYF_SCCP_H
#define SYSYF_SCCP_H

#include "Pass.h"
#include <unordered_map>
#include <unordered_set>

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

class SCCP : public Pass {
  public:
    explicit SCCP(Module *module) : Pass(module) {}
    void execute() final;
    void sccp(Function *f);
    [[nodiscard]] std::string get_name() const override { return name; }

  private:
    const std::string name = "SCCP";
    void visitInst(Instruction *inst);
    void replaceConstant(Function *f);

    const std::unordered_set<std::string> binary_ops{
        "add",  "sub",  "mul",  "sdiv", "srem", "fadd",
        "fsub", "fmul", "fdiv", "cmp",  "fcmp"};
    const std::unordered_set<std::string> unary_ops{"fptosi", "sitofp", "zext"};

    Constant *constFold(Instruction *inst, Constant *v1, Constant *v2);
    Constant *constFold(Instruction *inst, Constant *v);
    Constant *constFold(Instruction *inst);

    std::unordered_map<Value *, ValueStatus> value_map;

    ValueStatus getMapped(Value *key) {
        if (auto *constant = dynamic_cast<Constant *>(key))
            return {ValueStatus::CONST, constant};
        return value_map[key];
    }

    std::set<std::pair<BasicBlock *, BasicBlock *>> marked;
    std::vector<std::pair<BasicBlock *, BasicBlock *>> cfg_worklist;
    std::vector<Instruction *> ssa_worklist;
};

#endif // SYSYF_SCCP_H
