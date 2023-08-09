#ifndef CONSTSPREAD
#define CONSTSPREAD
#include "../ir/ir.h"
#include "BasicOperation.h"
#include "opt.h"

class ConstSpread : public Optimization {
public:
  ConstSpread(Module *m_) : Optimization(m_) {}
  void execute();
  ConstantInt *CalcInt(Instruction::OpID op, ConstantInt *v1, ConstantInt *v2);
  ConstantFloat *CalcFloat(Instruction::OpID op, ConstantFloat *v1,
                           ConstantFloat *v2);
  ConstantInt *CalcICMP(ICmpInst::ICmpOp op, ConstantInt *v1, ConstantInt *v2);
  ConstantInt *CalcFCMP(FCmpInst::FCmpOp op, ConstantFloat *v1, ConstantFloat *v2);
  bool SpreadingConst(Function *func);
  bool BranchProcess(Function *func);
  std::map<Value *, ConstantInt *> ConstIntMap;
  std::map<Value *, ConstantFloat *> ConstFloatMap;
  std::map<Instruction *, BasicBlock *> uselessInstr;
};

#endif // !CONSTSPREAD