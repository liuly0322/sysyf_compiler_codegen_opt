#ifndef _SYSYF_SYNTAX_TREE_CHECKER_H_
#define _SYSYF_SYNTAX_TREE_CHECKER_H_

#include <cassert>
#include "ErrorReporter.h"
#include "SyntaxTree.h"
#include <unordered_map>
#include <stack>
#include <deque>

class SyntaxTreeChecker : public SyntaxTree::Visitor {
   public:
    SyntaxTreeChecker(ErrorReporter& e) : err(e) {}
    virtual void visit(SyntaxTree::Assembly& node) override;
    virtual void visit(SyntaxTree::FuncDef& node) override;
    virtual void visit(SyntaxTree::BinaryExpr& node) override;
    virtual void visit(SyntaxTree::UnaryExpr& node) override;
    virtual void visit(SyntaxTree::LVal& node) override;
    virtual void visit(SyntaxTree::Literal& node) override;
    virtual void visit(SyntaxTree::ReturnStmt& node) override;
    virtual void visit(SyntaxTree::VarDef& node) override;
    virtual void visit(SyntaxTree::AssignStmt& node) override;
    virtual void visit(SyntaxTree::FuncCallStmt& node) override;
    virtual void visit(SyntaxTree::BlockStmt& node) override;
    virtual void visit(SyntaxTree::EmptyStmt& node) override;
    virtual void visit(SyntaxTree::ExprStmt& node) override;
    virtual void visit(SyntaxTree::FuncParam& node) override;
    virtual void visit(SyntaxTree::FuncFParamList& node) override;
    virtual void visit(SyntaxTree::BinaryCondExpr& node) override;
    virtual void visit(SyntaxTree::UnaryCondExpr& node) override;
    virtual void visit(SyntaxTree::IfStmt& node) override;
    virtual void visit(SyntaxTree::WhileStmt& node) override;
    virtual void visit(SyntaxTree::BreakStmt& node) override;
    virtual void visit(SyntaxTree::ContinueStmt& node) override;
    virtual void visit(SyntaxTree::InitVal& node) override;

   private:
    ErrorReporter& err;

    bool Expr_int;

    std::vector<std::unordered_map<std::string, bool>> variables;
    void enter_scope() {
        variables.emplace_back();
    }
    void exit_scope() {
        variables.pop_back();
    }
    bool declare_variable(std::string name, SyntaxTree::Type type) {
        if (variables.back().count(name)) {
            return false;
        }
        variables.back().insert({name, type == SyntaxTree::Type::INT});
        return true;
    }
    bool lookup_variable(std::string name, bool &is_int) {
        for (auto it = variables.rbegin(); it != variables.rend(); it++) {
            if (it->count(name)) {
                is_int = (*it)[name];
                return true;
            }
        }
        return false;
    }

    using PtrFuncFParamList = SyntaxTree::Ptr<SyntaxTree::FuncFParamList>;
    struct Function{
        bool ret_int;
        std::deque<bool> args_int;
        Function(SyntaxTree::Type ret_type, PtrFuncFParamList param_list) {
            ret_int = (ret_type == SyntaxTree::Type::INT);
            for (auto param : param_list->params) {
                args_int.emplace_back(param->param_type == SyntaxTree::Type::INT);
            }
        }
    };
    using PtrFunction = std::shared_ptr<Function>;
    std::unordered_map<std::string, PtrFunction> functions;
    bool declare_functions(std::string name, SyntaxTree::Type ret_type, PtrFuncFParamList param_list) {
        if (functions.count(name)) {
            return false;
        }
        functions.insert({
            name,
            PtrFunction(
                new Function(
                    ret_type,
                    param_list
                )
            )
        });
        return true;
    }
    PtrFunction lookup_functions(std::string name) {
        if (functions.count(name)) {
            return functions[name];
        }
        return nullptr;
    }
};

enum class ErrorType {
    Accepted = 0,
    Modulo,
    VarUnknown,
    VarDuplicated,
    FuncUnknown,
    FuncDuplicated,
    FuncParams
};

#endif  // _SYSYF_SYNTAX_TREE_CHECKER_H_
