#ifndef SYSYF_MEM2REG_H
#define SYSYF_MEM2REG_H

#include "BasicBlock.h"
#include "Function.h"
#include "IRBuilder.h"
#include "Instruction.h"
#include "Module.h"
#include "Pass.h"

class Mem2Reg : public Pass{
private:
	Function *func_; // 一次优化针对的是一个函数，这个Function应该就是优化的目标函数
	IRBuilder *builder; // builder是生成中间表示的类，可能需要互相引用
	std::map<BasicBlock *, std::vector<Value *>> define_var; // 基本块定义的值
    const std::string name = "Mem2Reg"; // 优化遍名字
	std::map<Value*, Value*> lvalue_connection; // 变量值存储
	std::set<Value*> no_union_set; // ?

public:
	explicit Mem2Reg(Module *m) : Pass(m) {}
	~Mem2Reg(){};
	void execute() final;
	void genPhi(); // 生成φ函数
	void insideBlockForwarding();
	void valueDefineCounting();
	void valueForwarding(BasicBlock *bb);
	void removeAlloc();
	void phiStatistic();
	const std::string get_name() const override { return name; }

	bool isLocalVarOp(Instruction* inst) {
		// 如果指令是store类型或者load类型
		if (inst->get_instr_type() == Instruction::OpID::store) {
			StoreInst* sinst = static_cast<StoreInst*>(inst);
			// 获得指令左值
			auto lvalue = sinst->get_lval();
			// 看是否是全局变量
			auto glob = dynamic_cast<GlobalVariable*>(lvalue);
			// 看是否是数组指针
			auto array_element_ptr = dynamic_cast<GetElementPtrInst*>(lvalue);
			// 如果不是全局变量，也不是数组指针，返回真（这些变量无需放置φ函数）
			return !glob && !array_element_ptr;
		}
		else if (inst->get_instr_type() == Instruction::OpID::load) {
			LoadInst* linst = static_cast<LoadInst*>(inst);
			auto lvalue = linst->get_lval();
			auto glob = dynamic_cast<GlobalVariable*>(lvalue);
			auto array_element_ptr = dynamic_cast<GetElementPtrInst*>(lvalue);
			return !glob && !array_element_ptr;
		}
		else
			return false;
	}
	// void DeleteLS();
	//可加一个遍历删除空phi节点
};

#endif // SYSYF_MEM2REG_H