#include "Module.h"

std::string print_as_op(Value *v, bool print_ty);
std::string print_cmp_type(CmpInst::CmpOp op);
std::string print_fcmp_type(FCmpInst::CmpOp op);