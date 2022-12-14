#include "IRBuilder.h"

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

// You can define global variables here
// to store state

// store variable type
Type *var_type = nullptr;
// store temporary value
Value *tmp_val = nullptr;
// whether require lvalue
bool require_lvalue = false;
// function that is being built
Function *cur_fun = nullptr;
// detect scope pre-enter (for elegance only)
bool pre_enter_scope = false;

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *FLOAT_T;
Type *INT32PTR_T;
Type *FLOATPTR_T;

/* Global Variable */

// used for backpatching
struct true_false_BB {
    BasicBlock *trueBB = nullptr;
    BasicBlock *falseBB = nullptr;
};

std::list<true_false_BB> IF_While_And_Cond_Stack;  // used for Cond
std::list<true_false_BB> IF_While_Or_Cond_Stack;   // used for Cond
std::list<true_false_BB> While_Stack;              // used for break and continue
// used for backpatching
std::vector<BasicBlock *> cur_basic_block_list;
std::vector<SyntaxTree::FuncParam> func_params;

std::vector<Constant *> init_val;
std::map<Value *, Value *> local2global;

//ret BB
BasicBlock *ret_BB;
Value *ret_addr;

/* Global Variable */

bool match_type(IRStmtBuilder *builder, Value **lhs_ptr, Value **rhs_ptr) {
    bool both_int;
    auto &lhs = *lhs_ptr;
    auto &rhs = *rhs_ptr;
    if (lhs->get_type() == rhs->get_type()) {
        both_int = lhs->get_type()->is_integer_type();
    } else {
        both_int = false;
        if (lhs->get_type()->is_integer_type()) {
            lhs = builder->create_sitofp(lhs, FLOAT_T);
        } else if (rhs->get_type()->is_integer_type()) {
            rhs = builder->create_sitofp(rhs, FLOAT_T);
        }
    }
    return both_int;
}

void IRBuilder::visit(SyntaxTree::Assembly &node) {
    VOID_T = Type::get_void_type(module.get());
    INT1_T = Type::get_int1_type(module.get());
    INT32_T = Type::get_int32_type(module.get());
    FLOAT_T = Type::get_float_type(module.get());
    INT32PTR_T = Type::get_int32_ptr_type(module.get());
    FLOATPTR_T = Type::get_float_ptr_type(module.get());
    for (const auto &def : node.global_defs) {
        def->accept(*this);
    }
}

void IRBuilder::visit(SyntaxTree::InitVal &node) {
    // simple ver
    if (node.isExp) {
        node.expr->accept(*this);
        auto tmp_int32_val = dynamic_cast<ConstantInt *>(tmp_val);
        auto tmp_float_val = dynamic_cast<ConstantFloat *>(tmp_val);
        if (var_type == INT32_T && tmp_float_val != nullptr) {
            if (tmp_float_val)
            tmp_val = CONST_INT(int(tmp_float_val->get_value()));
        } else if (var_type == FLOAT_T && tmp_int32_val != nullptr) {
            tmp_val = CONST_FLOAT(float(tmp_int32_val->get_value()));
        }
        init_val.push_back(dynamic_cast<Constant *>(tmp_val));
    }
    else {
        for (const auto &elem : node.elementList){
            elem->accept(*this);
        }
    }
}

void IRBuilder::visit(SyntaxTree::FuncDef &node) {
    FunctionType *fun_type;
    Type *ret_type;
    if (node.ret_type == SyntaxTree::Type::INT)
        ret_type = INT32_T;
    else if (node.ret_type == SyntaxTree::Type::FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    std::vector<Type *> param_types;
    std::vector<SyntaxTree::FuncParam>().swap(func_params);
    node.param_list->accept(*this);
    for (const auto &param : func_params) {
        if (param.param_type == SyntaxTree::Type::INT) {
            if (param.array_index.empty()) {
                param_types.push_back(INT32_T);
            } else {
                param_types.push_back(INT32PTR_T);
            }
        } else if (param.param_type == SyntaxTree::Type::FLOAT) {
            if (param.array_index.empty()) {
                param_types.push_back(FLOAT_T);
            } else {
                param_types.push_back(FLOATPTR_T);
            }
        }
    }
    fun_type = FunctionType::get(ret_type, param_types);
    auto fun = Function::create(fun_type, node.name, module.get());
    scope.push(node.name, fun);
    cur_fun = fun;
    auto funBB = BasicBlock::create(module.get(), "entry", fun);
    builder->set_insert_point(funBB);
    cur_basic_block_list.push_back(funBB);
    scope.enter();
    pre_enter_scope = true;

    //ret BB
    if (ret_type == INT32_T) {
        ret_addr = builder->create_alloca(INT32_T);
    } else if (ret_type == FLOAT_T) {
        ret_addr = builder->create_alloca(FLOAT_T);
    }
    ret_BB = BasicBlock::create(module.get(), "ret", fun);

    std::vector<Value *> args;
    for (auto arg = fun->arg_begin(); arg != fun->arg_end(); arg++) {
        args.push_back(*arg);
    }
    int param_num = func_params.size();
    for (int i = 0; i < param_num; i++) {
        if (func_params[i].array_index.empty()) {
            Value *alloc;
            if (func_params[i].param_type == SyntaxTree::Type::INT) {
                alloc = builder->create_alloca(INT32_T);
            } else if (func_params[i].param_type == SyntaxTree::Type::FLOAT) {
                alloc = builder->create_alloca(FLOAT_T);
            }
            builder->create_store(args[i], alloc);
            scope.push(func_params[i].name, alloc);
        } else {
            Value *alloc_array;
            if (func_params[i].param_type == SyntaxTree::Type::INT) {
                alloc_array = builder->create_alloca(INT32PTR_T);
            } else if (func_params[i].param_type == SyntaxTree::Type::FLOAT) {
                alloc_array = builder->create_alloca(FLOATPTR_T);
            }
            builder->create_store(args[i], alloc_array);
            scope.push(func_params[i].name, alloc_array);
        }
    }

    node.body->accept(*this);

    if (builder->get_insert_block()->get_terminator() == nullptr) {
        if (cur_fun->get_return_type()->is_void_type()) {
            builder->create_br(ret_BB);
        } else if (cur_fun->get_return_type()->is_integer_type()) {
            builder->create_store(CONST_INT(0), ret_addr);
            builder->create_br(ret_BB);
        } else if (cur_fun->get_return_type()->is_float_type()) {
            builder->create_store(CONST_FLOAT(0), ret_addr);
            builder->create_br(ret_BB);
        }
    }
    scope.exit();
    cur_basic_block_list.pop_back();

    //ret BB
    builder->set_insert_point(ret_BB);
    if (fun->get_return_type() == VOID_T) {
        builder->create_void_ret();
    } else {
        auto ret_val = builder->create_load(ret_addr);
        builder->create_ret(ret_val);
    }
}

void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {
    for (const auto &Param : node.params) {
        Param->accept(*this);
    }
}

void IRBuilder::visit(SyntaxTree::FuncParam &node) {
    func_params.push_back(node);
}

void IRBuilder::visit(SyntaxTree::VarDef &node) {
    // Type *var_type;
    if (node.btype == SyntaxTree::Type::INT) {
        var_type = INT32_T;
    } else if (node.btype == SyntaxTree::Type::FLOAT) {
        var_type = FLOAT_T;
    }
    BasicBlock *cur_fun_entry_block;
    BasicBlock *cur_fun_cur_block;
    if (scope.in_global() == false) {
        cur_fun_entry_block = cur_fun->get_entry_block();  // entry block
        cur_fun_cur_block = cur_basic_block_list.back();   // current block
    }
    if (node.is_constant) {
        // constant
        Value *var;
        if (node.array_length.empty()) {
            node.initializers->accept(*this);
            if (var_type == INT32_T) {
                if (dynamic_cast<ConstantFloat *>(tmp_val)) {
                    tmp_val = CONST_INT(int(dynamic_cast<ConstantFloat *>(tmp_val)->get_value()));
                }
                auto initializer = dynamic_cast<ConstantInt *>(tmp_val)->get_value();
                var = ConstantInt::get(initializer, module.get());
            } else if (var_type == FLOAT_T) {
                if (dynamic_cast<ConstantInt *>(tmp_val)) {
                    tmp_val = CONST_FLOAT(float(dynamic_cast<ConstantInt *>(tmp_val)->get_value()));
                }
                auto initializer = dynamic_cast<ConstantFloat *>(tmp_val)->get_value();
                var = ConstantFloat::get(initializer, module.get());
            }
            scope.push(node.name, var);
        } else {
            //simple ver
            int length;
            for (auto length_expr : node.array_length){
                length_expr->accept(*this);
                auto length_const = dynamic_cast<ConstantInt *>(tmp_val);
                length = length_const->get_value();
            }
            auto *array_type = ArrayType::get(var_type, length);
            init_val.clear();
            node.initializers->accept(*this);
            if (scope.in_global()) {
                int cur_size = init_val.size();
                while (cur_size < length){
                    if (var_type == INT32_T) {
                        init_val.push_back(CONST_INT(0));
                    } else if (var_type == FLOAT_T) {
                        init_val.push_back(CONST_FLOAT(0));
                    }
                    cur_size++;
                }
                auto initializer = ConstantArray::get(array_type, init_val);
                var = GlobalVariable::create(node.name, module.get(), array_type, true, initializer);
                scope.push(node.name, var);
            }
            else {
                auto tmp_terminator = cur_fun_entry_block->get_terminator();
                if (tmp_terminator != nullptr) {
                    cur_fun_entry_block->get_instructions().pop_back();
                }
                var = builder->create_alloca(array_type);
                cur_fun_cur_block->get_instructions().pop_back();
                cur_fun_entry_block->add_instruction(dynamic_cast<Instruction *>(var));
                dynamic_cast<Instruction *>(var)->set_parent(cur_fun_entry_block);
                if (tmp_terminator != nullptr) {
                    cur_fun_entry_block->add_instruction(tmp_terminator);
                }
                int pos = 0;
                for (auto init : init_val){
                    builder->create_store(init, builder->create_gep(var, {CONST_INT(0), CONST_INT(pos)}));
                    pos++;
                }
                while (pos < length){
                    if (var_type == INT32_T) {
                        builder->create_store(CONST_INT(0), builder->create_gep(var, {CONST_INT(0), CONST_INT(pos)}));
                    } else if (var_type == FLOAT_T) {
                        builder->create_store(CONST_FLOAT(0), builder->create_gep(var, {CONST_INT(0), CONST_INT(pos)}));
                    }
                    pos++;
                }
                scope.push(node.name, var);
                //add a const global array as llvm
                int cur_size = init_val.size();
                while (cur_size < length){
                    if (var_type == INT32_T) {
                        init_val.push_back(CONST_INT(0));
                    } else if (var_type == FLOAT_T) {
                        init_val.push_back(CONST_FLOAT(0));
                    }
                    cur_size++;
                }
                auto initializer = ConstantArray::get(array_type, init_val);
                std::string const_name = "__const." + cur_fun->get_name() + "." + node.name;
                std::string const_num = "";
                int suffix = 0;
                while (scope.find(const_name + const_num,false) != nullptr){
                    suffix ++ ;
                    const_num = "." + std::to_string(suffix);
                }
                auto const_var = GlobalVariable::create(const_name + const_num, module.get(), array_type, true, initializer);
                scope.push(const_name + const_num, const_var);
                local2global[var] = const_var;
            }
        }
    } else {
        if (node.array_length.empty()) {
            Value *var;
            if (scope.in_global()) {
                if (node.is_inited) {
                    node.initializers->accept(*this);
                    Constant *initializer;
                    if (var_type == INT32_T) {
                        if (dynamic_cast<ConstantFloat *>(tmp_val)) {
                            tmp_val = CONST_INT(int(dynamic_cast<ConstantFloat *>(tmp_val)->get_value()));
                        }
                        initializer = dynamic_cast<ConstantInt *>(tmp_val);
                    } else if (var_type == FLOAT_T) {
                        if (dynamic_cast<ConstantInt *>(tmp_val)) {
                            tmp_val = CONST_FLOAT(float(dynamic_cast<ConstantInt *>(tmp_val)->get_value()));
                        }
                        initializer = dynamic_cast<ConstantFloat *>(tmp_val);
                    }
                    var = GlobalVariable::create(node.name, module.get(), var_type, false, initializer);
                    scope.push(node.name, var);
                } else {
                    auto initializer = ConstantZero::get(var_type, module.get());
                    var = GlobalVariable::create(node.name, module.get(), var_type, false, initializer);
                    scope.push(node.name, var);
                }
            } else {
                auto tmp_terminator = cur_fun_entry_block->get_terminator();
                if (tmp_terminator != nullptr) {
                    cur_fun_entry_block->get_instructions().pop_back();
                }
                var = builder->create_alloca(var_type);
                cur_fun_cur_block->get_instructions().pop_back();
                cur_fun_entry_block->add_instruction(dynamic_cast<Instruction *>(var));
                if (tmp_terminator != nullptr) {
                    cur_fun_entry_block->add_instruction(tmp_terminator);
                }
                if (node.is_inited) {
                    node.initializers->accept(*this);
                    if (var->get_type()->get_pointer_element_type()->is_integer_type() && tmp_val->get_type()->is_float_type()) {
                        tmp_val = builder->create_fptosi(tmp_val, INT32_T);
                    } else if (var->get_type()->get_pointer_element_type()->is_float_type() && tmp_val->get_type()->is_integer_type()) {
                        tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
                    }
                    builder->create_store(tmp_val, var);
                }
                scope.push(node.name, var);
            }
        } else {
            //simple ver
            int length;
            for (auto length_expr : node.array_length){
                length_expr->accept(*this);
                auto length_const = dynamic_cast<ConstantInt *>(tmp_val);
                length = length_const->get_value();
            }
            auto *array_type = ArrayType::get(var_type, length);
            Value *var;
            if (scope.in_global()) {
                if (node.is_inited) {
                    init_val.clear();
                    node.initializers->accept(*this);
                    int cur_size = init_val.size();
                    while (cur_size < length){
                        if (var_type == INT32_T) {
                            init_val.push_back(CONST_INT(0));
                        } else if (var_type == FLOAT_T) {
                            init_val.push_back(CONST_FLOAT(0));
                        }
                        cur_size++;
                    }
                    auto initializer = ConstantArray::get(array_type, init_val);
                    var = GlobalVariable::create(node.name, module.get(), array_type, false, initializer);
                    scope.push(node.name, var);
                } else {
                    auto initializer = ConstantZero::get(array_type, module.get());
                    var = GlobalVariable::create(node.name, module.get(), array_type, false, initializer);
                    scope.push(node.name, var);
                }
            }
            else {
                auto tmp_terminator = cur_fun_entry_block->get_terminator();
                if (tmp_terminator != nullptr) {
                    cur_fun_entry_block->get_instructions().pop_back();
                }
                var = builder->create_alloca(array_type);
                cur_fun_cur_block->get_instructions().pop_back();
                cur_fun_entry_block->add_instruction(dynamic_cast<Instruction *>(var));
                if (tmp_terminator != nullptr) {
                    cur_fun_entry_block->add_instruction(tmp_terminator);
                }
                if (node.is_inited) {
                    init_val.clear();
                    node.initializers->accept(*this);
                    int pos = 0;
                    for (auto init : init_val){
                        builder->create_store(init, builder->create_gep(var, {CONST_INT(0), CONST_INT(pos)}));
                        pos++;
                    }
                    while (pos < length){
                        if (var_type == INT32_T) {
                            builder->create_store(CONST_INT(0), builder->create_gep(var, {CONST_INT(0), CONST_INT(pos)}));
                        } else if (var_type == FLOAT_T) {
                            builder->create_store(CONST_FLOAT(0), builder->create_gep(var, {CONST_INT(0), CONST_INT(pos)}));
                        }
                        pos++;
                    }
                }
                scope.push(node.name, var);
            }
        }
    }
}

void IRBuilder::visit(SyntaxTree::AssignStmt &node) {
    node.value->accept(*this);
    auto result = tmp_val;
    require_lvalue = true;
    node.target->accept(*this);
    auto addr = tmp_val;
    if (addr->get_type()->get_pointer_element_type()->is_integer_type() && result->get_type()->is_float_type()) {
        result = builder->create_fptosi(result, INT32_T);
    } else if (addr->get_type()->get_pointer_element_type()->is_float_type() && result->get_type()->is_integer_type()) {
        result = builder->create_sitofp(result, FLOAT_T);
    }
    builder->create_store(result, addr);
    tmp_val = result;
}

void IRBuilder::visit(SyntaxTree::LVal &node) {
    auto var = scope.find(node.name,false);
    bool should_return_lvalue = require_lvalue;
    require_lvalue = false;
    if (node.array_index.empty()) {
        if (should_return_lvalue) {
            if (var->get_type()->get_pointer_element_type()->is_array_type()) {
                tmp_val = builder->create_gep(var, {CONST_INT(0), CONST_INT(0)});
            } else if (var->get_type()->get_pointer_element_type()->is_pointer_type()) {
                tmp_val = builder->create_load(var);
            } else {
                tmp_val = var;
            }
            require_lvalue = false;
        } else {
            auto val_const_int = dynamic_cast<ConstantInt *>(var);
            auto val_const_float = dynamic_cast<ConstantFloat *>(var);
            if (val_const_int != nullptr) {
                tmp_val = val_const_int;
            } else if (val_const_float != nullptr) {
                tmp_val = val_const_float;
            } else {
                tmp_val = builder->create_load(var);
            }
        }
    } else {
        //simple ver
        node.array_index[0]->accept(*this);
        auto index = tmp_val;
        auto const_index = dynamic_cast<ConstantInt *>(index);
        if (const_index!=nullptr){
            //global const array
            auto global_var = dynamic_cast<GlobalVariable *>(var);
            if (global_var!=nullptr){
                if (global_var->is_const()){
                    auto init = global_var->get_init();
                    auto init_array = dynamic_cast<ConstantArray *>(init);
                    auto index_value = const_index->get_value();
                    tmp_val = init_array->get_element_value(index_value);
                    return ;
                }
            }
            //local const array
            auto const_var = local2global[var];
            if (const_var!=nullptr){
                auto global_const_var = dynamic_cast<GlobalVariable *>(const_var);
                if (global_const_var!=nullptr){
                    if (global_const_var->is_const()){
                        auto init = global_const_var->get_init();
                        auto init_array = dynamic_cast<ConstantArray *>(init);
                        auto index_value = const_index->get_value();
                        tmp_val = init_array->get_element_value(index_value);
                        return ;
                    }
                }
            }
        }
        Value *tmp_ptr;
        if (var->get_type()->get_pointer_element_type()->is_pointer_type()) {
            auto tmp_load = builder->create_load(var);
            tmp_ptr = builder->create_gep(tmp_load, {index});
        }
        else {
            tmp_ptr = builder->create_gep(var, {CONST_INT(0), index});
        }
        if (should_return_lvalue) {
            tmp_val = tmp_ptr;
            require_lvalue = false;
        }
        else {
            tmp_val = builder->create_load(tmp_ptr);
        }
    }
}

void IRBuilder::visit(SyntaxTree::Literal &node) {
    if (node.literal_type == SyntaxTree::Type::INT) {
        tmp_val = CONST_INT(node.int_const);
    } else if (node.literal_type == SyntaxTree::Type::FLOAT) {
        tmp_val = CONST_FLOAT(node.float_const);
    }
}

void IRBuilder::visit(SyntaxTree::ReturnStmt &node) {
    if (node.ret == nullptr) {
        builder->create_br(ret_BB);
    } else {
        node.ret->accept(*this);
        if (ret_addr->get_type()->get_pointer_element_type()->is_integer_type() && tmp_val->get_type()->is_float_type()) {
            tmp_val = builder->create_fptosi(tmp_val, INT32_T);
        } else if (ret_addr->get_type()->get_pointer_element_type()->is_float_type() && tmp_val->get_type()->is_integer_type()) {
            tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
        }
        builder->create_store(tmp_val, ret_addr);
        builder->create_br(ret_BB);
    }
}

void IRBuilder::visit(SyntaxTree::BlockStmt &node) {
    bool need_exit_scope = !pre_enter_scope;
    if (pre_enter_scope) {
        pre_enter_scope = false;
    } else {
        scope.enter();
    }
    for (auto &decl : node.body) {
        decl->accept(*this);
        if (builder->get_insert_block()->get_terminator() != nullptr)
            break;
    }
    if (need_exit_scope) {
        scope.exit();
    }
}

void IRBuilder::visit(SyntaxTree::EmptyStmt &node) { tmp_val = nullptr; }

void IRBuilder::visit(SyntaxTree::ExprStmt &node) { node.exp->accept(*this); }

void IRBuilder::visit(SyntaxTree::UnaryCondExpr &node) {
    if (node.op == SyntaxTree::UnaryCondOp::NOT) {
        node.rhs->accept(*this);
        auto r_val = tmp_val;
        if (r_val->get_type()->is_integer_type()) {
            tmp_val = builder->create_icmp_eq(r_val, CONST_INT(0));
        } else if (r_val->get_type()->is_float_type()) {
            tmp_val = builder->create_fcmp_eq(r_val, CONST_FLOAT(0));
        }
        tmp_val = builder->create_zext(tmp_val, INT32_T);
    }
}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) {
    CmpInst *cond_val;
    FCmpInst *f_cond_val;
    if (node.op == SyntaxTree::BinaryCondOp::LAND) {
        auto trueBB = BasicBlock::create(module.get(), "", cur_fun);
        IF_While_And_Cond_Stack.push_back({trueBB, IF_While_Or_Cond_Stack.back().falseBB});
        node.lhs->accept(*this);
        IF_While_And_Cond_Stack.pop_back();
        cond_val = dynamic_cast<CmpInst *>(tmp_val);
        f_cond_val = dynamic_cast<FCmpInst *>(tmp_val);
        if (tmp_val->get_type()->is_integer_type() || cond_val != nullptr) {
            if (cond_val == nullptr) {
                cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
            }
            builder->create_cond_br(cond_val, trueBB, IF_While_Or_Cond_Stack.back().falseBB);
        } else if (tmp_val->get_type()->is_float_type() || f_cond_val != nullptr) {
            if (f_cond_val == nullptr) {
                f_cond_val = builder->create_fcmp_ne(tmp_val, CONST_FLOAT(0));
            }
            builder->create_cond_br(f_cond_val, trueBB, IF_While_Or_Cond_Stack.back().falseBB);
        }
        builder->set_insert_point(trueBB);
        node.rhs->accept(*this);
    } else if (node.op == SyntaxTree::BinaryCondOp::LOR) {
        auto falseBB = BasicBlock::create(module.get(), "", cur_fun);
        IF_While_Or_Cond_Stack.push_back({IF_While_Or_Cond_Stack.back().trueBB, falseBB});
        node.lhs->accept(*this);
        IF_While_Or_Cond_Stack.pop_back();
        cond_val = dynamic_cast<CmpInst *>(tmp_val);
        f_cond_val = dynamic_cast<FCmpInst *>(tmp_val);
        if (tmp_val->get_type()->is_integer_type() || cond_val != nullptr) {
            if (cond_val == nullptr) {
                cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
            }
            builder->create_cond_br(cond_val, IF_While_Or_Cond_Stack.back().trueBB, falseBB);
        } else if (tmp_val->get_type()->is_float_type() || f_cond_val != nullptr) {
            if (f_cond_val == nullptr) {
                f_cond_val = builder->create_fcmp_ne(tmp_val, CONST_FLOAT(0));
            }
            builder->create_cond_br(f_cond_val, IF_While_Or_Cond_Stack.back().trueBB, falseBB);
        }
        builder->set_insert_point(falseBB);
        node.rhs->accept(*this);
    } else {
        node.lhs->accept(*this);
        auto l_val = tmp_val;
        node.rhs->accept(*this);
        auto r_val = tmp_val;
        bool both_int = match_type(builder, &l_val, &r_val);
        Value *cmp;
        switch (node.op) {
            case SyntaxTree::BinaryCondOp::LT:
                if (both_int) {
                    cmp = builder->create_icmp_lt(l_val, r_val);
                } else {
                    cmp = builder->create_fcmp_lt(l_val, r_val);
                }
                break;
            case SyntaxTree::BinaryCondOp::LTE:
                if (both_int) {
                    cmp = builder->create_icmp_le(l_val, r_val);
                } else {
                    cmp = builder->create_fcmp_le(l_val, r_val);
                }
                break;
            case SyntaxTree::BinaryCondOp::GTE:
                if (both_int) {
                    cmp = builder->create_icmp_ge(l_val, r_val);
                } else {
                    cmp = builder->create_fcmp_ge(l_val, r_val);
                }
                break;
            case SyntaxTree::BinaryCondOp::GT:
                if (both_int) {
                    cmp = builder->create_icmp_gt(l_val, r_val);
                } else {
                    cmp = builder->create_fcmp_gt(l_val, r_val);
                }
                break;
            case SyntaxTree::BinaryCondOp::EQ:
                if (both_int) {
                    cmp = builder->create_icmp_eq(l_val, r_val);
                } else {
                    cmp = builder->create_fcmp_eq(l_val, r_val);
                }
                break;
            case SyntaxTree::BinaryCondOp::NEQ:
                if (both_int) {
                    cmp = builder->create_icmp_ne(l_val, r_val);
                } else {
                    cmp = builder->create_fcmp_ne(l_val, r_val);
                }
                break;
            default:
                break;
        }
        tmp_val = builder->create_zext(cmp, INT32_T);
    }
}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) {
    if (node.rhs == nullptr) {
        node.lhs->accept(*this);
    } else {
        node.lhs->accept(*this);
        auto l_val_const_i = dynamic_cast<ConstantInt *>(tmp_val);
        auto l_val_const_f = dynamic_cast<ConstantFloat *>(tmp_val);
        auto l_val = tmp_val;
        node.rhs->accept(*this);
        auto r_val_const_i = dynamic_cast<ConstantInt *>(tmp_val);
        auto r_val_const_f = dynamic_cast<ConstantFloat *>(tmp_val);
        auto r_val = tmp_val;
        bool both_const = ((l_val_const_i != nullptr || l_val_const_f != nullptr) && (r_val_const_i != nullptr || r_val_const_f != nullptr));
        if (both_const) {
            bool both_int = (l_val_const_i != nullptr && r_val_const_i != nullptr);
            bool both_float = (l_val_const_f != nullptr && r_val_const_f != nullptr);
            int l_val_const_int, r_val_const_int;
            float l_val_const_float, r_val_const_float;
            if (both_int) {
                l_val_const_int = l_val_const_i->get_value();
                r_val_const_int = r_val_const_i->get_value();
            } else if (both_float) {
                l_val_const_float = l_val_const_f->get_value();
                r_val_const_float = r_val_const_f->get_value();
            } else {
                if (l_val_const_i == nullptr) {
                    l_val_const_float = l_val_const_f->get_value();
                    r_val_const_float = float(r_val_const_i->get_value());
                } else if (r_val_const_i == nullptr) {
                    l_val_const_float = float(l_val_const_i->get_value());
                    r_val_const_float = r_val_const_f->get_value();
                }
            }
            switch (node.op) {
                case SyntaxTree::BinOp::PLUS:
                    if (both_int) {
                        tmp_val = CONST_INT(l_val_const_int + r_val_const_int);
                    } else {
                        tmp_val = CONST_FLOAT(l_val_const_float + r_val_const_float);
                    }
                    break;
                case SyntaxTree::BinOp::MINUS:
                    if (both_int) {
                        tmp_val = CONST_INT(l_val_const_int - r_val_const_int);
                    } else {
                        tmp_val = CONST_FLOAT(l_val_const_float - r_val_const_float);
                    }
                    break;
                case SyntaxTree::BinOp::MULTIPLY:
                    if (both_int) {
                        tmp_val = CONST_INT(l_val_const_int * r_val_const_int);
                    } else {
                        tmp_val = CONST_FLOAT(l_val_const_float * r_val_const_float);
                    }
                    break;
                case SyntaxTree::BinOp::DIVIDE:
                    if (both_int) {
                        tmp_val = CONST_INT(l_val_const_int / r_val_const_int);
                    } else {
                        tmp_val = CONST_FLOAT(l_val_const_float / r_val_const_float);
                    }
                    break;
                case SyntaxTree::BinOp::MODULO:
                    if (both_int) {
                        tmp_val = CONST_INT(l_val_const_int % r_val_const_int);
                    } else {
                        std::cerr << "invalid operands" << std::endl;
                        exit(-1);
                    }
                    break;
            }
        } else {
            bool both_int = match_type(builder, &l_val, &r_val);
            switch (node.op) {
                case SyntaxTree::BinOp::PLUS:
                    if (both_int) {
                        tmp_val = builder->create_iadd(l_val, r_val);
                    } else {
                        tmp_val = builder->create_fadd(l_val, r_val);
                    }
                    break;
                case SyntaxTree::BinOp::MINUS:
                    if (both_int) {
                        tmp_val = builder->create_isub(l_val, r_val);
                    } else {
                        tmp_val = builder->create_fsub(l_val, r_val);
                    }
                    break;
                case SyntaxTree::BinOp::MULTIPLY:
                    if (both_int) {
                        tmp_val = builder->create_imul(l_val, r_val);
                    } else {
                        tmp_val = builder->create_fmul(l_val, r_val);
                    }
                    break;
                case SyntaxTree::BinOp::DIVIDE:
                    if (both_int) {
                        tmp_val = builder->create_isdiv(l_val, r_val);
                    } else {
                        tmp_val = builder->create_fdiv(l_val, r_val);
                    }
                    break;
                case SyntaxTree::BinOp::MODULO:
                    if (both_int) {
                        tmp_val = builder->create_isrem(l_val, r_val);
                    } else {
                        std::cerr << "invalid operands" << std::endl;
                        exit(-1);
                    }
                    break;
            }
        }
    }
}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) {
    node.rhs->accept(*this);
    if (node.op == SyntaxTree::UnaryOp::MINUS) {
        auto val_const_i = dynamic_cast<ConstantInt *>(tmp_val);
        auto val_const_f = dynamic_cast<ConstantFloat *>(tmp_val);
        auto r_val = tmp_val;
        if (val_const_i != nullptr) {
            tmp_val = CONST_INT(0 - val_const_i->get_value());
        } else if (val_const_f != nullptr) {
            tmp_val = CONST_FLOAT(0 - val_const_f->get_value());
        } else {
            if (r_val->get_type()->is_integer_type()) {
                tmp_val = builder->create_isub(CONST_INT(0), r_val);
            } else if (r_val->get_type()->is_float_type()) {
                tmp_val = builder->create_fsub(CONST_FLOAT(0), r_val);
            }
        }
    }
}

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {
    auto fun = dynamic_cast<Function *>(scope.find(node.name,true));
    std::vector<Value *> params;
    int i = 0;
    for (auto &param : node.params) {
        auto param_type = fun->get_function_type()->get_param_type(i++);
        if (param_type->is_integer_type() || param_type->is_float_type()) {
            require_lvalue = false;
        } else {
            require_lvalue = true;
        }
        param->accept(*this);
        require_lvalue = false;
        if (param_type->is_float_type() && tmp_val->get_type()->is_integer_type()) {
            auto const_tmp_val_i = dynamic_cast<ConstantInt *>(tmp_val);
            if (const_tmp_val_i != nullptr) {
                tmp_val = CONST_FLOAT(float(const_tmp_val_i->get_value()));
            } else {
                tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
            }
        } else if (param_type->is_integer_type() && tmp_val->get_type()->is_float_type()) {
            auto const_tmp_val_f = dynamic_cast<ConstantFloat *>(tmp_val);
            if (const_tmp_val_f != nullptr) {
                tmp_val = CONST_INT(int(const_tmp_val_f->get_value()));
            } else {
                tmp_val = builder->create_fptosi(tmp_val, INT32_T);
            }
        }
        params.push_back(tmp_val);
    }
    tmp_val = builder->create_call(fun, params);
}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {
    auto trueBB = BasicBlock::create(module.get(), "", cur_fun);
    auto falseBB = BasicBlock::create(module.get(), "", cur_fun);
    auto nextBB = BasicBlock::create(module.get(), "", cur_fun);
    IF_While_Or_Cond_Stack.push_back({nullptr, nullptr});
    IF_While_Or_Cond_Stack.back().trueBB = trueBB;
    if (node.else_statement == nullptr) {
        IF_While_Or_Cond_Stack.back().falseBB = nextBB;
    } else {
        IF_While_Or_Cond_Stack.back().falseBB = falseBB;
    }
    node.cond_exp->accept(*this);
    IF_While_Or_Cond_Stack.pop_back();
    auto ret_val = tmp_val;
    auto cond_val = dynamic_cast<CmpInst *>(ret_val);
    auto f_cond_val = dynamic_cast<FCmpInst *>(ret_val);
    Value *cond;
    if (cond_val == nullptr && f_cond_val == nullptr) {
        if (tmp_val->get_type()->is_integer_type()) {
            cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
        } else if (tmp_val->get_type()->is_float_type()) {
            f_cond_val = builder->create_fcmp_ne(tmp_val, CONST_FLOAT(0));
        }
    }
    if (cond_val != nullptr) {
        cond = cond_val;
    } else {
        cond = f_cond_val;
    }
    if (node.else_statement == nullptr) {
        builder->create_cond_br(cond, trueBB, nextBB);
    } else {
        builder->create_cond_br(cond, trueBB, falseBB);
    }
    cur_basic_block_list.pop_back();
    builder->set_insert_point(trueBB);
    cur_basic_block_list.push_back(trueBB);
    if (dynamic_cast<SyntaxTree::BlockStmt *>(node.if_statement.get())) {
        node.if_statement->accept(*this);
    } else {
        scope.enter();
        node.if_statement->accept(*this);
        scope.exit();
    }

    if (builder->get_insert_block()->get_terminator() == nullptr) {
        builder->create_br(nextBB);
    }
    cur_basic_block_list.pop_back();

    if (node.else_statement == nullptr) {
        falseBB->erase_from_parent();
    } else {
        builder->set_insert_point(falseBB);
        cur_basic_block_list.push_back(falseBB);
        if (dynamic_cast<SyntaxTree::BlockStmt *>(node.else_statement.get())) {
            node.else_statement->accept(*this);
        } else {
            scope.enter();
            node.else_statement->accept(*this);
            scope.exit();
        }
        if (builder->get_insert_block()->get_terminator() == nullptr) {
            builder->create_br(nextBB);
        }
        cur_basic_block_list.pop_back();
    }

    builder->set_insert_point(nextBB);
    cur_basic_block_list.push_back(nextBB);
    if (nextBB->get_pre_basic_blocks().size() == 0) {
        builder->set_insert_point(trueBB);
        nextBB->erase_from_parent();
    }
}

void IRBuilder::visit(SyntaxTree::WhileStmt &node) {
    auto whileBB = BasicBlock::create(module.get(), "", cur_fun);
    auto trueBB = BasicBlock::create(module.get(), "", cur_fun);
    auto nextBB = BasicBlock::create(module.get(), "", cur_fun);
    While_Stack.push_back({whileBB, nextBB});
    if (builder->get_insert_block()->get_terminator() == nullptr) {
        builder->create_br(whileBB);
    }
    cur_basic_block_list.pop_back();
    builder->set_insert_point(whileBB);
    IF_While_Or_Cond_Stack.push_back({trueBB, nextBB});
    node.cond_exp->accept(*this);
    IF_While_Or_Cond_Stack.pop_back();
    auto ret_val = tmp_val;
    auto cond_val = dynamic_cast<CmpInst *>(ret_val);
    auto f_cond_val = dynamic_cast<FCmpInst *>(ret_val);
    Value *cond;
    if (cond_val == nullptr && f_cond_val == nullptr) {
        if (tmp_val->get_type()->is_integer_type()) {
            cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
        } else if (tmp_val->get_type()->is_float_type()) {
            f_cond_val = builder->create_fcmp_ne(tmp_val, CONST_FLOAT(0));
        }
    }
    if (cond_val != nullptr) {
        cond = cond_val;
    } else {
        cond = f_cond_val;
    }
    builder->create_cond_br(cond, trueBB, nextBB);
    builder->set_insert_point(trueBB);
    cur_basic_block_list.push_back(trueBB);
    if (dynamic_cast<SyntaxTree::BlockStmt *>(node.statement.get())) {
        node.statement->accept(*this);
    } else {
        scope.enter();
        node.statement->accept(*this);
        scope.exit();
    }
    if (builder->get_insert_block()->get_terminator() == nullptr) {
        builder->create_br(whileBB);
    }
    cur_basic_block_list.pop_back();
    builder->set_insert_point(nextBB);
    cur_basic_block_list.push_back(nextBB);
    While_Stack.pop_back();
}

void IRBuilder::visit(SyntaxTree::BreakStmt &node) {
    builder->create_br(While_Stack.back().falseBB);
}

void IRBuilder::visit(SyntaxTree::ContinueStmt &node) {
    builder->create_br(While_Stack.back().trueBB);
}
