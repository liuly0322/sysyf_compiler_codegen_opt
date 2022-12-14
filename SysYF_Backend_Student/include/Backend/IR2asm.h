#ifndef MHSJ_IR2ASM_H
#define MHSJ_IR2ASM_H

#include<RegAlloc.h>
#include<ValueGen.h>
#include<Instruction.h>
#include<Module.h>
#include<LIR.h>


namespace IR2asm{
    const std::string space = std::string(4, ' ');
    const std::string endl = "\n";


    // RegAllocDriver* reg_alloc;
    // Module* m_;
    // Function* func_;

    enum CmpOp {
    EQ, // ==
    NE, // !=
    GT, // >
    GE, // >=
    LT, // <
    LE, // <=
    NOP
    };

    std::string ldr_const(Reg* rd, constant *val, std::string cmpop = "");
    std::string mov(Reg* rd, Operand2 *opr2);
    std::string getelementptr(Reg* rd, Location * ptr);
    std::string ret();
    std::string ret(Value* retval);
    std::string ret(Location *addr);
    std::string beq(Location *addr);
    std::string bne(Location *addr);
    std::string bgt(Location *addr);
    std::string bge(Location *addr);
    std::string blt(Location *addr);
    std::string ble(Location *addr);
    std::string b(Location* label);
    std::string br(Location* label);
    std::string cmp(Reg* rs, Operand2* opr2);
    std::string add(Reg* rd, Reg* rs, Operand2* opr2);
    std::string sub(Reg* rd, Reg* rs, Operand2* opr2);
    std::string r_sub(Reg* rd, Reg* rs, Operand2* opr2);
    std::string mul(Reg* rd, Reg* rs, Reg* rt);
    std::string sdiv(Reg* rd, Reg* rs, Reg* rt);
    std::string srem(Reg* rd, Reg* rs, Operand2* opr2);
    std::string load(Reg* rd, Location* addr, std::string cmpop = "");
    std::string safe_load(Reg* rd, Location* addr, int sp_extra_ofst, bool long_func, std::string cmpop = "");
    std::string store(Reg* rs, Location* addr, std::string cmpop = "");
    std::string safe_store(Reg* rd, Location* addr, int sp_extra_ofst, bool long_func, std::string cmpop = "");
    std::string call(label* fun);
}

#endif