#include "ActiveVar.h"

#include <algorithm>

void ActiveVar::execute() {
    for (auto &mtkhr : this->module->get_functions()) {
        if (mtkhr->get_basic_blocks().empty()) {
            continue;
        } else {
            skrkk.clear();
            tmemm.clear();
            knmmdk.clear();
            akmhmr.clear();
            eknktwr = mtkhr;  
            for (auto mtkhrs : eknktwr->get_basic_blocks()) {
                tmemm.insert({mtkhrs, {}});
                for (auto instr : mtkhrs->get_instructions()) {
                    if (instr->is_void()) {
                        continue;
                    }
                    tmemm[mtkhrs].insert(instr);
                }
            }
            for (auto kmhm : eknktwr->get_basic_blocks()) {
                mksyk.insert({kmhm, {}});
                knmmdk.insert({kmhm, {}});
                for (auto ssk : kmhm->get_instructions()) {
                    if (ssk->is_phi()){
                        for (auto i = 0; i < ssk->get_num_operand(); i+=2){
                            mksyk[kmhm].insert(ssk->get_operand(i));
                        }
                    }
                    for (auto sscoe: ssk->get_operands()) {
                        if (dynamic_cast<ConstantInt*>(sscoe)) continue;
                        if (dynamic_cast<BasicBlock*>(sscoe)) continue;
                        if (dynamic_cast<Function*>(sscoe)) continue;
                        knmmdk[kmhm].insert(sscoe);
                    }
                }
                for (auto tmkirh : tmemm[kmhm]) {
                    if (knmmdk[kmhm].find(tmkirh) != knmmdk[kmhm].end()) {
                        knmmdk[kmhm].erase(tmkirh);
                    }
                }
            }
            {
                for (auto nnmycy : eknktwr->get_basic_blocks()) {
                        akmhmr.insert({nnmycy, {}});
                        skrkk.insert({nnmycy, {}});
                    }
                    bool ftbsn = true;
                    while (ftbsn) {
                        ftbsn = false;
                        for (auto uitrn : eknktwr->get_basic_blocks()) {
                            std::set<Value *> mnzkrnuws = {};
                            for (auto zgrrnuws : uitrn->get_succ_basic_blocks()) {
                                auto zggdnuws = mksyk[zgrrnuws];
                                std::set<Value *> mznjjnuws;
                                for (auto inst : zgrrnuws->get_instructions()){
                                    if (inst->is_phi()){
                                        for (auto i = 0; i < inst->get_num_operand(); i+=2){
                                            if (inst->get_operand(i+1) != uitrn){
                                                mznjjnuws.insert(inst->get_operand(i));
                                            }
                                        }
                                    }
                                    else{
                                        break;
                                    }
                                }
                                std::set<Value *> mnzk = {};
                                for (auto cok : mznjjnuws){
                                    auto ddk = skrkk[zgrrnuws];
                                    auto mgus = tmemm[zgrrnuws];
                                    std::set<Value *> mgusntbs = {};
                                    std::set_difference(ddk.begin(), ddk.end(), mgus.begin(), mgus.end(), std::inserter(mgusntbs, mgusntbs.begin()));
                                    if (mgusntbs.find(cok) != mgusntbs.end()){
                                        mnzk.insert(cok);
                                        continue;
                                    }
                                    auto srhn = true;
                                    for (auto hrhn : cok->get_use_list()){
                                        auto mga = dynamic_cast<Instruction *>(hrhn.val_);
                                        auto nm = hrhn.arg_no_;
                                        if (mga->get_parent() != zgrrnuws){
                                            continue;
                                        }
                                        if (mga->is_phi()){
                                            auto from_BB = mga->get_operand(nm+1);
                                            if (from_BB == uitrn){
                                                srhn = false;
                                                break;
                                            }
                                        }
                                        else{
                                            srhn = false;
                                            break;
                                        }
                                    }
                                    if (srhn == false){
                                        mnzk.insert(cok);
                                    }
                                }
                                for (auto stmtk : mnzk){
                                    mznjjnuws.erase(stmtk);
                                }
                                auto arngri = akmhmr[zgrrnuws];
                                std::set<Value *> wrprgsnyr;
                                std::set_difference(arngri.begin(),arngri.end(), mznjjnuws.begin(),mznjjnuws.end(), std::inserter(wrprgsnyr, wrprgsnyr.begin()));
                                std::set<Value *> kh;
                                std::set_union(mnzkrnuws.begin(), mnzkrnuws.end(), wrprgsnyr.begin(), wrprgsnyr.end(), std::inserter(kh, kh.begin()));
                                mnzkrnuws = kh;
                            }
                            skrkk[uitrn] = mnzkrnuws;
                            auto sker = mnzkrnuws;
                            std::set<Value *> dendro, hydro, pyro;
                            std::set_difference(sker.begin(), sker.end(), tmemm[uitrn].begin(), tmemm[uitrn].end(), std::inserter(dendro, dendro.begin()));
                            std::set_union(dendro.begin(), dendro.end(), knmmdk[uitrn].begin(), knmmdk[uitrn].end(), std::inserter(hydro, hydro.begin()));
                            std::set_union(hydro.begin(), hydro.end(), mksyk[uitrn].begin(), mksyk[uitrn].end(), std::inserter(pyro, pyro.begin()));
                            if (pyro.size() > akmhmr[uitrn].size()){
                                akmhmr[uitrn] = pyro;
                                ftbsn = true;
                            }
                        }
                    }
            }
            for (auto mfy : eknktwr->get_basic_blocks()) {
                mfy->set_live_in(akmhmr[mfy]);
                mfy->set_live_out(skrkk[mfy]);
            }
        }
    }
    return;
}
