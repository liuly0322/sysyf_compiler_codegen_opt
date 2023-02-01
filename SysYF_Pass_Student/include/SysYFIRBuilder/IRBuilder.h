#ifndef _SYSYF_IR_BUILDER_H_
#define _SYSYF_IR_BUILDER_H_
#include "IRStmtBuilder.h"
#include "SyntaxTree.h"

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

class Scope {
  public:
    // enter a new scope
    void enter() {
        name2var.emplace_back();
        name2func.emplace_back();
    }

    // exit a scope
    void exit() {
        name2var.pop_back();
        name2func.pop_back();
    }

    bool in_global() { return name2var.size() == 1; }

    // push a name to scope
    // return true if successful
    // return false if this name already exits
    // but func name could be same with variable name
    bool push(const std::string &name, Value *val) {
        bool result;
        if (dynamic_cast<Function *>(val) != nullptr) {
            result =
                (name2func[name2func.size() - 1].insert({name, val})).second;
        } else {
            result = (name2var[name2var.size() - 1].insert({name, val})).second;
        }
        return result;
    }

    Value *find(const std::string &name, bool isfunc) {
        if (isfunc) {
            for (auto s = name2func.rbegin(); s != name2func.rend(); s++) {
                auto iter = s->find(name);
                if (iter != s->end()) {
                    return iter->second;
                }
            }
        } else {
            for (auto s = name2var.rbegin(); s != name2var.rend(); s++) {
                auto iter = s->find(name);
                if (iter != s->end()) {
                    return iter->second;
                }
            }
        }
        return nullptr;
    }

  private:
    std::vector<std::map<std::string, Value *>> name2var;
    std::vector<std::map<std::string, Value *>> name2func;
};

struct BinOp {
    union {
        SyntaxTree::BinOp bin_op;
        SyntaxTree::BinaryCondOp bin_cond_op;
    };
    enum Type {
        isBinOp,
        isBinCondOp,
    };
    Type flag;
    explicit BinOp(const SyntaxTree::BinOp &bin_op)
        : bin_op(bin_op), flag(isBinOp){};
    explicit BinOp(const SyntaxTree::BinaryCondOp &bin_cond_op)
        : bin_cond_op(bin_cond_op), flag(isBinCondOp){};
    [[nodiscard]] bool isCondOp() const { return flag == isBinCondOp; }
};

class IRBuilder : public SyntaxTree::Visitor {
  private:
    ConstantInt *CONST(int num) { return CONST_INT(num); }
    ConstantFloat *CONST(float num) { return CONST_FLOAT(num); }
    Value *typeConvertConstant(Constant *expr, Type *expected);
    Value *typeConvert(Value *prev_expr, Type *);
    template <typename T> Value *binOpGenConstantT(T lhs, T rhs, BinOp op);
    Value *binOpGenConstant(Constant *, Constant *, BinOp);
    Value *binOpGenCreateInst(Value *, Value *, BinOp);
    Value *binOpGenCreateCondInst(Value *, Value *, BinOp);
    void binOpGen(Value *, Value *, BinOp);
    void visit(SyntaxTree::InitVal &) final;
    void visit(SyntaxTree::Assembly &) final;
    void visit(SyntaxTree::FuncDef &) final;
    void visit(SyntaxTree::VarDef &) final;
    void visit(SyntaxTree::AssignStmt &) final;
    void visit(SyntaxTree::ReturnStmt &) final;
    void visit(SyntaxTree::BlockStmt &) final;
    void visit(SyntaxTree::EmptyStmt &) final;
    void visit(SyntaxTree::ExprStmt &) final;
    void visit(SyntaxTree::UnaryCondExpr &) final;
    void visit(SyntaxTree::BinaryCondExpr &) final;
    void visit(SyntaxTree::BinaryExpr &) final;
    void visit(SyntaxTree::UnaryExpr &) final;
    void visit(SyntaxTree::LVal &) final;
    void visit(SyntaxTree::Literal &) final;
    void visit(SyntaxTree::FuncCallStmt &) final;
    void visit(SyntaxTree::FuncParam &) final;
    void visit(SyntaxTree::FuncFParamList &) final;
    void visit(SyntaxTree::IfStmt &) final;
    void visit(SyntaxTree::WhileStmt &) final;
    void visit(SyntaxTree::BreakStmt &) final;
    void visit(SyntaxTree::ContinueStmt &) final;

    IRStmtBuilder *builder;
    Scope scope;
    std::unique_ptr<Module> module;

  public:
    ~IRBuilder() { delete builder; }
    IRBuilder() {
        module = std::make_unique<Module>("SysYF code");
        builder = new IRStmtBuilder(nullptr, module.get());
        auto *TyVoid = Type::get_void_type(module.get());
        auto *TyInt32 = Type::get_int32_type(module.get());
        auto *TyInt32Ptr = Type::get_int32_ptr_type(module.get());
        auto *TyFloat = Type::get_float_type(module.get());
        auto *TyFloatPtr = Type::get_float_ptr_type(module.get());

        auto *input_type = FunctionType::get(TyInt32, {});
        auto *get_int = Function::create(input_type, "get_int", module.get());

        input_type = FunctionType::get(TyFloat, {});
        auto *get_float =
            Function::create(input_type, "get_float", module.get());

        input_type = FunctionType::get(TyInt32, {});
        auto *get_char = Function::create(input_type, "get_char", module.get());

        std::vector<Type *> input_params;
        std::vector<Type *>().swap(input_params);
        input_params.push_back(TyInt32Ptr);
        input_type = FunctionType::get(TyInt32, input_params);
        auto *get_int_array =
            Function::create(input_type, "get_int_array", module.get());

        std::vector<Type *>().swap(input_params);
        input_params.push_back(TyFloatPtr);
        input_type = FunctionType::get(TyInt32, input_params);
        auto *get_float_array =
            Function::create(input_type, "get_float_array", module.get());

        std::vector<Type *> output_params;
        std::vector<Type *>().swap(output_params);
        output_params.push_back(TyInt32);
        auto *output_type = FunctionType::get(TyVoid, output_params);
        auto *put_int = Function::create(output_type, "put_int", module.get());

        std::vector<Type *>().swap(output_params);
        output_params.push_back(TyFloat);
        output_type = FunctionType::get(TyVoid, output_params);
        auto *put_float =
            Function::create(output_type, "put_float", module.get());

        std::vector<Type *>().swap(output_params);
        output_params.push_back(TyInt32);
        output_type = FunctionType::get(TyVoid, output_params);
        auto *put_char =
            Function::create(output_type, "put_char", module.get());

        std::vector<Type *>().swap(output_params);
        output_params.push_back(TyInt32);
        output_params.push_back(TyInt32Ptr);
        output_type = FunctionType::get(TyVoid, output_params);
        auto *put_int_array =
            Function::create(output_type, "put_int_array", module.get());

        std::vector<Type *>().swap(output_params);
        output_params.push_back(TyInt32);
        output_params.push_back(TyFloatPtr);
        output_type = FunctionType::get(TyVoid, output_params);
        auto *put_float_array =
            Function::create(output_type, "put_float_array", module.get());

        scope.enter();
        scope.push("getint", get_int);
        scope.push("getfloat", get_float);
        scope.push("getch", get_char);
        scope.push("getarray", get_int_array);
        scope.push("get_float_array", get_float_array);
        scope.push("putint", put_int);
        scope.push("putfloat", put_float);
        scope.push("putch", put_char);
        scope.push("putarray", put_int_array);
        scope.push("putfloatarray", put_float_array);
    }
    std::unique_ptr<Module> getModule() { return std::move(module); }
};

#endif // _SYSYF_IR_BUILDER_H_
