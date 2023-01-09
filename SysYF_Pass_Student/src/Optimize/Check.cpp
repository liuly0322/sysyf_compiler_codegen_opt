#include "Check.h"
#include "Module.h"
#include <algorithm>
#include <unordered_set>

void writeInfo(std::string message) { std::cout << "\033[36;1m" << message << "\033[0m "; }

void writeInfo(Value *val) {
    if (dynamic_cast<Instruction *>(val) != nullptr) {
        std::cout << "\033[32;1m"
                  << "at Instruction " << val->print() << "\033[0m ";
    } else if (dynamic_cast<BasicBlock *>(val) != nullptr) {
        std::cout << "\033[33;1m"
                  << "at BasicBlock " << val->get_name() << "\033[0m ";
    } else if (dynamic_cast<Function *>(val) != nullptr) {
        std::cout << "\033[34;1m"
                  << "at Function " << val->get_name() << "\033[0m ";
    } else if (dynamic_cast<GlobalVariable *>(val) != nullptr) {
        std::cout << "\033[35;1m"
                  << "of GlobalVariable " << val->get_name() << "\033[0m ";
    }
}

template <typename... Ts>
void writeTs() {}

template <typename T1, typename... Ts>
void writeTs(T1 V1, Ts... Vs) {
    writeInfo(V1);
    writeTs(Vs...);
}

/// A check failed (with values to print).
///
/// This calls the Message-only version so that the above is easier to set a
/// breakpoint on.
template <typename T1, typename... Ts>
void Check::checkFailed(const std::string &message, T1 V1, Ts... Vs) {
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
    Function *fun = bb->get_parent();
    Verify(fun != nullptr, "Basic not embedded in Function!", bb);

    // Ensure that basic blocks have terminators!
    Verify(bb->get_terminator() != nullptr, "Basic Block does not have terminator!", bb, fun);

    // Check constraints that this basic block imposes on all of the PHI nodes in it.
    // Collect PHI instructions.
    std::set<Instruction *> phi_list;
    for (auto *inst : bb->get_instructions()) {
        if (!inst->is_phi()) {
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
        if (inst->is_phi()) {
            Verify(phi_list.count(inst), "PHINode is not grouped at top of basic block!", inst, bb, fun,
                   "\nHere is block content\n", bb->print());
        }
        checkInstruction(inst);
    }

    // Check pred and succ relations.
    for (auto *succ_bb : bb->get_succ_basic_blocks()) {
        auto &preds = succ_bb->get_pre_basic_blocks();
        auto it = std::find(preds.begin(), preds.end(), bb);
        Verify(it != preds.end(), "contradictory precursor and successor!", fun, "current block", bb->get_name(),
               "is not in prev_bb of successor block", succ_bb->get_name());
    }
}

void Check::checkPhiInstruction(Instruction *inst) {
    // First check phi instruction as a common instruction.
    checkInstruction(inst);

    BasicBlock *bb = inst->get_parent();
    Function *fun = inst->get_function();

    auto preds = std::unordered_set<BasicBlock*>{bb->get_pre_basic_blocks().begin(), bb->get_pre_basic_blocks().end()};

    // Check nums of entry.
    // Verify(inst->get_operands().size() == preds.size() * 2,
    //    "PHINode should have one entry for each predecessor of its parent basic block!", inst, bb, fun);

    // Get all incoming BasicBlocks and values in the PHI node.
    std::set<Value *> basicBlocks;
    std::set<Value *> values;
    for (auto *opr : inst->get_operands()) {
        if (dynamic_cast<BasicBlock *>(opr) != nullptr) {
            Verify(preds.count(static_cast<BasicBlock*>(opr)) != 0, "Useless phi operand!", inst, bb, fun);
            basicBlocks.insert(opr);
        } else {
            values.insert(opr); // include constant
        }
    }

    // Check to make sure that if there is more than one entry for a
    // particular basic block in this PHI node,
    Verify(basicBlocks.size() * 2 == inst->get_operands().size(),
           "PHI node has multiple entries for the same basic block with different incoming values!", inst, bb, fun);

    // Check that all of the values of the PHI node have the same type as the
    // result, and that the incoming blocks are really basic blocks.
    for (auto *val : values) {
        Verify(val->get_type()->get_type_id() == inst->get_type()->get_type_id(),
               "PHI node operands are not the same type as the result!", inst, bb, fun);
    }
};

void Check::checkInstruction(Instruction *inst) {
    // Check instruction embedded in a basic block
    BasicBlock *bb = inst->get_parent();
    Verify(bb != nullptr, "Instruction not embedded in basic block!", inst);
    Function *fun = inst->get_function();

    // Check use of the instruction.
    checkUse(inst);

    // Check if all operands are defined
    for (auto *opr : inst->get_operands()) {
        // Constant
        if (dynamic_cast<Constant *>(opr) != nullptr) {
            continue;
        }
        // Actually a call instruction
        auto *func = dynamic_cast<Function *>(opr);
        if (func != nullptr) {
            Verify(functions.count(func), "Operand(Function) is not defined!", inst, bb, fun, "function",
                   func->get_name(), "not defined!");
            checkCallInstruction(inst);
        }
        BasicBlock *block = dynamic_cast<BasicBlock *>(opr);
        if (block != nullptr) {
            Verify(blocks.count(block), "Operand(BasicBlock) is not defined!", inst, bb, fun, "block",
                   block->get_name(), "not defined!");
        }
        Instruction *instr = dynamic_cast<Instruction *>(opr);
        if (instr != nullptr) {
            Verify(define_inst.count(instr), "Operand(Value) is not defined!", inst, bb, fun, "operand",
                   instr->get_name(), "not defined!");
        }
    }

    // Br instruction check
    if (inst->is_br()) {
        auto &succ_blocks = bb->get_succ_basic_blocks();
        for (auto *opr : inst->get_operands()) {
            BasicBlock *block = dynamic_cast<BasicBlock *>(opr);
            if (block != nullptr) {
                auto it = std::find(succ_blocks.begin(), succ_blocks.end(), block);
                Verify(it != succ_blocks.end(), "contradictory precursor and successor!", inst, bb, fun, "block",
                       block->get_name(), "not in successor!");
            }
        }
    }

    // Ret instruction check
    if (inst->is_ret()) {
        auto *ret_inst = dynamic_cast<ReturnInst *>(inst);
        if (ret_inst->is_void_ret()) {
            Verify(fun->get_return_type()->is_void_type(), "Wrong function return type!", inst, bb, fun);
        } else {
            Verify(fun->get_return_type()->get_type_id() == inst->get_operand(0)->get_type()->get_type_id(),
                   "Wrong function return type!", inst, bb, fun);
        }
    }
}

void Check::checkCallInstruction(Instruction *inst) {
    BasicBlock *bb = inst->get_parent();
    Function *fun = inst->get_function();

    // Check the consistency of return types.
    auto *func = dynamic_cast<Function *>(inst->get_operand(0));
    auto *funcTy = func->get_function_type();
    Verify(funcTy->get_return_type()->get_type_id() == inst->get_type()->get_type_id(),
           "Wrong call function return type!", inst, bb, fun);

    // Check nums of parameter.
    Verify(inst->get_num_operand() - 1 == funcTy->get_num_of_args(), "The number of parameters is inconsistent!", inst,
           bb, fun);

    // Check the consistency of parameter types.
    for (int i = 1; i < inst->get_num_operand(); i++) {
        Verify(inst->get_operand(i)->get_type()->get_type_id() == funcTy->get_param_type(i - 1)->get_type_id(),
               "Parameter type conflict!", inst, bb, fun);
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