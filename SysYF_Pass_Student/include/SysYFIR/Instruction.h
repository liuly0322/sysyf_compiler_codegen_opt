#ifndef _SYSYF_INSTRUCTION_H_
#define _SYSYF_INSTRUCTION_H_

#include "Constant.h"

class BasicBlock;
class Function;

class Instruction : public User {
  public:
    enum OpID {
        // Terminator Instructions
        ret,
        br,
        // Standard binary operators
        add,
        sub,
        mul,
        sdiv,
        srem,
        // Float binaru opeartors
        fadd,
        fsub,
        fmul,
        fdiv,
        // Memory operators
        alloca,
        load,
        store,
        // Other operators
        cmp,
        fcmp,
        phi,
        call,
        getelementptr,
        // Zero extend
        zext,
        // type cast bewteen float and singed integer
        fptosi,
        sitofp,
    };
    // create instruction, auto insert to bb
    // ty here is result type
    Instruction(Type *ty, OpID id, unsigned num_ops, BasicBlock *parent);
    Instruction(Type *ty, OpID id, unsigned num_ops);
    [[nodiscard]] inline const BasicBlock *get_parent() const {
        return parent_;
    }
    inline BasicBlock *get_parent() { return parent_; }
    void set_parent(BasicBlock *parent) { this->parent_ = parent; }
    // Return the function this instruction belongs to.
    Function *get_function();
    Module *get_module();

    OpID get_instr_type() { return op_id_; }

    bool is_void() {
        return ((op_id_ == ret) || (op_id_ == br) || (op_id_ == store) ||
                (op_id_ == call && this->get_type()->is_void_type()));
    }

    bool is_phi() { return op_id_ == phi; }
    bool is_store() { return op_id_ == store; }
    bool is_alloca() { return op_id_ == alloca; }
    bool is_ret() { return op_id_ == ret; }
    bool is_load() { return op_id_ == load; }
    bool is_br() { return op_id_ == br; }

    bool is_add() { return op_id_ == add; }
    bool is_sub() { return op_id_ == sub; }
    bool is_mul() { return op_id_ == mul; }
    bool is_div() { return op_id_ == sdiv; }
    bool is_rem() { return op_id_ == srem; }

    bool is_fadd() { return op_id_ == fadd; }
    bool is_fsub() { return op_id_ == fsub; }
    bool is_fmul() { return op_id_ == fmul; }
    bool is_fdiv() { return op_id_ == fdiv; }

    bool is_cmp() { return op_id_ == cmp; }
    bool is_fcmp() { return op_id_ == fcmp; }

    bool is_call() { return op_id_ == call; }
    bool is_gep() { return op_id_ == getelementptr; }
    bool is_zext() { return op_id_ == zext; }
    bool is_fptosi() { return op_id_ == fptosi; }
    bool is_sitofp() { return op_id_ == sitofp; }

    bool is_unary() {
        return (is_fptosi() || is_sitofp() || is_zext()) &&
               (get_num_operand() == 1);
    }

    bool is_int_binary() {
        return (is_add() || is_sub() || is_mul() || is_div() || is_rem()) &&
               (get_num_operand() == 2);
    }

    bool is_float_binary() {
        return (is_fadd() || is_fsub() || is_fmul() || is_fdiv()) &&
               (get_num_operand() == 2);
    }

    bool is_cmp_binary() {
        return (is_cmp() || is_fcmp()) && (get_num_operand() == 2);
    }

    bool is_binary() {
        return is_int_binary() || is_float_binary() || is_cmp_binary();
    }

    bool isTerminator() { return is_br() || is_ret(); }

    void set_id(int id) { id_ = id; }
    [[nodiscard]] int get_id() const { return id_; }

  private:
    BasicBlock *parent_;
    OpID op_id_;
    int id_;
};

class BinaryInst : public Instruction {
  private:
    BinaryInst(Type *ty, OpID id, Value *v1, Value *v2, BasicBlock *bb);

  public:
    static BinaryInst *create_add(Value *v1, Value *v2, BasicBlock *bb,
                                  Module *m);
    static BinaryInst *create_sub(Value *v1, Value *v2, BasicBlock *bb,
                                  Module *m);
    static BinaryInst *create_mul(Value *v1, Value *v2, BasicBlock *bb,
                                  Module *m);
    static BinaryInst *create_sdiv(Value *v1, Value *v2, BasicBlock *bb,
                                   Module *m);
    static BinaryInst *create_srem(Value *v1, Value *v2, BasicBlock *bb,
                                   Module *m);
    static BinaryInst *create_fadd(Value *v1, Value *v2, BasicBlock *bb,
                                   Module *m);
    static BinaryInst *create_fsub(Value *v1, Value *v2, BasicBlock *bb,
                                   Module *m);
    static BinaryInst *create_fmul(Value *v1, Value *v2, BasicBlock *bb,
                                   Module *m);
    static BinaryInst *create_fdiv(Value *v1, Value *v2, BasicBlock *bb,
                                   Module *m);

    std::string print() override;

  private:
    void assertValid();
};

class CmpInst : public Instruction {
  public:
    enum CmpOp {
        EQ, // ==
        NE, // !=
        GT, // >
        GE, // >=
        LT, // <
        LE  // <=
    };

  private:
    CmpInst(Type *ty, CmpOp op, Value *lhs, Value *rhs, BasicBlock *bb);

  public:
    static CmpInst *create_cmp(CmpOp op, Value *lhs, Value *rhs, BasicBlock *bb,
                               Module *m);

    CmpOp get_cmp_op() { return cmp_op_; }
    void set_cmp_op(CmpOp op) { cmp_op_ = op; }

    std::string print() override;

  private:
    CmpOp cmp_op_;

    void assertValid();
};

class FCmpInst : public Instruction {
  public:
    enum CmpOp {
        EQ, // ==
        NE, // !=
        GT, // >
        GE, // >=
        LT, // <
        LE  // <=
    };

  private:
    FCmpInst(Type *ty, CmpOp op, Value *lhs, Value *rhs, BasicBlock *bb);

  public:
    static FCmpInst *create_fcmp(CmpOp op, Value *lhs, Value *rhs,
                                 BasicBlock *bb, Module *m);

    CmpOp get_cmp_op() { return cmp_op_; }
    void set_cmp_op(CmpOp op) { cmp_op_ = op; }

    std::string print() override;

  private:
    CmpOp cmp_op_;

    void assertValid();
};

class CallInst : public Instruction {
  protected:
    CallInst(Function *func, std::vector<Value *> args, BasicBlock *bb);
    CallInst(Type *ret_ty, std::vector<Value *> args, BasicBlock *bb);

  public:
    static CallInst *create(Function *func, std::vector<Value *> args,
                            BasicBlock *bb);
    [[nodiscard]] FunctionType *get_function_type() const;

    std::string print() override;
};

class BranchInst : public Instruction {
  private:
    BranchInst(Value *cond, BasicBlock *if_true, BasicBlock *if_false,
               BasicBlock *bb);
    BranchInst(Value *cond, BasicBlock *bb);
    BranchInst(BasicBlock *if_true, BasicBlock *bb);
    explicit BranchInst(BasicBlock *bb);

  public:
    static BranchInst *create_cond_br(Value *cond, BasicBlock *if_true,
                                      BasicBlock *if_false, BasicBlock *bb);
    static BranchInst *create_br(BasicBlock *if_true, BasicBlock *bb);

    [[nodiscard]] bool is_cond_br() const;

    std::string print() override;
};

class ReturnInst : public Instruction {
  private:
    ReturnInst(Value *val, BasicBlock *bb);
    explicit ReturnInst(BasicBlock *bb);

  public:
    static ReturnInst *create_ret(Value *val, BasicBlock *bb);
    static ReturnInst *create_void_ret(BasicBlock *bb);
    [[nodiscard]] bool is_void_ret() const;

    std::string print() override;
};

class GetElementPtrInst : public Instruction {
  private:
    GetElementPtrInst(Value *ptr, std::vector<Value *> idxs, BasicBlock *bb);

  public:
    static Type *get_element_type(Value *ptr, std::vector<Value *> idxs);
    static GetElementPtrInst *create_gep(Value *ptr, std::vector<Value *> idxs,
                                         BasicBlock *bb);
    [[nodiscard]] Type *get_element_type() const;

    std::string print() override;

  private:
    Type *element_ty_;
};

class StoreInst : public Instruction {
  private:
    StoreInst(Value *val, Value *ptr, BasicBlock *bb);

  public:
    static StoreInst *create_store(Value *val, Value *ptr, BasicBlock *bb);

    Value *get_rval() { return this->get_operand(0); }
    Value *get_lval() { return this->get_operand(1); }

    std::string print() override;
};

class LoadInst : public Instruction {
  private:
    LoadInst(Type *ty, Value *ptr, BasicBlock *bb);

  public:
    static LoadInst *create_load(Type *ty, Value *ptr, BasicBlock *bb);
    Value *get_lval() { return this->get_operand(0); }

    [[nodiscard]] Type *get_load_type() const;

    std::string print() override;
};

class AllocaInst : public Instruction {
  private:
    AllocaInst(Type *ty, BasicBlock *bb);

  public:
    static AllocaInst *create_alloca(Type *ty, BasicBlock *bb);

    [[nodiscard]] Type *get_alloca_type() const;

    std::string print() override;

  private:
    Type *alloca_ty_;
};

class ZextInst : public Instruction {
  private:
    ZextInst(OpID op, Value *val, Type *ty, BasicBlock *bb);

  public:
    static ZextInst *create_zext(Value *val, Type *ty, BasicBlock *bb);

    [[nodiscard]] Type *get_dest_type() const;

    std::string print() override;

  private:
    Type *dest_ty_;
};

class FpToSiInst : public Instruction {
  private:
    FpToSiInst(OpID op, Value *val, Type *ty, BasicBlock *bb);

  public:
    static FpToSiInst *create_fptosi(Value *val, Type *ty, BasicBlock *bb);

    [[nodiscard]] Type *get_dest_type() const;

    std::string print() override;

  private:
    Type *dest_ty_;
};

class SiToFpInst : public Instruction {
  private:
    SiToFpInst(OpID op, Value *val, Type *ty, BasicBlock *bb);

  public:
    static SiToFpInst *create_sitofp(Value *val, Type *ty, BasicBlock *bb);

    [[nodiscard]] Type *get_dest_type() const;

    std::string print() override;

  private:
    Type *dest_ty_;
};

class PhiInst : public Instruction {
  private:
    PhiInst(OpID op, std::vector<Value *> vals,
            std::vector<BasicBlock *> val_bbs, Type *ty, BasicBlock *bb);

  public:
    static PhiInst *create_phi(Type *ty, BasicBlock *bb);
    Value *get_lval() { return l_val_; }
    void set_lval(Value *l_val) { l_val_ = l_val; }
    void add_phi_pair_operand(Value *val, Value *pre_bb) {
        this->add_operand(val);
        this->add_operand(pre_bb);
    }
    std::string print() override;

  private:
    Value *l_val_;
};

#endif // _SYSYF_INSTRUCTION_H_
