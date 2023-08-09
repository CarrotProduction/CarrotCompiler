#include "ConstSpread.h"

ConstantInt *ConstSpread::CalcInt(Instruction::OpID op, ConstantInt *v1,
                                  ConstantInt *v2) {
  int a = v1->value_, b = v2->value_;
  switch (op) {
  case Instruction::Add:
    return new ConstantInt(m->int32_ty_, a + b);
  case Instruction::Sub:
    return new ConstantInt(m->int32_ty_, a - b);
  case Instruction::Mul:
    return new ConstantInt(m->int32_ty_, a * b);
  case Instruction::SDiv:
    return new ConstantInt(m->int32_ty_, a / b);
  case Instruction::SRem:
    return new ConstantInt(m->int32_ty_, a % b);
  case Instruction::Shl:
    return new ConstantInt(m->int32_ty_, a << b);
  case Instruction::LShr:
    return new ConstantInt(m->int32_ty_, (unsigned)a >> b);
  case Instruction::AShr:
    return new ConstantInt(m->int32_ty_, a >> b);
  case Instruction::And:
    return new ConstantInt(m->int32_ty_, a & b);
  case Instruction::Or:
    return new ConstantInt(m->int32_ty_, a | b);
  case Instruction::Xor:
    return new ConstantInt(m->int32_ty_, a ^ b);
  default:
    return nullptr;
  }
}

ConstantFloat *ConstSpread::CalcFloat(Instruction::OpID op, ConstantFloat *v1,
                                      ConstantFloat *v2) {
  float a = v1->value_, b = v2->value_;
  switch (op) {
  case Instruction::FAdd:
    return new ConstantFloat(m->float32_ty_, a + b);
  case Instruction::FSub:
    return new ConstantFloat(m->float32_ty_, a - b);
  case Instruction::FMul:
    return new ConstantFloat(m->float32_ty_, a * b);
  case Instruction::FDiv:
    return new ConstantFloat(m->float32_ty_, a / b);
  default:
    return nullptr;
  }
}

ConstantInt *ConstSpread::CalcICMP(ICmpInst::ICmpOp op, ConstantInt *v1,
                                   ConstantInt *v2) {
  int lhs = v1->value_;
  int rhs = v2->value_;
  switch (op) {
  case ICmpInst::ICMP_EQ:
    return new ConstantInt(m->int1_ty_, lhs == rhs);
  case ICmpInst::ICMP_NE:
    return new ConstantInt(m->int1_ty_, lhs != rhs);
  case ICmpInst::ICMP_SGT:
    return new ConstantInt(m->int1_ty_, lhs > rhs);
  case ICmpInst::ICMP_SGE:
    return new ConstantInt(m->int1_ty_, lhs >= rhs);
  case ICmpInst::ICMP_SLE:
    return new ConstantInt(m->int1_ty_, lhs <= rhs);
  case ICmpInst::ICMP_SLT:
    return new ConstantInt(m->int1_ty_, lhs < rhs);
  case ICmpInst::ICMP_UGE:
    return new ConstantInt(m->int1_ty_, (unsigned)lhs >= (unsigned)rhs);
  case ICmpInst::ICMP_ULE:
    return new ConstantInt(m->int1_ty_, (unsigned)lhs <= (unsigned)rhs);
  case ICmpInst::ICMP_ULT:
    return new ConstantInt(m->int1_ty_, (unsigned)lhs < (unsigned)rhs);
  case ICmpInst::ICMP_UGT:
    return new ConstantInt(m->int1_ty_, (unsigned)lhs > (unsigned)rhs);
  default:
    return nullptr;
  }
}
ConstantInt *ConstSpread::CalcFCMP(FCmpInst::FCmpOp op, ConstantFloat *v1,
                                   ConstantFloat *v2) {
  float lhs = v1->value_;
  float rhs = v2->value_;
  switch (op) {
  case FCmpInst::FCMP_UEQ:
    return new ConstantInt(m->int1_ty_, lhs == rhs);
  case FCmpInst::FCMP_UNE:
    return new ConstantInt(m->int1_ty_, lhs != rhs);
  case FCmpInst::FCMP_UGT:
    return new ConstantInt(m->int1_ty_, lhs > rhs);
  case FCmpInst::FCMP_UGE:
    return new ConstantInt(m->int1_ty_, lhs >= rhs);
  case FCmpInst::FCMP_ULE:
    return new ConstantInt(m->int1_ty_, lhs <= rhs);
  case FCmpInst::FCMP_ULT:
    return new ConstantInt(m->int1_ty_, lhs < rhs);
  case FCmpInst::FCMP_FALSE:
    return new ConstantInt(m->int1_ty_, 0);
  case FCmpInst::FCMP_TRUE:
    return new ConstantInt(m->int1_ty_, 1);
  case FCmpInst::FCMP_OEQ:
    return new ConstantInt(m->int1_ty_, lhs == rhs);
  case FCmpInst::FCMP_ONE:
    return new ConstantInt(m->int1_ty_, lhs != rhs);
  case FCmpInst::FCMP_OGE:
    return new ConstantInt(m->int1_ty_, lhs >= rhs);
  case FCmpInst::FCMP_OGT:
    return new ConstantInt(m->int1_ty_, lhs > rhs);
  case FCmpInst::FCMP_OLE:
    return new ConstantInt(m->int1_ty_, lhs <= rhs);
  case FCmpInst::FCMP_OLT:
    return new ConstantInt(m->int1_ty_, lhs < rhs);
  default:
    return nullptr;
  }
}

void ConstSpread::execute() {
  assert(m != nullptr);
  for (Function *foo : m->function_list_) {
    if (foo->basic_blocks_.size()) {
      bool change = true;
      while (change) {
        change = false;
        change |= SpreadingConst(foo);
        change |= BranchProcess(foo);
        DeleteUnusedBB(foo);
      }
    }
  }
}

bool ConstSpread::SpreadingConst(Function *func) {
  uselessInstr.clear();
  for (auto bb : func->basic_blocks_) {
    ConstIntMap.clear();
    ConstFloatMap.clear();
    for (auto instr : bb->instr_list_) {
      ConstantInt *testConstInta = nullptr, *testConstIntb = nullptr;
      ConstantFloat *testConstFloata = nullptr, *testConstFloatb = nullptr;
      switch (instr->op_id_) {
      case Instruction::Add:
      case Instruction::Sub:
      case Instruction::Mul:
      case Instruction::SDiv:
      case Instruction::UDiv:
      case Instruction::SRem:
      case Instruction::URem:
      case Instruction::And:
      case Instruction::Or:
      case Instruction::Xor:
      case Instruction::Shl:
      case Instruction::AShr:
      case Instruction::LShr:
        testConstInta = dynamic_cast<ConstantInt *>(instr->get_operand(0));
        testConstIntb = dynamic_cast<ConstantInt *>(instr->get_operand(1));
        if (testConstInta && testConstIntb) {
          auto intRes =
              this->CalcInt(instr->op_id_, testConstInta, testConstIntb);
          if (intRes) {
            instr->replace_all_use_with(intRes);
            uselessInstr[instr] = bb;
          }
        }
        break;
      case Instruction::ICmp:
        testConstInta = dynamic_cast<ConstantInt *>(instr->get_operand(0));
        testConstIntb = dynamic_cast<ConstantInt *>(instr->get_operand(1));
        if (testConstInta && testConstIntb) {
          auto res = this->CalcICMP(dynamic_cast<ICmpInst *>(instr)->icmp_op_,
                                    testConstInta, testConstIntb);
          if (res) {
            instr->replace_all_use_with(res);
            uselessInstr[instr] = bb;
          }
        }
        break;
      case Instruction::FCmp:
        testConstFloata = dynamic_cast<ConstantFloat *>(instr->get_operand(0));
        testConstFloatb = dynamic_cast<ConstantFloat *>(instr->get_operand(1));
        if (testConstFloata && testConstFloatb) {
          auto res = this->CalcFCMP(dynamic_cast<FCmpInst *>(instr)->fcmp_op_,
                                    testConstFloata, testConstFloatb);
          if (res) {
            instr->replace_all_use_with(res);
            uselessInstr[instr] = bb;
          }
        }
        break;
      case Instruction::FAdd:
      case Instruction::FSub:
      case Instruction::FMul:
      case Instruction::FDiv:
        testConstFloata = dynamic_cast<ConstantFloat *>(instr->get_operand(0));
        testConstFloatb = dynamic_cast<ConstantFloat *>(instr->get_operand(1));
        if (testConstFloata && testConstFloatb) {
          auto floaRes =
              this->CalcFloat(instr->op_id_, testConstFloata, testConstFloatb);
          if (floaRes) {
            instr->replace_all_use_with(floaRes);
            uselessInstr[instr] = bb;
          }
        }
        break;
      case Instruction::FNeg:
        testConstFloata = dynamic_cast<ConstantFloat *>(instr->get_operand(0));
        if (testConstFloata) {
          instr->replace_all_use_with(
              new ConstantFloat(m->float32_ty_, -testConstFloata->value_));
          uselessInstr[instr] = bb;
        }
        break;
      case Instruction::FPtoSI:
        testConstFloata = dynamic_cast<ConstantFloat *>(instr->get_operand(0));
        if (testConstFloata) {
          instr->replace_all_use_with(
              new ConstantInt(m->int32_ty_, testConstFloata->value_));
          uselessInstr[instr] = bb;
        }
        break;
      case Instruction::SItoFP:
        testConstInta = dynamic_cast<ConstantInt *>(instr->get_operand(0));
        if (testConstInta) {
          instr->replace_all_use_with(
              new ConstantFloat(m->float32_ty_, testConstInta->value_));
          uselessInstr[instr] = bb;
        }
        break;
      case Instruction::ZExt:
        testConstInta = dynamic_cast<ConstantInt *>(instr->get_operand(0));
        if (testConstInta) {
          instr->replace_all_use_with(
              new ConstantInt(m->int32_ty_, testConstInta->value_));
          uselessInstr[instr] = bb;
        }
        break;
      case Instruction::Call:
        ConstIntMap.clear();
        ConstFloatMap.clear();
      case Instruction::Load: {
        auto globalVar = dynamic_cast<GlobalVariable *>(instr->get_operand(0));
        if (globalVar) {
          auto iterInt = ConstIntMap.find(globalVar);
          auto iterFloat = ConstFloatMap.find(globalVar);
          if (iterInt != ConstIntMap.end()) {
            instr->replace_all_use_with(iterInt->second);
            uselessInstr[instr] = bb;
          } else if (iterFloat != ConstFloatMap.end()) {
            instr->replace_all_use_with(iterFloat->second);
            uselessInstr[instr] = bb;
          }
        } else if (dynamic_cast<AllocaInst *>(instr->get_operand(0))) {
          auto pos = dynamic_cast<AllocaInst *>(instr->get_operand(0));
          if (pos->alloca_ty_->tid_ == Type::IntegerTyID) {
            auto iterInt = ConstIntMap.find(pos);
            if (iterInt != ConstIntMap.end()) {
              instr->replace_all_use_with(iterInt->second);
              uselessInstr[instr] = bb;
            }
          } else if (pos->alloca_ty_->tid_ == Type::FloatTyID) {
            auto iterFloat = ConstFloatMap.find(pos);
            if (iterFloat != ConstFloatMap.end()) {
              instr->replace_all_use_with(iterFloat->second);
              uselessInstr[instr] = bb;
            }
          }
        }
      } break;
      case Instruction::Store: {
        // std::cout << "EVER STORE\n";
        auto storePos = instr->get_operand(1);
        auto storeValInt = dynamic_cast<ConstantInt *>(instr->get_operand(0));
        auto storeValFloat =
            dynamic_cast<ConstantFloat *>(instr->get_operand(0));
        if (storeValInt) {
          auto iter1 = ConstIntMap.find(storePos);
          if (iter1 != ConstIntMap.end()) {
            if (iter1->second->value_ == storeValInt->value_)
              uselessInstr[instr] = bb;
            else
              iter1->second = storeValInt;
          } else
            ConstIntMap[storePos] = storeValInt;
        } else if (storeValFloat) {
          auto iter = ConstFloatMap.find(storePos);
          if (iter != ConstFloatMap.end()) {
            if (iter->second->value_ == storeValInt->value_)
              uselessInstr[instr] = bb;
            else
              iter->second = storeValFloat;
          } else
            ConstFloatMap[storePos] = storeValFloat;
        } else {
          // 非常量存储，则该地址数据不再是常量
          auto iterInt = ConstIntMap.find(storePos);
          auto iterFloat = ConstFloatMap.find(storePos);
          if (iterInt != ConstIntMap.end())
            ConstIntMap.erase(iterInt);
          if (iterFloat != ConstFloatMap.end())
            ConstFloatMap.erase(iterFloat);
        }
      } break;
      default:
        break;
      }
    }
  }
  if (!uselessInstr.empty()) {
    for (auto [instr, bb] : uselessInstr)
      bb->delete_instr(instr);
    return true;
  }
  return false;
}

bool ConstSpread::BranchProcess(Function *func) {
  bool change = false;
  for (auto bb : func->basic_blocks_) {
    auto br = bb->get_terminator();
    if (!br)
      continue;
    if (br->op_id_ == Instruction::Br &&
        dynamic_cast<BranchInst *>(br)->num_ops_ == 3) {
      auto cond = dynamic_cast<ConstantInt *>(br->get_operand(0));
      auto truebb = br->get_operand(1);
      auto falsebb = br->get_operand(2);
      if (!cond)
        continue;
      change = true;
      if (cond->value_ == 0) {
        bb->delete_instr(br);
        for (auto succ_bb : bb->succ_bbs_) {
          succ_bb->remove_pre_basic_block(bb);
          if (succ_bb != falsebb) {
            SolvePhi(bb, succ_bb);
          }
        }
        bb->succ_bbs_.clear();
        new BranchInst(dynamic_cast<BasicBlock *>(falsebb), bb);
      } else {
        bb->delete_instr(br);
        for (auto succ_bb : bb->succ_bbs_) {
          succ_bb->remove_pre_basic_block(bb);
          if (succ_bb != truebb) {
            SolvePhi(bb, succ_bb);
          }
        }
        bb->succ_bbs_.clear();
        new BranchInst(dynamic_cast<BasicBlock *>(truebb), bb);
      }
    }
  }
  return change;
}