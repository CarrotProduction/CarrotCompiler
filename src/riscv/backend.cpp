#include "backend.h"

RiscvBasicBlock *createRiscvBasicBlock(BasicBlock *bb) {
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
    switch (foo->type_->tid_) {
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

BinaryRiscvInst *RiscvBuilder::createBinaryInstr(BinaryInst *binaryInstr,
                                                 RiscvBasicBlock *rbb) {
  BinaryRiscvInst *instr =
      new BinaryRiscvInst(toRiscvOp.at(binaryInstr->op_id_),
                          regAlloca->find(binaryInstr->operands_[0], rbb),
                          regAlloca->find(binaryInstr->operands_[1], rbb),
                          regAlloca->find(binaryInstr, rbb), rbb);
  return instr;
}

void RiscvBuilder::solveAlloca(AllocaInst* instr, RiscvFunction* foo, RiscvBasicBlock *rbb) {
  foo->addArgs(regAlloca->findNonuse(rbb));
}

UnaryRiscvInst *RiscvBuilder::createUnaryInstr(UnaryInst *unaryInstr,
                                               RiscvBasicBlock *rbb) {
  UnaryRiscvInst *instr =
      new UnaryRiscvInst(toRiscvOp.at(unaryInstr->op_id_),
                         regAlloca->find(unaryInstr->operands_[0], rbb),
                         regAlloca->find(unaryInstr, rbb), rbb);
  return instr;
}

StoreRiscvInst *RiscvBuilder::createStoreInstr(StoreInst *storeInstr,
                                               RiscvBasicBlock *rbb) {
  StoreRiscvInst *instr = new StoreRiscvInst(
      storeInstr->type_, regAlloca->find(storeInstr->operands_[0], rbb),
      regAlloca->find(storeInstr->operands_[1], rbb), rbb);
  return instr;
}

LoadRiscvInst *RiscvBuilder::createLoadInstr(LoadInst *loadInstr,
                                             RiscvBasicBlock *rbb) {
  LoadRiscvInst *instr = new LoadRiscvInst(
      loadInstr->type_, regAlloca->find(loadInstr->operands_[0], rbb),
      regAlloca->find(loadInstr->operands_[1], rbb), rbb);
  return instr;
}

ICmpRiscvInstr *RiscvBuilder::createICMPInstr(ICmpInst *icmpInstr,
                                              BranchInst *brInstr,
                                              RiscvBasicBlock *rbb) {
  ICmpRiscvInstr *instr = new ICmpRiscvInstr(
      icmpInstr->icmp_op_, regAlloca->find(icmpInstr->operands_[0], rbb),
      regAlloca->find(icmpInstr->operands_[1], rbb),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
      rbb);
  return instr;
}

// FCMP处理上和ICMP类似，但是在最后生成语句的时候是输出两句
FCmpRiscvInstr *RiscvBuilder::createFCMPInstr(FCmpInst *fcmpInstr,
                                              BranchInst *brInstr,
                                              RiscvBasicBlock *rbb) {
  FCmpRiscvInstr *instr = new FCmpRiscvInstr(
      fcmpInstr->fcmp_op_, regAlloca->find(fcmpInstr->operands_[0], rbb),
      regAlloca->find(fcmpInstr->operands_[1], rbb), regAlloca->findNonuse(rbb),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
      rbb);
  return instr;
}

CallRiscvInst *RiscvBuilder::createCallInstr(CallInst *callInstr,
                                             RiscvBasicBlock *rbb) {
  std::vector<RiscvOperand *> args;
  for (int i = 1; i < callInstr->operands_.size(); i++)
    args.push_back(regAlloca->find(callInstr->operands_[i], rbb));
  // 涉及从Function 到RISCV function转换问题（第一个参数）
  CallRiscvInst *instr = new CallRiscvInst(
      createRiscvFunction(static_cast<Function *>(callInstr->operands_[0])),
      rbb, args);
  return instr;
}

// 总控程序
void RiscvBuilder::buildRISCV(Module *m) {
  
}