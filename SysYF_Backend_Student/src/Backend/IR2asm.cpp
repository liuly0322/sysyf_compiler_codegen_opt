#include<IR2asm.h>

namespace IR2asm{

std::string ldr_const(Reg* rd, constant *val, std::string cmpop) {
    std::string asmstr;
    asmstr += space;
    asmstr += "ldr" + cmpop + " ";
    asmstr += rd->get_code();
    asmstr += ", =";
    asmstr += val->get_num();
    asmstr += endl;
    return asmstr;
}

std::string mov(Reg* rd, Operand2 *opr2) {
    std::string asmstr;
    asmstr += space;
    asmstr += "mov ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += opr2->get_code();
    asmstr += endl;
    return asmstr;
}

std::string beq(Location *addr) {
    std::string asmstr;
    asmstr += space;
    asmstr += "beq ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string bne(Location *addr) {
    std::string asmstr;
    asmstr += space;
    asmstr += "bne ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string bgt(Location *addr) {
    std::string asmstr;
    asmstr += space;
    asmstr += "bgt ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string bge(Location *addr) {
    std::string asmstr;
    asmstr += space;
    asmstr += "bge ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string blt(Location *addr) {
    std::string asmstr;
    asmstr += space;
    asmstr += "blt ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string ble(Location *addr) {
    std::string asmstr;
    asmstr += space;
    asmstr += "ble ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string getelementptr(Reg* rd, Location * ptr){
    std::string asmstr;
    auto regbase = dynamic_cast<Regbase *>(ptr);
    if(regbase){
        if (regbase->get_offset() >= (1<<8) || regbase->get_offset() < 0) {
            asmstr += ldr_const(rd, new IR2asm::constant(regbase->get_offset()));
            asmstr += add(rd, &regbase->get_reg(), new Operand2(*rd));
        } else {
            asmstr += add(rd, &regbase->get_reg(), new Operand2(regbase->get_offset()));
        }
    }
    else{
        asmstr += space;
        asmstr += "ldr ";
        asmstr += rd->get_code();
        asmstr += ", ";
        auto labelexpr = dynamic_cast<label *>(ptr);
        asmstr += "";
        asmstr += labelexpr->get_code();
        asmstr += endl;
    }
    return asmstr;
}

std::string call(label* fun){
    std::string asmstr;
    asmstr += space;
    asmstr += "bl ";
    asmstr += fun->get_code();
    asmstr += endl;
    return asmstr;
}

std::string ret(Location *addr){
    std::string asmstr;
    asmstr += space;
    asmstr += "ldr r0, ";
    asmstr += addr->get_code();
    asmstr += endl;
    // asmstr += space;
    // asmstr += "bx lr" + endl;
    return asmstr;
}

std::string ret(Value* retval){
    auto const_retval = dynamic_cast<IR2asm::constant*>(retval);
    std::string asmstr;
    asmstr += space;
    if (const_retval) {
        asmstr += "ldr r0, =";
        asmstr += const_retval->get_num();
    } else {
        asmstr += "mov r0, ";
        asmstr += retval->get_code();
    }
    asmstr += endl;
    // asmstr += space;
    // asmstr += "br lr" + endl;
    return asmstr;
}

std::string ret(){
    std::string asmstr;
    asmstr += space;//TODO:RET NULL STRING NEEDED
    asmstr += "bx lr" + endl;
    return asmstr;
}

std::string br(Location* label) {
    return b(label);
}

std::string b(Location *addr) {
    std::string asmstr;
    asmstr += space;
    asmstr += "b ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string cmp(Reg* rs, Operand2* opr2) {
    std::string asmstr;
    asmstr += space;
    asmstr += "cmp ";
    asmstr += rs->get_code();
    asmstr += ", ";
    asmstr += opr2->get_code();
    asmstr += endl;
    return asmstr;
}

std::string add(Reg* rd, Reg* rs, Operand2* opr2){
    std::string asmstr;
    asmstr += space;
    asmstr += "add ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += rs->get_code();
    asmstr += ", ";
    asmstr += opr2->get_code();
    asmstr += endl;
    return asmstr;
}

std::string sub(Reg* rd, Reg* rs, Operand2* opr2){
    std::string asmstr;
    asmstr += space;
    asmstr += "sub ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += rs->get_code();
    asmstr += ", ";
    asmstr += opr2->get_code();
    asmstr += endl;
    return asmstr;
}

std::string r_sub(Reg* rd, Reg* rs, Operand2* opr2){
    std::string asmstr;
    asmstr += space;
    asmstr += "rsb ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += rs->get_code();
    asmstr += ", ";
    asmstr += opr2->get_code();
    asmstr += endl;
    return asmstr;
}

std::string mul(Reg* rd, Reg* rs, Reg* rt){
    std::string asmstr;
    asmstr += space;
    asmstr += "mul ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += rs->get_code();
    asmstr += ", ";
    asmstr += rt->get_code();
    asmstr += endl;
    return asmstr;
}

std::string sdiv(Reg* rd, Reg* rs, Reg* rt){
    std::string asmstr;
    asmstr += space;
    asmstr += "sdiv ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += rs->get_code();
    asmstr += ", ";
    asmstr += rt->get_code();
    asmstr += endl;
    return asmstr;
}

std::string srem(Reg* rd, Reg* rs, Operand2* opr2){
    std::string asmstr;
    asmstr += space;
    asmstr += "srem ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += rs->get_code();
    asmstr += ", ";
    asmstr += opr2->get_code();
    asmstr += endl;
    return asmstr;
}

std::string load(Reg* rd, Location* addr, std::string cmpop){
    std::string asmstr;
    asmstr += space;
    asmstr += "ldr" + cmpop + " ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string safe_load(Reg* rd, Location* addr, int sp_extra_ofst, bool long_func, std::string cmpop){
    std::string asmstr;
    bool is_sp_based = false;
    if(dynamic_cast<Regbase*>(addr)){
        Regbase* regbase = dynamic_cast<Regbase*>(addr);
        is_sp_based = (regbase->get_reg().get_id() == sp);
        int offset = regbase->get_offset() + ((is_sp_based)?sp_extra_ofst: 0);
        if(abs(offset) > 4095){
            asmstr += space;
            asmstr += "ldr lr, =";
            asmstr += std::to_string(offset);
            asmstr += endl;
            asmstr += space;
            asmstr += "add lr, lr, ";
            asmstr += regbase->get_reg().get_code();
            asmstr += endl;
            asmstr += load(rd, new Regbase(Reg(lr), 0), cmpop);
        }
        else{
            if(is_sp_based){
                Regbase* new_regbase = new Regbase(*regbase);
                new_regbase->set_offset(offset);
                asmstr += load(rd, new_regbase, cmpop);
            }
            else{
                asmstr += load(rd, addr, cmpop);
            }
        }
    }
    else{
        label* labl = dynamic_cast<label*>(addr);
        if(long_func || labl->get_offset()){
            asmstr += space;
            asmstr += "ldr lr, =";
            asmstr += labl->get_code();
            asmstr += endl;
            asmstr += load(rd, new Regbase(Reg(lr), 0), cmpop);
        }
        else{
            asmstr += load(rd, addr, cmpop);
        }
    }
    return asmstr;
}

std::string store(Reg* rd, Location* addr, std::string cmpop){
    std::string asmstr;
    asmstr += space;
    asmstr += "str" + cmpop + " ";
    asmstr += rd->get_code();
    asmstr += ", ";
    asmstr += addr->get_code();
    asmstr += endl;
    return asmstr;
}

std::string safe_store(Reg* rd, Location* addr, int sp_extra_ofst, bool long_func, std::string cmpop){
    std::string asmstr;
    bool is_sp_based = false;
    if(dynamic_cast<Regbase*>(addr)){
        Regbase* regbase = dynamic_cast<Regbase*>(addr);
        is_sp_based = (regbase->get_reg().get_id() == sp);
        int offset = regbase->get_offset() + ((is_sp_based)?sp_extra_ofst: 0);
        if(abs(offset) > 4095){
            asmstr += space;
            asmstr += "ldr lr, =";
            asmstr += std::to_string(offset);
            asmstr += endl;
            asmstr += space;
            asmstr += "add lr, lr, ";
            asmstr += regbase->get_reg().get_code();
            asmstr += endl;
            asmstr += store(rd, new Regbase(Reg(lr), 0), cmpop);
        }
        else{
            if(is_sp_based){
                Regbase* new_regbase = new Regbase(*regbase);
                new_regbase->set_offset(offset);
                asmstr += store(rd, new_regbase, cmpop);
            }
            else{
                asmstr += store(rd, addr, cmpop);
            }
        }
    }
    else{
        label* labl = dynamic_cast<label*>(addr);
        if(long_func || labl->get_offset()){
            asmstr += space;
            asmstr += "ldr lr, =";
            asmstr += labl->get_code();
            asmstr += endl;
            asmstr += store(rd, new Regbase(Reg(lr), 0), cmpop);
        }
        else{
            asmstr += store(rd, addr, cmpop);
        }
    }
    return asmstr;
}
}

