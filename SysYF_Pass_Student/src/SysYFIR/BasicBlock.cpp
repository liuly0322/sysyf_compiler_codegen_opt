#include "Module.h"
#include "BasicBlock.h"
#include "Function.h"
#include "IRPrinter.h"
#ifdef DEBUG
#include <cassert>
#endif
#include <algorithm>

BasicBlock::BasicBlock(Module *m, const std::string &name = "",
                      Function *parent = nullptr)
    : Value(Type::get_label_type(m), name), parent_(parent)
{
#ifdef DEBUG
    assert(parent && "currently parent should not be nullptr");
#endif
    parent_->add_basic_block(this);
}

Module *BasicBlock::get_module()
{
    return get_parent()->get_parent();
}

void BasicBlock::add_instruction(Instruction *instr)
{
    instr_list_.push_back(instr);
}

void BasicBlock::remove_phi_from(BasicBlock *bb) {
    auto delete_phi = std::vector<Instruction *>{};
    for (auto *inst : instr_list_) {
        if (!inst->is_phi())
            break;
        for (auto i = 1U; i < inst->get_operands().size();) {
            auto *op = inst->get_operand(i);
            if (op == bb) {
                inst->remove_operands(i - 1, i);
            } else {
                i += 2;
            }
        }
        if (inst->get_num_operand() == 2) {
            inst->replace_all_use_with(inst->get_operand(0));
            delete_phi.push_back(inst);
        }
    }
    for (auto *inst : delete_phi) {
        delete_instr(inst);
    }
}

void BasicBlock::add_instruction(std::list<Instruction *>::iterator instr_pos, Instruction *instr)
{
    instr_list_.insert(instr_pos, instr);
}

void BasicBlock::add_instr_begin(Instruction *instr)
{
    instr_list_.push_front(instr);
}

std::list<Instruction *>::iterator BasicBlock::find_instruction(Instruction *instr)
{
    return std::find(instr_list_.begin(), instr_list_.end(), instr);
}

void BasicBlock::delete_instr( Instruction *instr )
{
    instr_list_.remove(instr);
    instr->remove_use_of_ops();
}

const Instruction *BasicBlock::get_terminator() const
{
    if (instr_list_.empty()){
        return nullptr;
    }
    switch (instr_list_.back()->get_instr_type())
    {
    case Instruction::ret:
        return instr_list_.back();
        break;
    
    case Instruction::br:
        return instr_list_.back();
        break;

    default:
        return nullptr;
        break;
    }
}

void BasicBlock::erase_from_parent()
{
    this->get_parent()->remove(this);
}

std::string BasicBlock::print()
{
    std::string bb_ir;
    bb_ir += this->get_name();
    bb_ir += ":";
    // print prebb
    if(!this->get_pre_basic_blocks().empty())
    {
        bb_ir += "                                                ; preds = ";
    }
    for (auto bb : this->get_pre_basic_blocks() )
    {
        if( bb != *this->get_pre_basic_blocks().begin() )
            bb_ir += ", ";
        bb_ir += print_as_op(bb, false);
    }
    
    // print prebb
    if ( !this->get_parent() )
    {
        bb_ir += "\n";
        bb_ir += "; Error: Block without parent!";
    }
    bb_ir += "\n";
    for ( auto instr : this->get_instructions() )
    {
        bb_ir += "  ";
        bb_ir += instr->print();
        bb_ir += "\n";
    }

    return bb_ir;
}