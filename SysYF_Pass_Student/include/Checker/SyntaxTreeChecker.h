#ifndef _SYSYF_SYNTAX_TREE_CHECKER_H_
#define _SYSYF_SYNTAX_TREE_CHECKER_H_

#include "ErrorReporter.h"
#include "SyntaxTree.h"
#include <cassert>
#include <deque>
#include <stack>
#include <unordered_map>

class SyntaxTreeChecker : public SyntaxTree::Visitor {
  public:
    explicit SyntaxTreeChecker(ErrorReporter &e) : err(e) {}
    void visit(SyntaxTree::Assembly &node) override;
    void visit(SyntaxTree::FuncDef &node) override;
    void visit(SyntaxTree::BinaryExpr &node) override;
    void visit(SyntaxTree::UnaryExpr &node) override;
    void visit(SyntaxTree::LVal &node) override;
    void visit(SyntaxTree::Literal &node) override;
    void visit(SyntaxTree::ReturnStmt &node) override;
    void visit(SyntaxTree::VarDef &node) override;
    void visit(SyntaxTree::AssignStmt &node) override;
    void visit(SyntaxTree::FuncCallStmt &node) override;
    void visit(SyntaxTree::BlockStmt &node) override;
    void visit(SyntaxTree::EmptyStmt &node) override;
    void visit(SyntaxTree::ExprStmt &node) override;
    void visit(SyntaxTree::FuncParam &node) override;
    void visit(SyntaxTree::FuncFParamList &node) override;
    void visit(SyntaxTree::BinaryCondExpr &node) override;
    void visit(SyntaxTree::UnaryCondExpr &node) override;
    void visit(SyntaxTree::IfStmt &node) override;
    void visit(SyntaxTree::WhileStmt &node) override;
    void visit(SyntaxTree::BreakStmt &node) override;
    void visit(SyntaxTree::ContinueStmt &node) override;
    void visit(SyntaxTree::InitVal &node) override;

  private:
    ErrorReporter &err;

    bool Expr_int;

    std::vector<std::unordered_map<std::string, bool>> variables;
    void enter_scope() { variables.emplace_back(); }
    void exit_scope() { variables.pop_back(); }
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
    struct Function {
        bool ret_int;
        std::deque<bool> args_int;
        Function(SyntaxTree::Type ret_type, PtrFuncFParamList param_list) {
            ret_int = (ret_type == SyntaxTree::Type::INT);
            for (auto param : param_list->params) {
                args_int.emplace_back(param->param_type ==
                                      SyntaxTree::Type::INT);
            }
        }
    };
    using PtrFunction = std::shared_ptr<Function>;
    std::unordered_map<std::string, PtrFunction> functions;
    bool declare_functions(std::string name, SyntaxTree::Type ret_type,
                           PtrFuncFParamList param_list) {
        if (functions.count(name)) {
            return false;
        }
        functions.insert(
            {name, PtrFunction(new Function(ret_type, param_list))});
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

#endif // _SYSYF_SYNTAX_TREE_CHECKER_H_
