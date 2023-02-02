/**
 * @file IRBuilder.cpp
 * @author liuly (me@liuly.moe)
 * @brief IRBuilder 简易实现
 * @version 0.1
 * @date 2022-12-08
 * 一个 SysYF IRBuilder 的简易实现，目前仅支持一维数组
 * @copyright Copyright (c) 2022
 */

#include "IRBuilder.h"
#include "SyntaxTree.h"

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *FLOAT_T;

const std::map<SyntaxTree::BinaryCondOp, CmpInst::CmpOp> binCondOpToCmpOp = {
    {SyntaxTree::BinaryCondOp::EQ, CmpInst::CmpOp::EQ},
    {SyntaxTree::BinaryCondOp::NEQ, CmpInst::CmpOp::NE},
    {SyntaxTree::BinaryCondOp::GT, CmpInst::CmpOp::GT},
    {SyntaxTree::BinaryCondOp::GTE, CmpInst::CmpOp::GE},
    {SyntaxTree::BinaryCondOp::LT, CmpInst::CmpOp::LT},
    {SyntaxTree::BinaryCondOp::LTE, CmpInst::CmpOp::LE},
};

const std::map<SyntaxTree::BinaryCondOp, FCmpInst::CmpOp> binCondOpToFCmpOp = {
    {SyntaxTree::BinaryCondOp::EQ, FCmpInst::CmpOp::EQ},
    {SyntaxTree::BinaryCondOp::NEQ, FCmpInst::CmpOp::NE},
    {SyntaxTree::BinaryCondOp::GT, FCmpInst::CmpOp::GT},
    {SyntaxTree::BinaryCondOp::GTE, FCmpInst::CmpOp::GE},
    {SyntaxTree::BinaryCondOp::LT, FCmpInst::CmpOp::LT},
    {SyntaxTree::BinaryCondOp::LTE, FCmpInst::CmpOp::LE},
};

Type *baseTypetoLLVMTy(SyntaxTree::Type type) {
    switch (type) {
    case SyntaxTree::Type::INT:
        return INT32_T;
    case SyntaxTree::Type::VOID:
        return VOID_T;
    case SyntaxTree::Type::BOOL:
        return INT1_T;
    case SyntaxTree::Type::FLOAT:
        return FLOAT_T;
    default:
        return nullptr;
    }
}

Type *SyntaxTreeTytoLLVMTy(SyntaxTree::Type type, bool isPtr = false) {
    Type *base_type = baseTypetoLLVMTy(type);
    if (base_type == nullptr || !isPtr)
        return base_type;
    return Type::get_pointer_type(base_type);
}

Value *IRBuilder::typeConvertConstant(Constant *expr, Type *expected) {
    auto *is_const_int = dynamic_cast<ConstantInt *>(expr);
    auto *is_const_float = dynamic_cast<ConstantFloat *>(expr);
    if (is_const_int != nullptr) {
        if (expected == FLOAT_T)
            return CONST_FLOAT(is_const_int->get_value());
        if (expected == INT1_T)
            return CONST_INT(is_const_int->get_value() != 0);
    }
    if (is_const_float != nullptr) {
        if (expected == INT32_T)
            return CONST_INT(static_cast<int>(is_const_float->get_value()));
        if (expected == INT1_T)
            return CONST_INT(is_const_float->get_value() != 0);
    }
    return expr;
}

Value *IRBuilder::typeConvert(Value *expr, Type *expected) {
    auto *type = expr->get_type();

    if (type == expected)
        return expr;

    if (auto *constant = dynamic_cast<Constant *>(expr))
        return typeConvertConstant(constant, expected);

    // pointer deref
    if (type->is_pointer_type()) {
        type = type->get_pointer_element_type();
        expr = builder->create_load(expr);
    }

    // bool cast to int
    if (type == INT1_T && expected != INT1_T) {
        type = INT32_T;
        expr = builder->create_zext(expr, expected);
    }

    if (type == INT32_T && expected == FLOAT_T)
        return builder->create_sitofp(expr, expected);
    if (type == FLOAT_T && expected == INT32_T)
        return builder->create_fptosi(expr, expected);
    if (type == INT32_T && expected == INT1_T)
        return builder->create_icmp_ne(expr, CONST_INT(0));
    if (type == FLOAT_T && expected == INT1_T)
        return builder->create_fcmp_ne(expr, CONST_FLOAT(0));

    return expr;
}

template <typename T>
Value *IRBuilder::binOpGenConstantT(T lhs, T rhs, BinOp op) {
    if (op.isCondOp()) {
        switch (op.bin_cond_op) {
        case SyntaxTree::BinaryCondOp::LT:
            return CONST_INT(lhs < rhs);
        case SyntaxTree::BinaryCondOp::LTE:
            return CONST_INT(lhs <= rhs);
        case SyntaxTree::BinaryCondOp::GT:
            return CONST_INT(lhs > rhs);
        case SyntaxTree::BinaryCondOp::GTE:
            return CONST_INT(lhs >= rhs);
        case SyntaxTree::BinaryCondOp::EQ:
            return CONST_INT(lhs == rhs);
        case SyntaxTree::BinaryCondOp::NEQ:
            return CONST_INT(lhs != rhs);
        default:
            break;
        }
        return nullptr;
    }
    switch (op.bin_op) {
    case SyntaxTree::BinOp::PLUS:
        return CONST(lhs + rhs);
    case SyntaxTree::BinOp::MINUS:
        return CONST(lhs - rhs);
    case SyntaxTree::BinOp::MULTIPLY:
        return CONST(lhs * rhs);
    case SyntaxTree::BinOp::DIVIDE:
        return CONST(lhs / rhs);
    case SyntaxTree::BinOp::MODULO:
        return CONST(static_cast<int>(lhs) % static_cast<int>(rhs));
    }
    return nullptr;
}

Value *IRBuilder::binOpGenConstant(Constant *lhs, Constant *rhs, BinOp op) {
    if ((dynamic_cast<ConstantFloat *>(lhs) != nullptr) ||
        (dynamic_cast<ConstantFloat *>(rhs) != nullptr)) {
        auto lhs_value = static_cast<ConstantFloat *>(typeConvert(lhs, FLOAT_T))
                             ->get_value();
        auto rhs_value = static_cast<ConstantFloat *>(typeConvert(rhs, FLOAT_T))
                             ->get_value();
        return binOpGenConstantT(lhs_value, rhs_value, op);
    }
    auto lhs_value = dynamic_cast<ConstantInt *>(lhs)->get_value();
    auto rhs_value = dynamic_cast<ConstantInt *>(rhs)->get_value();
    return binOpGenConstantT(lhs_value, rhs_value, op);
}

Value *IRBuilder::binOpGenCreateInst(Value *lhs, Value *rhs, BinOp op) {
    const bool is_int = lhs->get_type()->is_integer_type();
    switch (op.bin_op) {
    case SyntaxTree::BinOp::PLUS:
        return is_int ? builder->create_iadd(lhs, rhs)
                      : builder->create_fadd(lhs, rhs);
    case SyntaxTree::BinOp::MINUS:
        return is_int ? builder->create_isub(lhs, rhs)
                      : builder->create_fsub(lhs, rhs);
    case SyntaxTree::BinOp::MULTIPLY:
        return is_int ? builder->create_imul(lhs, rhs)
                      : builder->create_fmul(lhs, rhs);
    case SyntaxTree::BinOp::DIVIDE:
        return is_int ? builder->create_isdiv(lhs, rhs)
                      : builder->create_fdiv(lhs, rhs);
    case SyntaxTree::BinOp::MODULO:
        return builder->create_isrem(lhs, rhs);
    }
    return nullptr;
}

Value *IRBuilder::binOpGenCreateCondInst(Value *lhs, Value *rhs, BinOp op) {
    const bool is_int = lhs->get_type()->is_integer_type();
    if (is_int) {
        const auto cmp_op = binCondOpToCmpOp.at(op.bin_cond_op);
        return builder->create_icmp(lhs, rhs, cmp_op);
    }
    const auto fcmp_op = binCondOpToFCmpOp.at(op.bin_cond_op);
    return builder->create_fcmp(lhs, rhs, fcmp_op);
}

void IRBuilder::binOpGen(Value *lhs, Value *rhs, BinOp op) {
    // For Both Constants
    auto *lhs_constant = dynamic_cast<Constant *>(lhs);
    auto *rhs_constant = dynamic_cast<Constant *>(rhs);
    if ((lhs_constant != nullptr) && (rhs_constant != nullptr)) {
        prev_expr = binOpGenConstant(lhs_constant, rhs_constant, op);
        return;
    }

    // For pointers
    if (lhs->get_type()->is_pointer_type())
        lhs = builder->create_load(lhs);
    if (rhs->get_type()->is_pointer_type())
        rhs = builder->create_load(rhs);

    // Type convert
    if (lhs->get_type()->is_float_type() || rhs->get_type()->is_float_type()) {
        lhs = typeConvert(lhs, FLOAT_T);
        rhs = typeConvert(rhs, FLOAT_T);
    } else {
        lhs = typeConvert(lhs, INT32_T);
        rhs = typeConvert(rhs, INT32_T);
    }

    if (op.isCondOp())
        prev_expr = binOpGenCreateCondInst(lhs, rhs, op);
    else
        prev_expr = binOpGenCreateInst(lhs, rhs, op);
}

void IRBuilder::visit(SyntaxTree::Assembly &node) {
    VOID_T = Type::get_void_type(module.get());
    INT1_T = Type::get_int1_type(module.get());
    INT32_T = Type::get_int32_type(module.get());
    FLOAT_T = Type::get_float_type(module.get());
    for (const auto &def : node.global_defs)
        def->accept(*this);
}

void IRBuilder::visit(SyntaxTree::FuncDef &node) {
    // 函数声明
    auto *ret_type = SyntaxTreeTytoLLVMTy(node.ret_type);
    std::vector<Type *> params{};
    for (const auto &param : node.param_list->params) {
        params.push_back(SyntaxTreeTytoLLVMTy(param->param_type,
                                              !param->array_index.empty()));
    }
    auto *FunTy = FunctionType::get(ret_type, std::move(params));
    auto *Fun = Function::create(FunTy, node.name, module.get());
    scope.push(node.name, Fun);

    scope.enter();

    // 分配函数参数的空间
    auto *entryBlock = BasicBlock::create(module.get(), "", Fun);
    builder->set_insert_point(entryBlock);
    auto arg = Fun->arg_begin();
    auto arg_end = Fun->arg_end();
    auto param = node.param_list->params.begin();
    auto param_end = node.param_list->params.end();
    while (arg != arg_end && param != param_end) {
        auto name = (*param)->name;
        auto *val = *arg;
        auto *argAlloca = builder->create_alloca(val->get_type());
        builder->create_store(val, argAlloca);
        scope.push(name, argAlloca);
        arg++, param++;
    }

    // 返回值和返回基本块
    if (ret_type != VOID_T)
        ret_addr = builder->create_alloca(ret_type);
    ret_BB = BasicBlock::create(module.get(), "", Fun);
    builder->set_insert_point(ret_BB);
    if (ret_type == VOID_T) {
        builder->create_void_ret();
    } else {
        auto *ret_val = builder->create_load(ret_addr);
        builder->create_ret(ret_val);
    }

    // 访问函数体
    builder->set_insert_point(entryBlock);
    for (const auto &stmt : node.body->body) {
        stmt->accept(*this);
        if (builder->get_insert_block()->get_terminator() != nullptr)
            break;
    }

    // 可能省略了返回语句
    auto *default_ret_val = typeConvert(CONST_INT(0), ret_type);
    if (builder->get_insert_block()->get_terminator() == nullptr) {
        if (ret_type != VOID_T)
            builder->create_store(default_ret_val, ret_addr);
        builder->create_br(ret_BB);
    }

    scope.exit();
}

void IRBuilder::visit(SyntaxTree::VarDef &node) {
    // 计算类型（变量 or 数组）
    auto *element_type = baseTypetoLLVMTy(node.btype);
    auto *type = element_type;
    auto is_array = !node.array_length.empty();
    if (is_array) {
        node.array_length[0]->accept(*this);
        auto *const_int = dynamic_cast<ConstantInt *>(prev_expr);
        type = Type::get_array_type(type, const_int->get_value());
    }

    auto get_array_init = [&]() {
        std::vector<Constant *> initializer_values;
        for (const auto &val : node.initializers->elementList) {
            val->accept(*this);
            prev_expr = typeConvert(prev_expr, element_type);
            initializer_values.push_back(dynamic_cast<Constant *>(prev_expr));
        }
        auto zero_length =
            static_cast<ArrayType *>(type)->get_num_of_elements() -
            static_cast<int>(initializer_values.size());
        auto *const_zero = typeConvert(CONST_INT(0), element_type);
        for (auto i = 0U; i < zero_length; i++)
            initializer_values.push_back(dynamic_cast<Constant *>(const_zero));
        return ConstantArray::get(static_cast<ArrayType *>(type),
                                  initializer_values);
    };

    auto create_glob = [&](const std::string &name, Constant *init) {
        return GlobalVariable::create(name, module.get(), type,
                                      node.is_constant, init);
    };

    Value *addr;
    if (scope.in_global()) {
        if (!node.is_inited ||
            (is_array && node.initializers->elementList.empty())) {
            auto *initializer = ConstantZero::get(type, module.get());
            addr = create_glob(node.name, initializer);
        } else if (is_array) {
            addr = create_glob(node.name, get_array_init());
        } else {
            node.initializers->expr->accept(*this);
            addr = typeConvert(prev_expr, element_type);
            if (!node.is_constant)
                addr = create_glob(node.name, dynamic_cast<Constant *>(addr));
        }
    } else if (!node.is_constant) {
        auto *entryBB = ret_BB->get_parent()->get_entry_block();
        addr = AllocaInst::create_alloca(type, entryBB);
        entryBB->get_instructions().pop_back();
        entryBB->add_instr_begin(static_cast<Instruction *>(addr));
    } else if (is_array) {
        auto fun_name = ret_BB->get_parent()->get_name();
        auto array_name = "__const." + fun_name + "." + node.name;
        while (scope.find(array_name, false) != nullptr)
            array_name += ".1";
        addr = create_glob(array_name, get_array_init());
    } else {
        node.initializers->expr->accept(*this);
        addr = typeConvert(prev_expr, element_type);
    }

    // 保存到符号表
    scope.push(node.name, addr);

    // 处理局部变量及数组初始化
    if ((dynamic_cast<AllocaInst *>(addr) == nullptr) || !node.is_inited) {
        return;
    }
    if (!is_array) {
        node.initializers->expr->accept(*this);
        prev_expr = typeConvert(prev_expr, element_type);
        builder->create_store(prev_expr, addr);
        return;
    }
    auto *const_zero = typeConvert(CONST_INT(0), element_type);
    auto array_size = static_cast<ArrayType *>(type)->get_num_of_elements();
    auto inited_size = static_cast<int>(node.initializers->elementList.size());
    for (auto i = 0; i < static_cast<int>(array_size); i++) {
        if (i < inited_size) {
            node.initializers->elementList[i]->accept(*this);
            prev_expr = typeConvert(prev_expr, element_type);
        } else {
            prev_expr = const_zero;
        }
        auto *ptr = builder->create_gep(addr, {CONST_INT(0), CONST_INT(i)});
        builder->create_store(prev_expr, ptr);
    }
}

void IRBuilder::visit(SyntaxTree::InitVal &node) { node.expr->accept(*this); }

void IRBuilder::visit(SyntaxTree::LVal &node) {
    // const 常量返回值，否则返回地址
    // 首先获取变量（地址）
    auto name = node.name;
    auto *ptr = scope.find(name, false);

    // 对变量名的访问
    if (node.array_index.empty()) {
        if (ptr->get_type()->is_pointer_type() &&
            ptr->get_type()->get_pointer_element_type()->is_array_type()) {
            // 如果是数组，需要解下第一维，转换成指针
            prev_expr = builder->create_gep(ptr, {CONST_INT(0), CONST_INT(0)});
        } else {
            prev_expr = ptr;
        }
        return;
    }

    // 对数组的访问，应该都有左值 ptr
    std::vector<Value *> indexes{};
    bool index_all_const = true;
    // int a[2][3] 相比 int a[][3]，a 多了一个维度，需要补 0
    if (ptr->get_type()->get_pointer_element_type()->is_array_type()) {
        indexes.push_back(CONST_INT(0));
    } else {
        ptr = builder->create_load(ptr);
    }
    for (const auto &index : node.array_index) {
        index->accept(*this);
        auto *const_index = dynamic_cast<ConstantInt *>(prev_expr);
        if (const_index == nullptr) {
            index_all_const = false;
        }
        indexes.push_back(typeConvert(prev_expr, INT32_T));
    }

    // 如果数组是 const，下标也是 const，直接返回 Constant*
    auto *ptr_global = dynamic_cast<GlobalVariable *>(ptr);
    if (ptr_global != nullptr && ptr_global->is_const() && index_all_const) {
        auto *initVal = ptr_global->get_init();
        for (auto iter = ++indexes.begin(); iter != indexes.end(); iter++) {
            initVal = dynamic_cast<ConstantArray *>(initVal)->get_element_value(
                dynamic_cast<ConstantInt *>(*iter)->get_value());
        }
        prev_expr = initVal;
        return;
    }

    // 否则生成 gep
    prev_expr = builder->create_gep(ptr, indexes);
}

void IRBuilder::visit(SyntaxTree::AssignStmt &node) {
    node.target->accept(*this);
    auto *ptr = prev_expr;
    node.value->accept(*this);
    auto *value =
        typeConvert(prev_expr, ptr->get_type()->get_pointer_element_type());
    builder->create_store(value, ptr);
}

void IRBuilder::visit(SyntaxTree::Literal &node) {
    if (node.literal_type == SyntaxTree::Type::FLOAT)
        prev_expr = CONST_FLOAT(node.float_const);
    else if (node.literal_type == SyntaxTree::Type::INT)
        prev_expr = CONST_INT(node.int_const);
}

void IRBuilder::visit(SyntaxTree::ReturnStmt &node) {
    if (node.ret) {
        node.ret->accept(*this);
        auto *expected_type = ret_addr->get_type()->get_pointer_element_type();
        builder->create_store(typeConvert(prev_expr, expected_type), ret_addr);
    }
    builder->create_br(ret_BB);
}

void IRBuilder::visit(SyntaxTree::BlockStmt &node) {
    scope.enter();
    for (const auto &stmt : node.body) {
        stmt->accept(*this);
        if (builder->get_insert_block()->get_terminator() != nullptr)
            break;
    }
    scope.exit();
}

void IRBuilder::visit(SyntaxTree::ExprStmt &node) { node.exp->accept(*this); }

void IRBuilder::visit(SyntaxTree::UnaryCondExpr &node) {
    // 文法上，这个后面只能跟一元算数表达式，无法跟随条件表达式
    // 所以这里可以看成是：“计算”
    node.rhs->accept(*this);
    auto *type = prev_expr->get_type();
    if (type->is_pointer_type()) {
        type = type->get_pointer_element_type();
        prev_expr = builder->create_load(prev_expr);
    }
    if (type == INT32_T) {
        prev_expr = builder->create_icmp_eq(prev_expr, CONST_INT(0));
        return;
    }
    if (type == FLOAT_T) {
        prev_expr = builder->create_fcmp_eq(prev_expr, CONST_FLOAT(0));
        return;
    }
    if (type == INT1_T) {
        if (auto *value = dynamic_cast<ConstantInt *>(prev_expr)) {
            prev_expr = CONST_INT(value->get_value() == 0);
            return;
        }
        auto fcmp_lut = std::map<FCmpInst::CmpOp, FCmpInst::CmpOp>{
            {FCmpInst::EQ, FCmpInst::NE}, {FCmpInst::NE, FCmpInst::EQ},
            {FCmpInst::GT, FCmpInst::LE}, {FCmpInst::LE, FCmpInst::GT},
            {FCmpInst::LT, FCmpInst::GE}, {FCmpInst::GE, FCmpInst::LT}};
        auto cmp_lut = std::map<CmpInst::CmpOp, CmpInst::CmpOp>{
            {CmpInst::EQ, CmpInst::NE}, {CmpInst::NE, CmpInst::EQ},
            {CmpInst::GT, CmpInst::LE}, {CmpInst::LE, CmpInst::GT},
            {CmpInst::LT, CmpInst::GE}, {CmpInst::GE, CmpInst::LT}};
        if (auto *cmp_inst = dynamic_cast<CmpInst *>(prev_expr))
            cmp_inst->set_cmp_op(cmp_lut[cmp_inst->get_cmp_op()]);
        else if (auto *fcmp_inst = dynamic_cast<FCmpInst *>(prev_expr))
            fcmp_inst->set_cmp_op(fcmp_lut[fcmp_inst->get_cmp_op()]);
    }
}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) {
    node.lhs->accept(*this);
    auto *lhs = prev_expr;
    node.rhs->accept(*this);
    auto *rhs = prev_expr;
    binOpGen(lhs, rhs, BinOp(node.op));
}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) {
    node.rhs->accept(*this);
    if (node.op == SyntaxTree::UnaryOp::PLUS)
        return;
    auto *const_int = dynamic_cast<ConstantInt *>(prev_expr);
    auto *const_float = dynamic_cast<ConstantFloat *>(prev_expr);
    if (const_int != nullptr)
        prev_expr = CONST_INT(-const_int->get_value());
    else if (const_float != nullptr)
        prev_expr = CONST_FLOAT(-const_float->get_value());
    else
        binOpGen(CONST_INT(0), prev_expr, BinOp(SyntaxTree::BinOp::MINUS));
}

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {
    auto name = node.name;
    std::vector<Value *> arguments{};
    auto param = node.params.begin();
    auto param_end = node.params.end();
    auto *Fun = dynamic_cast<Function *>(scope.find(name, true));
    auto arg = Fun->arg_begin();
    auto arg_end = Fun->arg_end();
    while (arg != arg_end && param != param_end) {
        (*param)->accept(*this);
        prev_expr = typeConvert(prev_expr, (*arg)->get_type());
        arguments.push_back(prev_expr);
        arg++, param++;
    }
    prev_expr = builder->create_call(Fun, arguments);
}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) {
    auto op = node.op;
    auto *curFunction = ret_BB->get_parent();
    if (op == SyntaxTree::BinaryCondOp::LAND ||
        op == SyntaxTree::BinaryCondOp::LOR) {
        auto *rhsBB = BasicBlock::create(module.get(), "", curFunction);

        auto prevCond = curCondStruct;
        curCondStruct = op == SyntaxTree::BinaryCondOp::LAND
                            ? CondStructType{rhsBB, prevCond.falseBB}
                            : CondStructType{prevCond.trueBB, rhsBB};
        node.lhs->accept(*this);
        if (builder->get_insert_block()->get_terminator() == nullptr) {
            prev_expr = typeConvert(prev_expr, INT1_T);
            builder->create_cond_br(prev_expr, curCondStruct.trueBB,
                                    curCondStruct.falseBB);
        }
        curCondStruct = prevCond;

        builder->set_insert_point(rhsBB);
        node.rhs->accept(*this);
        if (builder->get_insert_block()->get_terminator() == nullptr) {
            prev_expr = typeConvert(prev_expr, INT1_T);
            builder->create_cond_br(prev_expr, curCondStruct.trueBB,
                                    curCondStruct.falseBB);
        }
        return;
    }
    node.lhs->accept(*this);
    auto *lhs = prev_expr;
    node.rhs->accept(*this);
    auto *rhs = prev_expr;
    binOpGen(lhs, rhs, BinOp(node.op));
}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {
    auto *curFunction = ret_BB->get_parent();
    auto *trueBB = BasicBlock::create(module.get(), "", curFunction);
    auto *falseBB = BasicBlock::create(module.get(), "", curFunction);
    auto *exitBB = node.else_statement
                       ? BasicBlock::create(module.get(), "", curFunction)
                       : falseBB;

    const auto prev_cond_struct = curCondStruct;
    curCondStruct = CondStructType{trueBB, falseBB};
    node.cond_exp->accept(*this);
    if (builder->get_insert_block()->get_terminator() == nullptr) {
        prev_expr = typeConvert(prev_expr, INT1_T);
        builder->create_cond_br(prev_expr, trueBB, falseBB);
    }
    curCondStruct = prev_cond_struct;

    builder->set_insert_point(trueBB);
    node.if_statement->accept(*this);
    if (builder->get_insert_block()->get_terminator() == nullptr)
        builder->create_br(exitBB);

    if (node.else_statement) {
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if (builder->get_insert_block()->get_terminator() == nullptr)
            builder->create_br(exitBB);
    }

    builder->set_insert_point(exitBB);
    if (exitBB->get_pre_basic_blocks().empty()) {
        builder->set_insert_point(trueBB);
        exitBB->erase_from_parent();
    }
}

void IRBuilder::visit(SyntaxTree::WhileStmt &node) {
    auto *curFunction = ret_BB->get_parent();
    auto *condBB = BasicBlock::create(module.get(), "", curFunction);
    auto *innerBB = BasicBlock::create(module.get(), "", curFunction);
    auto *exitBB = BasicBlock::create(module.get(), "", curFunction);

    const auto prev_while_struct = curWhileStruct;
    curWhileStruct = WhileStructType{condBB, innerBB, exitBB};

    if (builder->get_insert_block()->get_terminator() == nullptr)
        builder->create_br(condBB);
    builder->set_insert_point(condBB);

    const auto prev_cond_struct = curCondStruct;
    curCondStruct = CondStructType{innerBB, exitBB};
    node.cond_exp->accept(*this);
    if (builder->get_insert_block()->get_terminator() == nullptr) {
        prev_expr = typeConvert(prev_expr, INT1_T);
        builder->create_cond_br(prev_expr, innerBB, exitBB);
    }
    curCondStruct = prev_cond_struct;

    builder->set_insert_point(innerBB);
    node.statement->accept(*this);
    if (builder->get_insert_block()->get_terminator() == nullptr)
        builder->create_br(condBB);

    builder->set_insert_point(exitBB);

    curWhileStruct = prev_while_struct;
}

void IRBuilder::visit(SyntaxTree::BreakStmt & /*node*/) {
    builder->create_br(curWhileStruct.exitBB);
}

void IRBuilder::visit(SyntaxTree::ContinueStmt & /*node*/) {
    builder->create_br(curWhileStruct.condBB);
}

// empty
void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {}
void IRBuilder::visit(SyntaxTree::FuncParam &node) {}
void IRBuilder::visit(SyntaxTree::EmptyStmt &node) {}
