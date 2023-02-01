#ifndef _SYSYF_GLOBALVARIABLE_H_
#define _SYSYF_GLOBALVARIABLE_H_

#include "Constant.h"

class Module;

class GlobalVariable : public User {
  private:
    bool is_const_;
    Constant *init_val_;
    GlobalVariable(std::string name, Module *m, Type *ty, bool is_const,
                   Constant *init = nullptr);

  public:
    static GlobalVariable *create(std::string name, Module *m, Type *ty,
                                  bool is_const, Constant *init);

    Constant *get_init() { return init_val_; }
    [[nodiscard]] bool is_const() const { return is_const_; }
    std::string print() override;
};
#endif //_SYSYF_GLOBALVARIABLE_H_
