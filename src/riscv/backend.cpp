#include "backend.h"

// 全部合流
void RiscvBuilder::solvePhiInstr(PhiInst* instr) {
  int n = instr->operands_.size();
  for (int i = 1; i < n; i++)
    DSU_for_Variable.merge(instr->operands_[i]->name_,
                           instr->operands_[0]->name_);
}

BinaryRiscvInst *RiscvBuilder::createBinaryInstr(BinaryInst *binaryInstr) {
  BinaryRiscvInst *instr =
      new BinaryRiscvInst(toRiscvOp.at(binaryInstr->op_id_), regAlloca->find(binaryInstr->operands_[0]),
                          regAlloca->find(binaryInstr->operands_[1]),
                          regAlloca->find(binaryInstr), this->rbb);
  return instr;
}

UnaryRiscvInst *RiscvBuilder::createUnaryInstr(UnaryInst *unaryInstr) {

}