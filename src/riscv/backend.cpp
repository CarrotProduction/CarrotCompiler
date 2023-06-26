#include "backend.h"

RiscvBasicBlock *createRiscvBasicBlock(BasicBlock *bb = nullptr) {
  if (bb == nullptr) {
    LableCount++;
    return new RiscvBasicBlock(toLable(LableCount), LableCount);
  }
  if (rbbLabel.count(bb))
    return rbbLabel[bb];
  LableCount++;
  auto cur = new RiscvBasicBlock(toLable(LableCount), LableCount);
  return rbbLabel[bb] = cur;
}

RiscvFunction *createRiscvFunction(Function *foo) {
  assert(foo != nullptr);
  if (functionLabel.count(foo) == 0) {
    auto ty = RiscvOperand::Void;
    switch (foo->type_->tid_)
    {
    case Type::VoidTyID:
      ty = RiscvOperand::Void;
      break;
    case Type::IntegerTyID:
      ty = RiscvOperand::IntReg;
      break;
    case Type::FloatTyID:
      ty = RiscvOperand::FloatReg;
      break;
    }
    RiscvFunction *cur =
        new RiscvFunction(foo->name_, foo->arguments_.size(), ty);
    return functionLabel[foo] = cur;
  }
  return functionLabel[foo];
}

// 全部合流
void RiscvBuilder::solvePhiInstr(PhiInst *instr) {
  int n = instr->operands_.size();
  for (int i = 1; i < n; i++)
    DSU_for_Variable.merge(instr->operands_[i]->name_,
                           instr->operands_[0]->name_);
}

BinaryRiscvInst *RiscvBuilder::createBinaryInstr(BinaryInst *binaryInstr) {
  BinaryRiscvInst *instr =
      new BinaryRiscvInst(toRiscvOp.at(binaryInstr->op_id_),
                          regAlloca->find(binaryInstr->operands_[0]),
                          regAlloca->find(binaryInstr->operands_[1]),
                          regAlloca->find(binaryInstr), this->rbb);
  return instr;
}

UnaryRiscvInst *RiscvBuilder::createUnaryInstr(UnaryInst *unaryInstr) {
  UnaryRiscvInst *instr =
      new UnaryRiscvInst(toRiscvOp.at(unaryInstr->op_id_),
                         regAlloca->find(unaryInstr->operands_[0]),
                         regAlloca->find(unaryInstr), this->rbb);
  return instr;
}

StoreRiscvInst *RiscvBuilder::createStoreInstr(StoreInst *storeInstr) {
  StoreRiscvInst *instr = new StoreRiscvInst(
      storeInstr->type_, regAlloca->find(storeInstr->operands_[0]),
      regAlloca->find(storeInstr->operands_[1]), this->rbb);
}

LoadRiscvInst *RiscvBuilder::createLoadInstr(LoadInst *loadInstr) {
  LoadRiscvInst *instr = new LoadRiscvInst(
      loadInstr->type_, regAlloca->find(loadInstr->operands_[0]),
      regAlloca->find(loadInstr->operands_[1]), this->rbb);
}

ICmpRiscvInstr *RiscvBuilder::creatreICMPInstr(ICmpInst *icmpInstr,
                                               BranchInst *brInstr) {
  ICmpRiscvInstr *instr = new ICmpRiscvInstr(
      icmpInstr->icmp_op_, regAlloca->find(icmpInstr->operands_[0]),
      regAlloca->find(icmpInstr->operands_[1]),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
      this->rbb);
  return instr;
}

FCmpRiscvInstr *RiscvBuilder::createFCMPInstr(FCmpInst *fcmpInstr,
                                              BranchInst *brInstr) {
  FCmpRiscvInstr *instr = new FCmpRiscvInstr(
      fcmpInstr->fcmp_op_, regAlloca->find(fcmpInstr->operands_[0]),
      regAlloca->find(fcmpInstr->operands_[1]),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
      this->rbb);
  return instr;
}

CallRiscvInst *RiscvBuilder::createCallInstr(CallInst *callInstr) {
  std::vector<RiscvOperand *> args;
  for (int i = 1; i < callInstr->operands_.size(); i++)
    args.push_back(regAlloca->find(callInstr->operands_[i]));
  // 涉及从Function 到RISCV function转换问题（第一个参数）
  CallRiscvInst *instr = new CallRiscvInst(
      createRiscvFunction(static_cast<Function *>(callInstr->operands_[0])),
      this->rbb, args);
  return instr;
}