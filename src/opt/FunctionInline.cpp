#include "FunctionInline.h"
extern int id;
extern std::map<Instruction::OpID, std::string> instr_id2string_;
void FunctionInline::execute() {
  buildCallerGraph();
  findRecursiveFunction();
  for (auto iter = m->function_list_.begin();
       iter != m->function_list_.end();) {
    auto foo = *iter;
    if (recursiveFunc.count(foo) || foo->basic_blocks_.empty() ||
        foo->basic_blocks_.size() > 10 || foo->name_ == "main") {
      iter++;
      continue;
    }
    for (auto it = foo->use_list_.begin(); it != foo->use_list_.end();) {
      auto call = *it++;
      inlineFunc(dynamic_cast<CallInst *>(call.val_));
    }
    iter = m->function_list_.erase(iter);
  }
}

void FunctionInline::buildCallerGraph() {
  for (auto foo : m->function_list_)
    for (auto use : foo->use_list_) {
      auto callInstr = dynamic_cast<CallInst *>(use.val_);
      auto callerFunc = callInstr->parent_->parent_;
      FunctionCall[callerFunc].insert(foo);
    }
}

void FunctionInline::findRecursiveFunction() {
  std::set<Function *> judged;
  std::set<Function *> vis;
  std::function<void(Function *)> dfs = [&](Function *place) {
    vis.insert(place);
    if (judged.find(place) == judged.end())
      for (auto callee : FunctionCall[place])
        if (vis.find(callee) != vis.end())
          recursiveFunc.insert(callee);
        else
          dfs(callee);
    judged.insert(place);
    vis.erase(place);
  };
  for (auto func : m->function_list_)
    dfs(func);
}

Value *transOperand(std::map<Value *, Value *> &old2NewIns, Value *old) {
  auto iter = old2NewIns.find(old);
  if (iter != old2NewIns.end())
    return iter->second;
  return old;
}

Instruction *insFactory(Instruction *ins,
                        std::map<Value *, Value *> &old2NewIns,
                        std::map<BasicBlock *, BasicBlock *> &old2NewBB) {
  Instruction *dup = nullptr;
  std::vector<Value *> args;
  switch (ins->op_id_) {
  case Instruction::Br:
    if (ins->num_ops_ == 3) {
      dup = new BranchInst(transOperand(old2NewIns, ins->get_operand(0)),
                           old2NewBB[(BasicBlock *)ins->get_operand(1)],
                           old2NewBB[(BasicBlock *)ins->get_operand(2)],
                           old2NewBB[ins->parent_]);
    } else {
      dup = new BranchInst(old2NewBB[(BasicBlock *)ins->get_operand(0)],
                           old2NewBB[ins->parent_]);
    }
    break;
  case Instruction::Add:
  case Instruction::Sub:
  case Instruction::Mul:
  case Instruction::SDiv:
  case Instruction::SRem:
  case Instruction::FAdd:
  case Instruction::FSub:
  case Instruction::FMul:
  case Instruction::FDiv:
    dup = new BinaryInst(
        ins->type_, ins->op_id_, transOperand(old2NewIns, ins->get_operand(0)),
        transOperand(old2NewIns, ins->get_operand(1)), old2NewBB[ins->parent_]);
    break;
  case Instruction::Alloca:
    dup = new AllocaInst(((AllocaInst *)ins)->alloca_ty_,
                         old2NewBB[ins->parent_]);
    break;
  case Instruction::Load:
    dup = new LoadInst(transOperand(old2NewIns, ins->get_operand(0)),
                       old2NewBB[ins->parent_]);
    break;
  case Instruction::Store:
    dup = new StoreInst(transOperand(old2NewIns, ins->get_operand(0)),
                        transOperand(old2NewIns, ins->get_operand(1)),
                        old2NewBB[ins->parent_]);
    break;
  case Instruction::GetElementPtr:
    for (int i = 1; i < ins->operands_.size(); i++) {
      args.push_back(transOperand(old2NewIns, ins->get_operand(i)));
    }
    dup = new GetElementPtrInst(transOperand(old2NewIns, ins->get_operand(0)),
                                args, old2NewBB[ins->parent_]);
    break;
  case Instruction::ZExt:
    dup =
        new ZextInst(ins->op_id_, transOperand(old2NewIns, ins->get_operand(0)),
                     ins->type_, old2NewBB[ins->parent_]);
    break;
  case Instruction::FPtoSI:
    dup = new FpToSiInst(ins->op_id_,
                         transOperand(old2NewIns, ins->get_operand(0)),
                         ins->type_, old2NewBB[ins->parent_]);
    break;
  case Instruction::SItoFP:
    dup = new SiToFpInst(ins->op_id_,
                         transOperand(old2NewIns, ins->get_operand(0)),
                         ins->type_, old2NewBB[ins->parent_]);
    break;
  case Instruction::BitCast:
    dup =
        new Bitcast(ins->op_id_, transOperand(old2NewIns, ins->get_operand(0)),
                    ins->type_, old2NewBB[ins->parent_]);
    break;
  case Instruction::ICmp:
    dup = new ICmpInst(((ICmpInst *)ins)->icmp_op_,
                       transOperand(old2NewIns, ins->get_operand(0)),
                       transOperand(old2NewIns, ins->get_operand(1)),
                       old2NewBB[ins->parent_]);
    break;
  case Instruction::FCmp:
    dup = new FCmpInst(((FCmpInst *)ins)->fcmp_op_,
                       transOperand(old2NewIns, ins->get_operand(0)),
                       transOperand(old2NewIns, ins->get_operand(1)),
                       old2NewBB[ins->parent_]);
    break;
  case Instruction::Call:
    for (int i = 0; i < ins->operands_.size() - 1; i++) {
      args.push_back(transOperand(old2NewIns, ins->get_operand(i)));
    }
    dup = new CallInst((Function *)ins->get_operand(ins->operands_.size() - 1),
                       args, old2NewBB[ins->parent_]);
    break;
  case Instruction::Ret:
    break;
  case Instruction::PHI:
    break;
  case Instruction::FNeg:
  case Instruction::UDiv:
  case Instruction::URem:
  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
    break;
  }
  return dup;
}

void FunctionInline::inlineFunc(CallInst *call) {
  assert(call != nullptr);
  auto callBB = call->parent_;
  auto callFunc = call->parent_->parent_;
  assert(!call->operands_.empty());
  auto funcCalled =
      dynamic_cast<Function *>(call->get_operand(call->operands_.size() - 1));
  auto splitBB = new BasicBlock(m, std::to_string(id++), callFunc);
  Value *newRetVal = nullptr;
  bool moveTo = false;
  for (auto iter = callBB->instr_list_.begin();
       iter != callBB->instr_list_.end();) {
    auto ins = *iter++;
    if (moveTo) {
      callBB->remove_instr(ins);
      ins->parent_ = splitBB;
      splitBB->add_instruction(ins);
    } else if (ins == call)
      moveTo = true;
  }
  for (auto succBB : callBB->succ_bbs_) {
    succBB->remove_pre_basic_block(callBB);
    succBB->add_pre_basic_block(splitBB);
    splitBB->add_succ_basic_block(succBB);
  }
  callBB->succ_bbs_.clear();
  std::map<Value *, Value *> old2NewIns;
  std::map<BasicBlock *, BasicBlock *> old2NewBB;
  for (int i = 0; i < funcCalled->arguments_.size(); i++)
    old2NewIns[funcCalled->arguments_[i]] = call->get_operand(i);
  for (auto oldBB : funcCalled->basic_blocks_) {
    auto newBB = new BasicBlock(m, std::to_string(id++), callFunc);
    old2NewBB[oldBB] = newBB;
  }
  std::list<BasicBlock *> stackOldBB;
  std::set<BasicBlock *> visitedOldBB;
  stackOldBB.push_back(funcCalled->basic_blocks_[0]);
  visitedOldBB.insert(funcCalled->basic_blocks_[0]);
  while (!stackOldBB.empty()) {
    auto oldBB = stackOldBB.back();
    stackOldBB.pop_back();
    for (auto oldIns : oldBB->instr_list_) {
      if (oldIns->is_ret()) {
        auto newBB = old2NewBB[oldBB];
        if (newRetVal != nullptr)
          return;
        new BranchInst(splitBB, newBB);
        if (!oldIns->operands_.empty()) {
          newRetVal = old2NewIns[oldIns->operands_[0]];
          call->replace_all_use_with(newRetVal);
        }
        callBB->delete_instr(call);
        new BranchInst(old2NewBB[funcCalled->basic_blocks_[0]], callBB);
        newBB->delete_instr(oldIns);
      } else {
        Instruction *newIns = insFactory(oldIns, old2NewIns, old2NewBB);
        old2NewIns[oldIns] = newIns;
        if (oldIns->op_id_ == Instruction::Br) {
          if (oldIns->operands_.size() == 1) {
            auto nextBB = (BasicBlock *)oldIns->get_operand(0);
            if (!visitedOldBB.count(nextBB)) {
              stackOldBB.push_back(nextBB);
              visitedOldBB.insert(nextBB);
            }
          } else if (oldIns->operands_.size() == 3) {
            auto trueBB = (BasicBlock *)oldIns->get_operand(1);
            auto falseBB = (BasicBlock *)oldIns->get_operand(2);
            if (!visitedOldBB.count(trueBB)) {
              stackOldBB.push_back(trueBB);
              visitedOldBB.insert(trueBB);
            }
            if (!visitedOldBB.count(falseBB)) {
              stackOldBB.push_back(falseBB);
              visitedOldBB.insert(falseBB);
            }
          }
        }
      }
    }
  }
  for (auto &iter : old2NewBB)
    if (!visitedOldBB.count(iter.first))
      callFunc->remove_bb(iter.second);
}