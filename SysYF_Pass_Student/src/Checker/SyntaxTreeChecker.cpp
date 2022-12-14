#include "SyntaxTreeChecker.h"

using namespace SyntaxTree;

void SyntaxTreeChecker::visit(Assembly& node) {
    enter_scope();
    for (auto def : node.global_defs) {
        def->accept(*this);
    }
    exit_scope();
}

void SyntaxTreeChecker::visit(FuncDef& node) {
    if (!declare_functions(node.name, node.ret_type, node.param_list)) {
        err.error(node.loc, "Duplicated functions declare!");
        exit(int(ErrorType::FuncDuplicated));
    }
    enter_scope();
    node.param_list->accept(*this);
    node.body->accept(*this);
    exit_scope();
}

void SyntaxTreeChecker::visit(BinaryExpr& node) {
    node.lhs->accept(*this);
    bool lhs_int = this->Expr_int;
    node.rhs->accept(*this);
    bool rhs_int = this->Expr_int;
    if (node.op == SyntaxTree::BinOp::MODULO) {
        if (!lhs_int || !rhs_int) {
            err.error(node.loc, "Operands of module should be integers.");
            exit(int(ErrorType::Modulo));
        }
    }
    this->Expr_int = lhs_int & rhs_int;
}

void SyntaxTreeChecker::visit(UnaryExpr& node) {
    node.rhs->accept(*this);
}

void SyntaxTreeChecker::visit(LVal& node) {
    if (!lookup_variable(node.name, this->Expr_int)) {
        err.error(node.loc, "Unknown variable!");
        exit(int(ErrorType::VarUnknown));
    }
}

void SyntaxTreeChecker::visit(Literal& node) {
    this->Expr_int = (node.literal_type == SyntaxTree::Type::INT);
}

void SyntaxTreeChecker::visit(ReturnStmt& node) {
    node.ret->accept(*this);
}

void SyntaxTreeChecker::visit(VarDef& node) {
    if (node.is_inited) {
        node.initializers->accept(*this);
    }
    if (!declare_variable(node.name, node.btype)) {
        err.error(node.loc, "Duplicated variables declare!");
        exit(int(ErrorType::VarDuplicated));
    }
}

void SyntaxTreeChecker::visit(AssignStmt& node) {
    node.target->accept(*this);
    node.value->accept(*this);
}
void SyntaxTreeChecker::visit(FuncCallStmt& node) {
    for (auto param : node.params) {
        param->accept(*this);
    }
    auto function = lookup_functions(node.name);
    if (!function) {
        err.error(node.loc, "Unknown function!");
        exit(int(ErrorType::FuncUnknown));
    }
    if (function->args_int.size() != node.params.size()) {
        err.error(node.loc, "Function call arguments dismatch!");
        exit(int(ErrorType::FuncParams));
    }
    auto arg_int = function->args_int.begin();
    for (auto param : node.params) {
        param->accept(*this);
        if (this->Expr_int != *arg_int) {
            err.error(node.loc, "Function call arguments dismatch!");
            exit(int(ErrorType::FuncParams));
        }
        arg_int++;
    }
    this->Expr_int = function->ret_int;
}
void SyntaxTreeChecker::visit(BlockStmt& node) {
    for (auto stmt : node.body) {
        if (dynamic_cast<BlockStmt*>(stmt.get())) {
            enter_scope();
        }
        stmt->accept(*this);
        if (dynamic_cast<BlockStmt*>(stmt.get())) {
            exit_scope();
        }
    }
}
void SyntaxTreeChecker::visit(EmptyStmt& node) {}
void SyntaxTreeChecker::visit(SyntaxTree::ExprStmt& node) {
    node.exp->accept(*this);
}

void SyntaxTreeChecker::visit(SyntaxTree::FuncParam& node) {
    if (!declare_variable(node.name, node.param_type)) {
        err.error(node.loc, "Duplicated variables declare!");
        exit(int(ErrorType::VarDuplicated));
    }
}

void SyntaxTreeChecker::visit(SyntaxTree::FuncFParamList& node) {
    for (auto param : node.params) {
        param->accept(*this);
    }
}

void SyntaxTreeChecker::visit(SyntaxTree::BinaryCondExpr& node) {
    node.lhs->accept(*this);
    node.rhs->accept(*this);
}
void SyntaxTreeChecker::visit(SyntaxTree::UnaryCondExpr& node) {
    node.rhs->accept(*this);
}
void SyntaxTreeChecker::visit(SyntaxTree::IfStmt& node) {
    node.cond_exp->accept(*this);
    enter_scope();
    node.if_statement->accept(*this);
    exit_scope();
    if (node.else_statement) {
        enter_scope();
        node.else_statement->accept(*this);
        exit_scope();
    }
}
void SyntaxTreeChecker::visit(SyntaxTree::WhileStmt& node) {
    node.cond_exp->accept(*this);
    enter_scope();
    node.statement->accept(*this);
    exit_scope();
}
void SyntaxTreeChecker::visit(SyntaxTree::BreakStmt& node) {}
void SyntaxTreeChecker::visit(SyntaxTree::ContinueStmt& node) {}

void SyntaxTreeChecker::visit(SyntaxTree::InitVal& node) {
    if (node.isExp) {
        node.expr->accept(*this);
    } 
    else {
        for (auto element : node.elementList) {
            element->accept(*this);
        }
    }
}
