#ifndef _SYSYF_REGALLOC_H_
#define _SYSYF_REGALLOC_H_

#include "Value.h"
#include "Module.h"
#include <map>
#include <set>
#include <queue>

class Interval;
class RegAlloc;


struct Range{
    Range(int f,int t):from(f),to(t){}
    int from;
    int to;
};


class Interval{
public:
    explicit Interval(Value* value):val(value){}
    int reg_num = -1;
    Value* val;
    std::list<Range*> range_list;
    std::list<int> position_list;
    void add_range(int from,int to);
    void add_use_pos(int pos){position_list.push_front(pos);}
    bool covers(int id);
    bool covers(Instruction* inst);
    bool intersects(Interval* interval);
    void union_interval(Interval* interval);
};


struct cmp_interval{
    bool operator()(const Interval* a, const Interval* b) const {
        auto a_from = (*(a->range_list.begin()))->from;
        auto b_from = (*(b->range_list.begin()))->from;
        if(a_from!=b_from){
            return a_from < b_from;
        }else{
            return a->val->get_name() < b->val->get_name();
        }
    }
};

const int priority[] = {
        5,//r0
        4,//r1
        3,//r2
        2,//r3
        12,//r4
        11,//r5
        10,//r6
        9,//r7
        8,//r8
        7,//r9
        6,//r10
        -1,//r11
        1//r12
};


struct cmp_reg {
    bool operator()(const int reg1,const int reg2)const{
#ifdef DEBUG
        assert(reg1>=0&&reg1<=12&&reg2<=12&&reg2>=0&&"invalid reg id");
#endif
        return priority[reg1] > priority[reg2];
    }
};

const std::vector<int> all_reg_id = {0,1,2,3,4,5,6,7,8,9,10,12};

class RegAllocDriver{
public:
    explicit RegAllocDriver(Module* m):module(m){}
    void compute_reg_alloc();
    std::map<Value*, Interval*>& get_reg_alloc_in_func(Function* f){return reg_alloc[f];}
    std::list<BasicBlock*>& get_bb_order_in_func(Function* f){return bb_order[f];}
private:
    std::map<Function*, std::list<BasicBlock*>> bb_order;
    std::map<Function*,std::map<Value*,Interval*>> reg_alloc;
    Module* module;
};

/*****************Linear Scan Register Allocation*******************/

class RegAlloc{
public:
    explicit RegAlloc(Function* f):func(f){}
    void execute();
    std::map<Value*,Interval*>& get_reg_alloc(){return val2Inter;}
    std::list<BasicBlock*>& get_block_order(){return block_order;}
private:
    void compute_block_order();
    void number_operations();
    void build_intervals();
    void walk_intervals();
    void set_unused_reg_num();
    void get_dfs_order(BasicBlock* bb,std::set<BasicBlock*>& visited);
    void add_interval(Interval* interval){interval_list.insert(interval);}
    void add_reg_to_pool(Interval* inter);
    bool try_alloc_free_reg();
    std::set<int> unused_reg_id = {all_reg_id.begin(),all_reg_id.end()};
    Interval* current = nullptr;
    std::map<Value*, Interval*> val2Inter;
    Function* func;
    std::list<BasicBlock*> block_order={};
    std::set<Interval*,cmp_interval> interval_list;
};

#endif // _SYSYF_REGALLOC_H_