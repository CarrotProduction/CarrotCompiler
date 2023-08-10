#include "SimplifyJump.h"

void SimplifyJump::execute() {
  for (auto foo : m->function_list_)
    if (foo->basic_blocks_.empty()) {
      deleteUnReachableBlock(foo);
      mergePreBlock(foo);
      deleteUselessPhi(foo);
      deleteUselessJump(foo);
    }
}

void SimplifyJump::deleteUselessBlock(Function *foo,
                                      std::vector<BasicBlock *> &uselessBlock) {
  for (auto bb : uselessBlock)
    foo->remove_bb(bb);
}

bool SimplifyJump::checkUselessJump(BasicBlock *bb) {
  auto JumpPos = bb->get_terminator()->get_operand(0);
  for (auto preBB : bb->pre_bbs_) {
    auto br = preBB->get_terminator();
    if (br->operands_.size() == 1)
      continue;
    auto trueBB = br->get_operand(1);
    auto falseBB = br->get_operand(2);
    if (trueBB == JumpPos || falseBB == JumpPos)
      return false;
  }
  return true;
}

void SimplifyJump::deleteUnReachableBlock(Function *foo) {
  std::vector<BasicBlock *> uselessBlock;
  for (int i = 2; i < foo->basic_blocks_.size(); i++) {
    auto curbb = foo->basic_blocks_[i];
    if (curbb->pre_bbs_.empty()) {
      uselessBlock.push_back(curbb);
      // 发现无用块后需要提前进行phi合流处理
      for (auto use : curbb->use_list_) {
        auto instr = dynamic_cast<PhiInst *>(use.val_);
        if (instr != nullptr)
          instr->remove_operands(use.arg_no_ - 1, use.arg_no_);
      }
    }
  }
}

void SimplifyJump::mergePreBlock(Function *foo) {
  std::vector<BasicBlock *> uselessBlock;
  for (int i = 2; i < foo->basic_blocks_.size(); i++) {
    auto bb = foo->basic_blocks_[i];
    if (bb->pre_bbs_.size() == 1) {
      auto preBlock = *bb->pre_bbs_.begin();
      auto preBr = preBlock->get_terminator();
      if (preBlock->succ_bbs_.size() != 1)
        continue;
      preBlock->delete_instr(preBr);
      for (auto instr : bb->instr_list_) {
        preBlock->add_instruction(instr);
        bb->remove_instr(instr);
      }
      preBlock->remove_succ_basic_block(bb);
      for (auto suc : bb->succ_bbs_) {
        preBlock->add_succ_basic_block(suc);
        suc->remove_pre_basic_block(bb);
        suc->add_pre_basic_block(preBlock);
      }
      bb->replace_all_use_with(preBlock);
      uselessBlock.push_back(bb);
    }
  }
  deleteUselessBlock(foo, uselessBlock);
}

void SimplifyJump::deleteUselessPhi(Function *foo) {
  for (auto bb : foo->basic_blocks_)
    if (bb->pre_bbs_.size() == 1)
      for (auto instr : bb->instr_list_)
        if (instr->is_phi()) {
          instr->replace_all_use_with(instr->get_operand(0));
          bb->delete_instr(instr);
        }
}

void SimplifyJump::deleteUselessJump(Function *foo) {
  std::vector<BasicBlock *> uselessBlock;
  for (int i = 2; i < foo->basic_blocks_.size(); i++) {
    BasicBlock *curbb = foo->basic_blocks_[i];
    if (curbb->instr_list_.size() != 1)
      continue;
    auto branchInstr = curbb->get_terminator();
    if (branchInstr->operands_.size() != 1 || branchInstr->is_ret())
      continue;
    if (!checkUselessJump(curbb))
      continue;
    uselessBlock.push_back(curbb);
    auto JumpTarget = dynamic_cast<BasicBlock *>(branchInstr->get_operand(0));
    for (auto instr : JumpTarget->instr_list_)
      if (instr->is_phi()) {
        for (int i = 1; i < instr->operands_.size(); i += 2) {
          if (instr->get_operand(i) == curbb) {
            auto val = instr->get_operand(i - 1);
            instr->remove_operands(i - 1, i);
            for (auto preBB : curbb->pre_bbs_) {
              instr->add_operand(val);
              instr->add_operand(preBB);
            }
            break;
          }
        }
      }
    curbb->replace_all_use_with(JumpTarget);
    for (auto preBB : curbb->pre_bbs_) {
      preBB->add_succ_basic_block(JumpTarget);
      JumpTarget->add_pre_basic_block(preBB);
    }
  }
  deleteUselessBlock(foo, uselessBlock);
}