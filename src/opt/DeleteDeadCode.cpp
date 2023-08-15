#include "DeleteDeadCode.h"
#include "ConstSpread.h"

std::set<std::string> OptFunc = {"getint",          "getfloat",
                                 "getch",           "getarray",
                                 "getfarray",       "putint",
                                 "putfloat",        "putch",
                                 "putarray",        "putfarray",
                                 "_sysy_starttime", "_sysy_stoptime",
                                 "memcpy",          "memclr",
                                 "memset",          "llvm.memset.p0.i32",
                                 "__aeabi_memcpy4", "__aeabi_memclr4",
                                 "__aeabi_memset4"};

void DeadCodeDeletion::initFuncPtrArg() {
  for (auto foo : m->function_list_) {
    if (foo->basic_blocks_.empty())
      continue;
    for (auto arg : foo->arguments_)
      if (arg->type_->tid_ == Type::PointerTyID) {
        if (!funcPtrArgs.count(foo))
          funcPtrArgs[foo] = {};
        funcPtrArgs[foo].insert(arg);
      }
  }
}
void DeadCodeDeletion::Init(Function *foo) {
  storePos.clear();
  for (auto bb : foo->basic_blocks_) {
    for (auto ins : bb->instr_list_) {
      if (ins->op_id_ == Instruction::GetElementPtr) {
      } else if (ins->op_id_ == Instruction::Store) {
        if (!storePos.count(ins->get_operand(1))) {
          storePos.insert({ins->get_operand(1), {}});
        }
        storePos[ins->get_operand(1)].push_back(ins);
        if (dynamic_cast<GlobalVariable *>(ins->get_operand(1)))
          OptFunc.insert(foo->name_);
        if (dynamic_cast<GetElementPtrInst *>(ins->get_operand(1)))
          OptFunc.insert(foo->name_);
        if (funcPtrArgs[foo].count(ins->get_operand(1)))
          OptFunc.insert(foo->name_);
      } else if (ins->op_id_ == Instruction::Call) {
        auto f = ins->get_operand(ins->operands_.size() - 1);
        if (OptFunc.count(f->name_))
          OptFunc.insert(foo->name_);
      }
    }
  }
}
bool DeadCodeDeletion::checkOpt(Function *foo, Instruction *ins) {
  if (ins->op_id_ == Instruction::Ret) {
    exitBlock = ins->parent_;
    return true;
  } else if (ins->op_id_ == Instruction::Call) {
    auto f = ins->get_operand(ins->operands_.size() - 1);
    return OptFunc.count(f->name_);
  } else if (ins->op_id_ == Instruction::Store) {
    if (dynamic_cast<GlobalVariable *>(ins->get_operand(1)))
      return true;
    if (dynamic_cast<GetElementPtrInst *>(ins->get_operand(1)))
      return true;
    if (funcPtrArgs[foo].count(ins->get_operand(1)))
      return true;
    return false;
  }
  return false;
}

void DeadCodeDeletion::findInstr(Function *foo) {
  std::list<Value *> workList;
  for (auto bb : foo->basic_blocks_) {
    for (auto ins : bb->instr_list_) {
      if (checkOpt(foo, ins)) {
        uselessInstr.insert(ins);
        workList.push_back(ins);
      }
    }
  }
  while (!workList.empty()) {
    auto ins = dynamic_cast<Instruction *>(workList.back());
    workList.pop_back();
    if (ins == nullptr) {
      continue;
    }
    for (auto operand : ins->operands_) {
      auto temp = dynamic_cast<Instruction *>(operand);
      if (!temp)
        continue;
      if (uselessInstr.insert(temp).second) {
        workList.push_back(temp);
      }
    }
    if (ins->op_id_ == Instruction::PHI) {
      for (int i = 1; i < ins->operands_.size(); i += 2) {
        auto bb = dynamic_cast<BasicBlock *>(ins->get_operand(i));
        auto br = bb->get_terminator();
        if (uselessInstr.insert(br).second) {
          workList.push_back(br);
        }
      }
    }
    if (storePos.count(ins)) {
      for (auto curInstr : storePos[ins]) {
        if (uselessInstr.insert(dynamic_cast<Instruction *>(curInstr)).second) {
          workList.push_back(curInstr);
        }
      }
      storePos.erase(ins);
    }
    if (uselessBlock.insert(ins->parent_).second) {
      for (auto RFrontier : ins->parent_->rdom_frontier_) {
        auto t = RFrontier->get_terminator();
        if (uselessInstr.insert(t).second) {
          workList.push_back(t);
        }
      }
    }
  }
}
void DeadCodeDeletion::deleteInstr(Function *foo) {
  int deleteCnt = 0, changeCnt = 0;
  for (auto bb : foo->basic_blocks_) {
    std::vector<Instruction *> ins2Del;
    for (auto ins : bb->instr_list_) {
      if (!uselessInstr.count(ins)) {
        if (ins->op_id_ != Instruction::Br) {
          ins2Del.push_back(ins);
        } else {
          if (ins->operands_.size() == 3) {
            changeCnt++;
            auto trueBB = dynamic_cast<BasicBlock *>(ins->get_operand(1));
            auto falseBB = dynamic_cast<BasicBlock *>(ins->get_operand(2));
            trueBB->remove_pre_basic_block(bb);
            falseBB->remove_pre_basic_block(bb);
            bb->remove_succ_basic_block(trueBB);
            bb->remove_succ_basic_block(falseBB);
            BasicBlock *temp = exitBlock;
            std::vector<BasicBlock *> rdoms(bb->rdoms_.begin(),
                                            bb->rdoms_.end());
            std::sort(rdoms.begin(), rdoms.end(),
                      [=](BasicBlock *x, BasicBlock *y) -> bool {
                        return x->rdoms_.count(y);
                      });
            for (auto rdbb : rdoms) {
              if (rdbb != bb && uselessBlock.count(rdbb)) {
                temp = rdbb;
                break;
              }
            }
            ins->remove_operands(0, 2);
            ins->num_ops_ = 1;
            ins->operands_.resize(1);
            ins->use_pos_.resize(1);
            ins->set_operand(0, temp);
            bb->add_succ_basic_block(temp);
            temp->add_pre_basic_block(bb);
          }
        }
      }
    }
    deleteCnt += ins2Del.size();
    for (auto ins : ins2Del) {
      bb->delete_instr(ins);
    }
  }
}

void DeadCodeDeletion::execute() {
  ReverseDomainTree reverseDomainTree(m);
  reverseDomainTree.execute();
  initFuncPtrArg();
  for (auto foo : m->function_list_)
    if (!foo->basic_blocks_.empty()) {
      Init(foo);
      findInstr(foo);
      deleteInstr(foo);
      DeleteUnusedBB(foo);
    }
}
