#include "Check.h"
#include "Module.h"
#include <algorithm>

void writeInfo(Value *val) {
    if (dynamic_cast<Instruction *>(val)) {
        std::cout << "\033[32;1mat Instruction " << val->print() << "\033[0m ";
    } else if (dynamic_cast<BasicBlock *>(val)) {
        std::cout << "\033[33;1mat BasicBlock " << val->get_name() << "\033[0m ";
    } else if (dynamic_cast<Function *>(val)) {
        std::cout << "\033[34;1mat Function " << val->get_name() << "\033[0m ";
    } else if (dynamic_cast<GlobalVariable *>(val)) {
        std::cout << "\033[34;1mof GlobalVariable " << val->get_name() << "\033[0m ";
    }
}

template <typename... Ts>
void writeTs() {}

template <typename T1, typename... Ts>
void writeTs(T1 &V1, Ts &...Vs) {
    writeInfo(V1);
    writeTs(Vs...);
}

/// A check failed (with values to print).
///
/// This calls the Message-only version so that the above is easier to set a
/// breakpoint on.
template <typename T1, typename... Ts>
void Check::checkFailed(const std::string &message, T1 &V1, Ts &...Vs) {
    std::cout << "\033[31;1mERROR: " << message << "\033[0m ";
    writeTs(V1, Vs...);
    std::cout << std::endl;
    Broken = false;
}

/// We know that cond should be true, if not print an error message.
#define Verify(C, ...)                                                                                                 \
    do {                                                                                                               \
        if (!(C)) {                                                                                                    \
            checkFailed(__VA_ARGS__);                                                                                  \
            return;                                                                                                    \
        }                                                                                                              \
    } while (false)

void Check::execute() {
    // TODO write your IR Module checker here.
    // Collect functions
    // std::cout << "/*************Check************/" << std::endl;
    functions.clear();
    for (auto *fun : module->get_functions()) {
        functions.insert(fun);
    }

    // Collect define value
    define_global.clear();
    // Collect global value and check use of global value by the way.
    for (auto *global_val : module->get_global_variable()) {
        define_global.insert(global_val);
        checkUse(global_val);
    }

    // Check each function
    for (auto *fun : module->get_functions()) {
        if (fun->get_basic_blocks().empty()) {
            continue;
        }
        valueDefineCounting(fun);
        checkFunction(fun);
    }
}

void Check::checkFunction(Function *fun) {
    // Check function context.
    Verify(fun->get_parent() == module, "Function context does not match Module context!", fun);

    // Check function return type.
    Type *retTy = fun->get_return_type();
    Verify(retTy->is_integer_type() || retTy->is_float_type() || retTy->is_void_type(),
           "Functions cannot return aggregate values!", fun);

    // Check the entry node.
    BasicBlock *entry = fun->get_entry_block();
    Verify(entry->get_pre_basic_blocks().empty(), "Entry block to function must not have predecessors!", fun);

    // Check each BasicBlock
    for (auto *bb : fun->get_basic_blocks()) {
        if (bb->empty()) {
            continue;
        }
        checkBasicBlock(bb);
    }
}

void Check::checkBasicBlock(BasicBlock *bb) {
    // Check instruction embedded in a basic block
    Function *func = bb->get_parent();
    Verify(func, "Basic not embedded in Function!", bb);

    // Ensure that basic blocks have terminators!
    Verify(bb->get_terminator(), "Basic Block does not have terminator!", bb);

    // Check constraints that this basic block imposes on all of the PHI nodes in it.
    // Collect PHI instructions.
    std::set<Instruction *> phi_list;
    for (auto *inst : bb->get_instructions()) {
        if (inst->get_instr_type() != Instruction::OpID::phi) {
            break;
        }
        phi_list.insert(inst);
    }

    // Check phi instruction.
    for (auto *phi : phi_list) {
        checkPhiInstruction(phi);
    }

    // Check other instructions.
    for (auto *inst : bb->get_instructions()) {
        // Phi instruction should be at the front of the basicblock.
        if (inst->get_instr_type() == Instruction::OpID::phi) {
            Verify(phi_list.count(inst), "PHINode is not grouped at top of basic block!", inst, bb);
        }
        checkInstruction(inst);
    }

    // Check pred and succ relations.
    for (auto *succ_bb : bb->get_succ_basic_blocks()) {
        auto &preds = succ_bb->get_pre_basic_blocks();
        auto it = std::find(preds.begin(), preds.end(), bb);
        Verify(it != preds.end(), "contradictory precursor and successor!", bb, succ_bb);
    }
}

void Check::checkPhiInstruction(Instruction *inst) {
    // First check phi instruction as a common instruction.
    checkInstruction(inst);

    // auto &preds = inst->get_parent()->get_pre_basic_blocks();

    // Check nums of entry.
    // Verify(inst->get_operands().size() == preds.size() * 2,
    //    "PHINode should have one entry for each predecessor of its parent basic block!", inst);

    // Get all incoming BasicBlocks and values in the PHI node.
    std::set<Value *> basicBlocks;
    std::set<Value *> values;
    for (auto *opr : inst->get_operands()) {
        if (dynamic_cast<BasicBlock *>(opr)) {
            basicBlocks.insert(opr);
        } else {
            values.insert(opr); // include constant
        }
    }

    // Check to make sure that if there is more than one entry for a
    // particular basic block in this PHI node,
    Verify(basicBlocks.size() * 2 == inst->get_operands().size(),
           "PHI node has multiple entries for the same basic block with different incoming values!", inst);

    // Check that all of the values of the PHI node have the same type as the
    // result, and that the incoming blocks are really basic blocks.
    for (auto *val : values) {
        Verify(val->get_type()->get_type_id() == inst->get_type()->get_type_id(),
               "PHI node operands are not the same type as the result!", inst);
    }
};

void Check::checkInstruction(Instruction *inst) {
    // Check instruction embedded in a basic block
    BasicBlock *bb = inst->get_parent();
    Verify(bb, "Instruction not embedded in basic block!", inst);

    // Check use of the instruction.
    checkUse(inst);

    // Check if all operands are defined
    for (auto *opr : inst->get_operands()) {
        // Constant
        if (dynamic_cast<Constant *>(opr)) {
            continue;
        }
        // Actually a call instruction
        auto *func = dynamic_cast<Function *>(opr);
        if (func) {
            Verify(functions.count(func), "Operand(Function) is not defined!", inst);
            checkCallInstruction(inst);
        }
        BasicBlock *block = dynamic_cast<BasicBlock *>(opr);
        if (block) {
            Verify(blocks.count(block), "Operand(BasicBlock) is not defined!", inst, block);
        }
        Instruction *instr = dynamic_cast<Instruction *>(opr);
        if (instr) {
            Verify(define_inst.count(instr), "Operand(Value) is not defined!", inst, instr);
        }
    }
    // Ret instruction check
    if (inst->get_instr_type() == Instruction::OpID::ret) {
        auto *ret_inst = dynamic_cast<ReturnInst *>(inst);
        auto *func = bb->get_parent();
        if (ret_inst->is_void_ret()) {
            Verify(func->get_return_type()->is_void_type(), "Wrong function return type!", inst, func);
        } else {
            Verify(func->get_return_type()->get_type_id() == inst->get_operand(0)->get_type()->get_type_id(),
                   "Wrong function return type!", inst, func);
        }
    }
}

void Check::checkCallInstruction(Instruction *inst) {
    // Check the consistency of return types.
    auto *func = dynamic_cast<Function *>(inst->get_operand(0));
    auto *funcTy = func->get_function_type();
    Verify(funcTy->get_return_type()->get_type_id() == inst->get_type()->get_type_id(),
           "Wrong call function return type!", inst, func);

    // Check nums of parameter.
    Verify(inst->get_num_operand() - 1 == funcTy->get_num_of_args(), "The number of parameters is inconsistent!", inst);

    // Check the consistency of parameter types.
    for (int i = 1; i < inst->get_num_operand(); i++) {
        Verify(inst->get_operand(i)->get_type()->get_type_id() == funcTy->get_param_type(i - 1)->get_type_id(),
               "Parameter type conflict!", inst);
    }
}

void Check::checkUse(User *user) {
    // Check that all uses of the instruction/global value, if they are instructions
    // themselves, actually have parent basic blocks.  If the use is not an
    // instruction, it is an error!
    for (auto &use : user->get_use_list()) {
        Instruction *use_inst = dynamic_cast<Instruction *>(use.val_);
        if (use_inst != nullptr) {
            Verify(use_inst->get_parent() != nullptr,
                   "Instruction referencing instruction/global value not embedded in a basic block!", user);
        } else {
            checkFailed("Use of instruction/global value is not an instruction!", user);
            return;
        }
    }
}

void Check::valueDefineCounting(Function *fun) {
    // Collect BasicBLock.
    blocks.clear();
    for (auto *bb : fun->get_basic_blocks()) {
        blocks.insert(bb);
    }
    // Collect Instruction.
    for (auto *bb : fun->get_basic_blocks()) {
        for (auto *inst : bb->get_instructions()) {
            if (inst->is_void()) {
                continue;
            }
            define_inst.insert(inst);
        }
    }
}