#include "CombineInstr.h"
#include <unordered_map>

void CombineInstr::execute() {
  for (auto foo : m->function_list_)
    if (!foo->basic_blocks_.empty())
      for (BasicBlock *bb : foo->basic_blocks_)
        checkBlock(bb);
}

void CombineInstr::checkBlock(BasicBlock *bb) {
  bool change = true;
  while (change) {
    change = false;
    for (auto instr : bb->instr_list_) {
      if (instr->op_id_ != Instruction::Add &&
          instr->op_id_ != Instruction::Sub)
        continue;
      if (instr->use_list_.size() != 1 || instr->use_list_.back().arg_no_ != 0)
        continue;
      Instruction *nextInstr =
          dynamic_cast<Instruction *>(instr->use_list_.back().val_);
      if (nextInstr == nullptr || instr->op_id_ != nextInstr->op_id_ ||
          instr->parent_ != nextInstr->parent_)
        continue;
      std::unordered_map<Value *, unsigned int> Optime;
      Instruction *StartInstr = instr, *EndInstr = nullptr;
      Instruction *invalidStart = nullptr, *invalidEnd = nullptr; // 无效指令
      bool isRepeat = false;
      Value *dupOp = nullptr, *Candi0 = instr->get_operand(0),
            *Candi1 = instr->get_operand(1);
      Optime[Candi0]++;
      Optime[Candi1]++;
      Optime[nextInstr->get_operand(1)]++;
      // 迭代过程
      instr = nextInstr;
      while (instr->use_list_.size() == 1 &&
             instr->use_list_.back().arg_no_ == 0) {
        nextInstr = dynamic_cast<Instruction *>(instr->use_list_.back().val_);
        if (nextInstr == nullptr || instr->op_id_ != nextInstr->op_id_ ||
            instr->parent_ != nextInstr->parent_)
          break;
        if (!isRepeat) {
          if (Optime.find(nextInstr->get_operand(1)) != Optime.end())
            isRepeat = true;
          EndInstr = instr;
          invalidStart = nextInstr;
        } else if (Optime.find(nextInstr->get_operand(1)) == Optime.end())
          break;
        Optime[nextInstr->get_operand(1)]++;
        instr = nextInstr;
      }
      invalidEnd = instr;
      // 尝试合并：1相同，很多0；0相同，很多1都可以合并
      unsigned int dupTime = 0;
      if (Optime[Candi1] == 1 && Optime[Candi0] > 1) {
        dupOp = Candi0;
        dupTime = Optime[Candi0];
      } else if (Optime[Candi0] == 1 && Optime[Candi1] > 1) {
        dupOp = Candi1;
        dupTime = Optime[Candi1];
      } else
        continue;
      for (auto p : Optime) {
        if (p.second == 1) {
          if (p.first != dupOp) {
            dupTime = 0;
            break;
          }
        } else {
          if (dupTime != p.second) {
            dupTime = 0;
            break;
          }
        }
      }
      if (!dupTime)
        continue;
      ConstantInt *dupTimeConst =
          new ConstantInt(instr->parent_->parent_->parent_->int32_ty_, dupTime);
      Instruction *toMulInst = new BinaryInst(
          bb->parent_->parent_->int32_ty_, Instruction::Mul,
          static_cast<Value *>(EndInstr), dupTimeConst, bb, true);
      toMulInst->name_ = invalidStart->name_;
      bb->add_instruction_before_inst(toMulInst, invalidStart);
      invalidEnd->replace_all_use_with(toMulInst);
      for (Instruction *ins = invalidStart; ins != nextInstr;) {
        bb->delete_instr(ins);
        ins = dynamic_cast<Instruction *>(ins->use_list_.back().val_);
      }
      change = true;
      break;
    }
  }
}