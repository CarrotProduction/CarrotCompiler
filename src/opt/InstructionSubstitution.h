#ifndef INSTRUCTIONSUBSTITUTION
#define INSTRUCTIONSUBSTITUTION

#include "BasicOperation.h"
#include "opt.h"

class InstructionSubstitution : public Optimization {
  bool checkMulConst(Value *val);
  bool checkDivRemConst(Value *val);

public:
  InstructionSubstitution(Module *m) : Optimization(m) {}
  void execute();
  void SolveMul(Instruction *instr, BasicBlock *bb);
  void SolveDiv(Instruction *instr, BasicBlock *bb);
  void SolveRem(Instruction *instr, BasicBlock *bb);
};

#endif // !INSTRUCTIONSUBSTITUTION