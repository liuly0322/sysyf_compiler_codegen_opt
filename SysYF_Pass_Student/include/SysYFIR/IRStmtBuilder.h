#ifndef _SYSYF_IRSTMTBUILDER_H_
#define _SYSYF_IRSTMTBUILDER_H_

#include "Module.h"

class IRStmtBuilder {
  private:
    BasicBlock *BB_;
    Module *m_;

  public:
    IRStmtBuilder(BasicBlock *bb, Module *m) : BB_(bb), m_(m){};
    ~IRStmtBuilder() = default;
    Module *get_module() { return m_; }
    BasicBlock *get_insert_block() { return BB_; }
    void set_insert_point(BasicBlock *bb) {
        BB_ = bb;
    } // 在某个基本块中插入指令
    BinaryInst *create_iadd(Value *lhs, Value *rhs) {
        return BinaryInst::create_add(lhs, rhs, BB_, m_);
    } // 创建加法指令（以及其他算术指令）
    BinaryInst *create_isub(Value *lhs, Value *rhs) {
        return BinaryInst::create_sub(lhs, rhs, BB_, m_);
    }
    BinaryInst *create_imul(Value *lhs, Value *rhs) {
        return BinaryInst::create_mul(lhs, rhs, BB_, m_);
    }
    BinaryInst *create_isdiv(Value *lhs, Value *rhs) {
        return BinaryInst::create_sdiv(lhs, rhs, BB_, m_);
    }
    BinaryInst *create_isrem(Value *lhs, Value *rhs) {
        return BinaryInst::create_srem(lhs, rhs, BB_, m_);
    }

    CmpInst *create_icmp(Value *lhs, Value *rhs, CmpInst::CmpOp op) {
        return CmpInst::create_cmp(op, lhs, rhs, BB_, m_);
    }
    CmpInst *create_icmp_eq(Value *lhs, Value *rhs) {
        return CmpInst::create_cmp(CmpInst::EQ, lhs, rhs, BB_, m_);
    }
    CmpInst *create_icmp_ne(Value *lhs, Value *rhs) {
        return CmpInst::create_cmp(CmpInst::NE, lhs, rhs, BB_, m_);
    }
    CmpInst *create_icmp_gt(Value *lhs, Value *rhs) {
        return CmpInst::create_cmp(CmpInst::GT, lhs, rhs, BB_, m_);
    }
    CmpInst *create_icmp_ge(Value *lhs, Value *rhs) {
        return CmpInst::create_cmp(CmpInst::GE, lhs, rhs, BB_, m_);
    }
    CmpInst *create_icmp_lt(Value *lhs, Value *rhs) {
        return CmpInst::create_cmp(CmpInst::LT, lhs, rhs, BB_, m_);
    }
    CmpInst *create_icmp_le(Value *lhs, Value *rhs) {
        return CmpInst::create_cmp(CmpInst::LE, lhs, rhs, BB_, m_);
    }

    BinaryInst *create_fadd(Value *lhs, Value *rhs) {
        return BinaryInst::create_fadd(lhs, rhs, BB_, m_);
    }
    BinaryInst *create_fsub(Value *lhs, Value *rhs) {
        return BinaryInst::create_fsub(lhs, rhs, BB_, m_);
    }
    BinaryInst *create_fmul(Value *lhs, Value *rhs) {
        return BinaryInst::create_fmul(lhs, rhs, BB_, m_);
    }
    BinaryInst *create_fdiv(Value *lhs, Value *rhs) {
        return BinaryInst::create_fdiv(lhs, rhs, BB_, m_);
    }

    FCmpInst *create_fcmp(Value *lhs, Value *rhs, FCmpInst::CmpOp op) {
        return FCmpInst::create_fcmp(op, lhs, rhs, BB_, m_);
    }
    FCmpInst *create_fcmp_eq(Value *lhs, Value *rhs) {
        return FCmpInst::create_fcmp(FCmpInst::EQ, lhs, rhs, BB_, m_);
    }
    FCmpInst *create_fcmp_ne(Value *lhs, Value *rhs) {
        return FCmpInst::create_fcmp(FCmpInst::NE, lhs, rhs, BB_, m_);
    }
    FCmpInst *create_fcmp_gt(Value *lhs, Value *rhs) {
        return FCmpInst::create_fcmp(FCmpInst::GT, lhs, rhs, BB_, m_);
    }
    FCmpInst *create_fcmp_ge(Value *lhs, Value *rhs) {
        return FCmpInst::create_fcmp(FCmpInst::GE, lhs, rhs, BB_, m_);
    }
    FCmpInst *create_fcmp_lt(Value *lhs, Value *rhs) {
        return FCmpInst::create_fcmp(FCmpInst::LT, lhs, rhs, BB_, m_);
    }
    FCmpInst *create_fcmp_le(Value *lhs, Value *rhs) {
        return FCmpInst::create_fcmp(FCmpInst::LE, lhs, rhs, BB_, m_);
    }

    CallInst *create_call(Value *func, std::vector<Value *> args) {
#ifdef DEBUG
        assert(dynamic_cast<Function *>(func) &&
               "func must be Function * type");
#endif
        return CallInst::create(static_cast<Function *>(func), args, BB_);
    }

    BranchInst *create_br(BasicBlock *if_true) {
        return BranchInst::create_br(if_true, BB_);
    }
    BranchInst *create_cond_br(Value *cond, BasicBlock *if_true,
                               BasicBlock *if_false) {
        return BranchInst::create_cond_br(cond, if_true, if_false, BB_);
    }

    ReturnInst *create_ret(Value *val) {
        return ReturnInst::create_ret(val, BB_);
    }
    ReturnInst *create_void_ret() { return ReturnInst::create_void_ret(BB_); }

    GetElementPtrInst *create_gep(Value *ptr, std::vector<Value *> idxs) {
        return GetElementPtrInst::create_gep(ptr, idxs, BB_);
    }

    StoreInst *create_store(Value *val, Value *ptr) {
        return StoreInst::create_store(val, ptr, BB_);
    }
    LoadInst *create_load(Type *ty, Value *ptr) {
        return LoadInst::create_load(ty, ptr, BB_);
    }
    LoadInst *create_load(Value *ptr) {
#ifdef DEBUG
        assert(ptr->get_type()->is_pointer_type() &&
               "ptr must be pointer type");
#endif
        return LoadInst::create_load(
            ptr->get_type()->get_pointer_element_type(), ptr, BB_);
    }

    AllocaInst *create_alloca(Type *ty) {
        return AllocaInst::create_alloca(ty, BB_);
    }
    ZextInst *create_zext(Value *val, Type *ty) {
        return ZextInst::create_zext(val, ty, BB_);
    }
    FpToSiInst *create_fptosi(Value *val, Type *ty) {
        return FpToSiInst::create_fptosi(val, ty, BB_);
    }
    SiToFpInst *create_sitofp(Value *val, Type *ty) {
        return SiToFpInst::create_sitofp(val, ty, BB_);
    }
};

#endif // _SYSYF_IRSTMTBUILDER_H_
