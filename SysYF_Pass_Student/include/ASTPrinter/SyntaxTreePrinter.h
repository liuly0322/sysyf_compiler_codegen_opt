#ifndef _SYSYF_SYNTAX_TREE_PRINTER_H_
#define _SYSYF_SYNTAX_TREE_PRINTER_H_

#include "SyntaxTree.h"

class SyntaxTreePrinter : public SyntaxTree::Visitor {
  public:
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
    void print_indent();

  private:
    int indent = 0;
};

#endif // _SYSYF_SYNTAX_TREE_PRINTER_H_
