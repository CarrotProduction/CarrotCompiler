#include "InstructionSubstitution.h"

bool InstructionSubstitution::checkMulConst(Value *val) {
  if (dynamic_cast<ConstantInt *>(val) == nullptr)
    return false;
  int IntVal = dynamic_cast<ConstantInt *>(val)->value_;
  return IntVal > 0 && __builtin_popcount(IntVal) == 1;
}

bool InstructionSubstitution::checkDivRemConst(Value *val) {
  if (dynamic_cast<ConstantInt *>(val) == nullptr)
    return false;
  int IntVal = dynamic_cast<ConstantInt *>(val)->value_;
  return IntVal > 0 && __builtin_popcount(IntVal) == 1;
}

// 取模和除法需要考察正负性，下面是对模2^30和除2^30的汇编代码：
/*
div(int):
        sraiw   a5,a0,31
        srliw   a5,a5,2
        addw    a0,a5,a0
        srai    a0,a0,30
        ret
rem1(int):
        sraiw   a5,a0,31
        srliw   a5,a5,2
        addw    a0,a5,a0
        slli    a0,a0,34
        srli    a0,a0,34
        subw    a0,a0,a5
        ret
*/
void InstructionSubstitution::execute() {
  for (auto foo : m->function_list_)
    for (auto bb : foo->basic_blocks_) {
      auto curList = bb->instr_list_;
      for (auto instr : curList) {
        if (instr->is_mul() && (checkMulConst(instr->get_operand(0)) ||
            checkMulConst(instr->get_operand(1))))
          SolveMul(instr, bb);
        // if (instr->is_div() && checkDivRemConst(instr->get_operand(1)))
        //   SolveDiv(instr, bb);
        // if (instr->is_rem() && checkDivRemConst(instr->get_operand(1)))
        //   SolveRem(instr, bb);
      }
    }
}

// 本步骤不做常量折叠
// 乘法理论上可以优化popcount为2的，但是因为插入指令有问题因而暂时未开放
void InstructionSubstitution::SolveMul(Instruction *instr, BasicBlock *bb) {
  auto v1 = dynamic_cast<ConstantInt *>(instr->get_operand(0)),
       v2 = dynamic_cast<ConstantInt *>(instr->get_operand(1));
  if (v2 == nullptr) {
    std::swap(v1, v2);
    std::swap(instr->operands_[0], instr->operands_[1]);
  }
  int valC = v2->value_;
  // 乘以0在常量折叠处做
  if (!valC)
    return;
  int pos1 = __builtin_ctz(valC);
  // valC ^= 1 << pos1;
  // Value *op1 = instr->get_operand(0);
  // if (valC) {
  //   int pos2 = __builtin_ctz(valC);
  //   auto newOP1 = instr->operands_[0];
  //   if (pos1) {
  //     auto instr1 = new BinaryInst(m->int32_ty_, Instruction::Shl, op1,
  //                                 new ConstantInt(m->int32_ty_, pos1), bb);
  //     bb->add_instruction_before_inst(instr1, instr); 
  //     newOP1 = static_cast<Value *>(instr1);
  //   }
  //   auto instr2 = new BinaryInst(m->int32_ty_, Instruction::Shl, op1,
  //                                new ConstantInt(m->int32_ty_, pos2), bb);
  //   bb->add_instruction_before_inst(instr2, instr);
  //   instr->op_id_ = Instruction::Add;
  //   instr->operands_[0] = newOP1;
  //   instr->operands_[1] = static_cast<Value *>(instr2);
  // }
  // // 2 ^ k
  // else {
  instr->op_id_ = Instruction::Shl;
  instr->operands_[1] = new ConstantInt(m->int32_ty_, pos1);
  // }
}

void InstructionSubstitution::SolveDiv(Instruction *instr, BasicBlock *bb) {
  instr->op_id_ = Instruction::AShr;
  int v2 = dynamic_cast<ConstantInt *>(instr->get_operand(1))->value_;
  instr->operands_[1] = new ConstantInt(m->int32_ty_, __builtin_ctz(v2));
}

void InstructionSubstitution::SolveRem(Instruction *instr, BasicBlock *bb) {
  instr->op_id_ = Instruction::And;
  int v2 = dynamic_cast<ConstantInt *>(instr->get_operand(1))->value_;
  instr->operands_[1] = new ConstantInt(m->int32_ty_, v2 - 1u);
}