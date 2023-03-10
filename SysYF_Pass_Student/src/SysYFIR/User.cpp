#include "User.h"
#include <vector>
#ifdef DEBUG
#include <cassert>
#endif

User::User(Type *ty, std::string name, unsigned num_ops)
    : Value(ty, std::move(name)), num_ops_(num_ops) {
    // if (num_ops_ > 0)
    //   operands_.reset(new std::list<Value *>());
    operands_.resize(num_ops_, nullptr);
}

std::vector<Value *> &User::get_operands() { return operands_; }

Value *User::get_operand(unsigned i) const { return operands_[i]; }

void User::set_operand(unsigned i, Value *v) {
#ifdef DEBUG
    assert(i < num_ops_ && "set_operand out of index");
#endif
    // assert(operands_[i] == nullptr && "ith operand is not null");
    operands_[i] = v;
    v->add_use(this, i);
}

void User::add_operand(Value *v) {
    operands_.push_back(v);
    v->add_use(this, num_ops_);
    num_ops_++;
}

unsigned User::get_num_operand() const { return num_ops_; }

void User::remove_use_of_ops() {
    for (auto *op : operands_) {
        op->remove_use(this);
    }
}

void User::remove_operands(unsigned index1, unsigned index2) {
    // index2 之后的也要删，但是要加回去
    auto backup = std::vector<Value *>{};
    for (auto i = index2 + 1; i < num_ops_; i++) {
        backup.push_back(operands_[i]);
    }
    for (auto i = index1; i < num_ops_; i++) {
        operands_[i]->remove_use(this);
    }
    operands_.erase(operands_.begin() + index1, operands_.begin() + num_ops_);
    // std::cout<<operands_.size()<<std::endl;
    num_ops_ = operands_.size();
    for (auto *op : backup) {
        add_operand(op);
    }
}
