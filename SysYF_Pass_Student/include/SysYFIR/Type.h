#ifndef _SYSYF_TYPE_H_
#define _SYSYF_TYPE_H_

#include <iostream>
#include <vector>

class Module;
class IntegerType;
class FloatType;
class FunctionType;
class ArrayType;
class PointerType;

class Type {
  public:
    enum TypeID {
        VoidTyID,     // Void
        LabelTyID,    // Labels, e.g., BasicBlock
        IntegerTyID,  // Integers, include 32 bits and 1 bit
        FloatTyID,    // Floats, only 32 bits
        FunctionTyID, // Functions
        ArrayTyID,    // Arrays
        PointerTyID,  // Pointer
    };

    explicit Type(TypeID tid, Module *m);
    ~Type() = default;

    [[nodiscard]] TypeID get_type_id() const { return tid_; }

    [[nodiscard]] bool is_void_type() const {
        return get_type_id() == VoidTyID;
    }

    [[nodiscard]] bool is_label_type() const {
        return get_type_id() == LabelTyID;
    }

    [[nodiscard]] bool is_integer_type() const {
        return get_type_id() == IntegerTyID;
    }

    [[nodiscard]] bool is_float_type() const {
        return get_type_id() == FloatTyID;
    }

    [[nodiscard]] bool is_function_type() const {
        return get_type_id() == FunctionTyID;
    }

    [[nodiscard]] bool is_array_type() const {
        return get_type_id() == ArrayTyID;
    }

    [[nodiscard]] bool is_pointer_type() const {
        return get_type_id() == PointerTyID;
    }

    static bool is_eq_type(Type *ty1, Type *ty2);

    static Type *get_void_type(Module *m);

    static Type *get_label_type(Module *m);

    static IntegerType *get_int1_type(Module *m);

    static IntegerType *get_int32_type(Module *m);

    static FloatType *get_float_type(Module *m);

    static PointerType *get_int32_ptr_type(Module *m);

    static PointerType *get_float_ptr_type(Module *m);

    static PointerType *get_pointer_type(Type *contained);

    static ArrayType *get_array_type(Type *contained, unsigned num_elements);

    Type *get_pointer_element_type();

    Type *get_array_element_type();

    int get_size();

    Module *get_module();

    std::string print();

  private:
    TypeID tid_;
    Module *m_;
};

class IntegerType : public Type {
  public:
    explicit IntegerType(unsigned num_bits, Module *m);

    static IntegerType *get(unsigned num_bits, Module *m);

    [[nodiscard]] unsigned get_num_bits() const { return num_bits_; };

  private:
    unsigned num_bits_;
};

class FloatType : public Type {
  public:
    explicit FloatType(Module *m);
    static FloatType *get(Module *m);
};

class FunctionType : public Type {
  public:
    FunctionType(Type *result, std::vector<Type *> params);

    static bool is_valid_return_type(Type *ty);
    static bool is_valid_argument_type(Type *ty);

    static FunctionType *get(Type *result, std::vector<Type *> params);

    [[nodiscard]] unsigned get_num_of_args() const;

    [[nodiscard]] Type *get_param_type(unsigned i) const;
    std::vector<Type *>::iterator param_begin() { return args_.begin(); }
    std::vector<Type *>::iterator param_end() { return args_.end(); }
    [[nodiscard]] Type *get_return_type() const;

  private:
    Type *result_;
    std::vector<Type *> args_;
};

class ArrayType : public Type {
  public:
    ArrayType(Type *contained, unsigned num_elements);

    static bool is_valid_element_type(Type *ty);

    static ArrayType *get(Type *contained, unsigned num_elements);

    [[nodiscard]] Type *get_element_type() const { return contained_; }
    [[nodiscard]] unsigned get_num_of_elements() const { return num_elements_; }

  private:
    Type *contained_;       // The element type of the array.
    unsigned num_elements_; // Number of elements in the array.
};

class PointerType : public Type {
  public:
    explicit PointerType(Type *contained);
    [[nodiscard]] Type *get_element_type() const { return contained_; }

    static PointerType *get(Type *contained);

  private:
    Type *contained_; // The element type of the ptr.
};

#endif // _SYSYF_TYPE_H_