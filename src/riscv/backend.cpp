#include "backend.h"
#include <cassert>

void RiscvBuilder::initializeRegisterFile() {
  // todo：分配寄存器堆，初始化寄存器堆各项参数
  // assert(false);
}

// 进行标号
// 未知指令：FNeg
// 注意：IR中因为没有addi和add和浮点的区别，该步操作由build操作进行修正
const std::map<Instruction::OpID, RiscvInstr::InstrType> toRiscvOp = {
    {Instruction::OpID::Add, RiscvInstr::InstrType::ADD},
    {Instruction::OpID::Sub, RiscvInstr::InstrType::SUB},
    {Instruction::OpID::Mul, RiscvInstr::InstrType::MUL},
    {Instruction::OpID::SDiv, RiscvInstr::InstrType::DIV},
    {Instruction::OpID::SRem, RiscvInstr::InstrType::REM},
    {Instruction::OpID::FAdd, RiscvInstr::InstrType::FADD},
    {Instruction::OpID::FSub, RiscvInstr::InstrType::FSUB},
    {Instruction::OpID::FMul, RiscvInstr::InstrType::FMUL},
    {Instruction::OpID::FDiv, RiscvInstr::InstrType::FDIV},
    {Instruction::OpID::Ret, RiscvInstr::InstrType::RET},
    {Instruction::OpID::ICmp, RiscvInstr::InstrType::ICMP},
    {Instruction::OpID::FCmp, RiscvInstr::InstrType::FCMP},
    {Instruction::OpID::Call, RiscvInstr::InstrType::CALL},
    {Instruction::OpID::SItoFP, RiscvInstr::InstrType::SITOFP},
    {Instruction::OpID::FPtoSI, RiscvInstr::InstrType::FPTOSI},
    {Instruction::OpID::Or, RiscvInstr::InstrType::OR},
    {Instruction::OpID::And, RiscvInstr::InstrType::AND},
    {Instruction::OpID::Shl, RiscvInstr::InstrType::SHL},
    {Instruction::OpID::LShr, RiscvInstr::InstrType::LSHR},
    {Instruction::OpID::AShr, RiscvInstr::InstrType::ASHR},
    {Instruction::OpID::Load, RiscvInstr::InstrType::LW},
    {Instruction::OpID::Store, RiscvInstr::InstrType::SW},
};

int LabelCount = 0;
std::map<BasicBlock *, RiscvBasicBlock *> rbbLabel;
std::map<Function *, RiscvFunction *> functionLabel;
std::string toLabel(int ind) { return ".L" + std::to_string(ind); }

RiscvBasicBlock *createRiscvBasicBlock(BasicBlock *bb) {
  if (bb == nullptr) {
    LabelCount++;
    return new RiscvBasicBlock(toLabel(LabelCount), LabelCount);
  }
  if (rbbLabel.count(bb))
    return rbbLabel[bb];
  LabelCount++;
  auto cur = new RiscvBasicBlock(toLabel(LabelCount), LabelCount);
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

BinaryRiscvInst *RiscvBuilder::createBinaryInstr(RegAlloca *regAlloca,
                                                 BinaryInst *binaryInstr,
                                                 RiscvBasicBlock *rbb) {
  auto id = toRiscvOp.at(binaryInstr->op_id_);
  // 立即数处理

  // If both operands are imm value, caculate the result directly and save to
  // binaryInstr value.
  if (binaryInstr->operands_[0]->is_constant() &&
      binaryInstr->operands_[1]->is_constant() &&
      dynamic_cast<ConstantInt *>(binaryInstr->operands_[0]) != nullptr) {
    int value[] = {
        static_cast<ConstantInt *>(binaryInstr->operands_[0])->value_,
        static_cast<ConstantInt *>(binaryInstr->operands_[1])->value_};
    int value_result;
    switch (binaryInstr->op_id_) {
    case Instruction::OpID::Add:
      value_result = value[0] + value[1];
      break;
    case Instruction::OpID::Sub:
      value_result = value[0] - value[1];
      break;
    case Instruction::OpID::Mul:
      value_result = value[0] * value[1];
      break;
    case Instruction::OpID::SDiv:
      value_result = value[0] / value[1];
      break;
    case Instruction::OpID::SRem:
      value_result = value[0] % value[1];
      break;
    default:
      std::cerr << "[Fatal Error] Binary instruction immediate caculation not "
                   "implemented."
                << std::endl;
      std::terminate();
    }
    rbb->addInstrBack(
        new MoveRiscvInst(regAlloca->findReg(binaryInstr, rbb, nullptr, 0, 0),
                          value_result, rbb));
    return nullptr;
  }
  BinaryRiscvInst *instr = new BinaryRiscvInst(
      id, regAlloca->findReg(binaryInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(binaryInstr->operands_[1], rbb, nullptr, 1),
      regAlloca->findReg(binaryInstr, rbb, nullptr, 1, 0), rbb, true);
  return instr;
}

UnaryRiscvInst *RiscvBuilder::createUnaryInstr(RegAlloca *regAlloca,
                                               UnaryInst *unaryInstr,
                                               RiscvBasicBlock *rbb) {
  UnaryRiscvInst *instr = new UnaryRiscvInst(
      toRiscvOp.at(unaryInstr->op_id_),
      regAlloca->findReg(unaryInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(unaryInstr, rbb, nullptr, 1, 0), rbb);
  return instr;
}

// IR中的Store对应到RISCV为MOV指令或浮点MOV指令或LI指令或真正的store指令
std::vector<RiscvInstr *> RiscvBuilder::createStoreInstr(RegAlloca *regAlloca,
                                                         StoreInst *storeInstr,
                                                         RiscvBasicBlock *rbb) {
  auto testConstInt = dynamic_cast<ConstantInt *>(storeInstr->operands_[0]);
  if (testConstInt != nullptr) {
    // 整数部分可以直接li指令
    std::vector<RiscvInstr *> ans;
    auto regPos = getRegOperand("t0");
    ans.push_back(
        new MoveRiscvInst(regPos, new RiscvConst(testConstInt->value_), rbb));
    // 指针类型找ptr
    if (storeInstr->operands_[1]->type_->tid_ == Type::TypeID::PointerTyID)
      ans.push_back(new StoreRiscvInst(
          storeInstr->operands_[0]->type_, regPos,
          regAlloca->findMem(storeInstr->operands_[1], rbb, nullptr, 0), rbb));
    else
      ans.push_back(new StoreRiscvInst(
          storeInstr->operands_[0]->type_, regPos,
          regAlloca->findMem(storeInstr->operands_[1], rbb, nullptr, 0), rbb));
    return ans;
  }
  // 真正的store：第二操作数为一个指针类型
  if (storeInstr->operands_[1]->type_->tid_ == Type::TypeID::PointerTyID) {
    auto curType = static_cast<PointerType *>(storeInstr->operands_[1]->type_);

    // Alignment check.
    // if (calcTypeSize(curType->contained_) > 4) {
    //   auto mem =
    //       regAlloca->findMem(storeInstr->operands_[1], rbb, nullptr, false);
    //   if (static_cast<RiscvIntPhiReg *>(mem)->shift_ & 7) {
    //     std::cerr << "[Error] Alignment error." << std::endl;
    //     std::terminate();
    //   }
    // }

    StoreRiscvInst *instr = new StoreRiscvInst(
        curType->contained_,
        regAlloca->findReg(storeInstr->operands_[0], rbb, nullptr, 1),
        regAlloca->findMem(storeInstr->operands_[1], rbb, nullptr, 0), rbb);
    return {instr};
  }
  // 下面为整型或浮点的mov
  // 浮点部分需要提前写入内存中，然后等效于直接mov
  // TODO:先把浮点常数以全局变量形式存入内存中，再直接fmv
  std::vector<RiscvInstr *> ans;
  auto regPos = regAlloca->findReg(storeInstr->operands_[0], rbb, nullptr, 1);
  ans.push_back(new MoveRiscvInst(
      regAlloca->findReg(storeInstr->operands_[1], rbb), regPos, rbb));
  ans.push_back(new StoreRiscvInst(storeInstr->operands_[0]->type_, regPos,
                                   regAlloca->findMem(storeInstr->operands_[0]),
                                   rbb));
  return ans;
}

// Load 指令仅一个参数！它本身就是一个value
std::vector<RiscvInstr *> RiscvBuilder::createLoadInstr(RegAlloca *regAlloca,
                                                        LoadInst *loadInstr,
                                                        RiscvBasicBlock *rbb) {
  assert(loadInstr->operands_[0]->type_->tid_ == Type::TypeID::PointerTyID);
  auto curType = static_cast<PointerType *>(loadInstr->operands_[0]->type_);
  // if (calcTypeSize(curType->contained_) > 4) {
  //   auto mem = regAlloca->findMem(loadInstr->operands_[0], rbb, nullptr, false);
  //   if (static_cast<RiscvIntPhiReg *>(mem)->shift_ & 7) {
  //     std::cerr << "[Error] Alignment error." << std::endl;
  //     std::terminate();
  //   }
  // }
  std::vector<RiscvInstr *> ans;
  auto regPos =
      regAlloca->findReg(static_cast<Value *>(loadInstr), rbb, nullptr, 1, 0);
  ans.push_back(new LoadRiscvInst(
      curType->contained_, regPos,
      regAlloca->findMem(loadInstr->operands_[0], rbb, nullptr, false), rbb));
  return ans;
}

ICmpRiscvInstr *RiscvBuilder::createICMPInstr(RegAlloca *regAlloca,
                                              ICmpInst *icmpInstr,
                                              BranchInst *brInstr,
                                              RiscvBasicBlock *rbb) {
  ICmpRiscvInstr *instr = new ICmpRiscvInstr(
      icmpInstr->icmp_op_,
      regAlloca->findReg(icmpInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(icmpInstr->operands_[1], rbb, nullptr, 1),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
      rbb);
  return instr;
}

ICmpRiscvInstr *RiscvBuilder::createICMPSInstr(RegAlloca *regAlloca,
                                               ICmpInst *icmpInstr,
                                               RiscvBasicBlock *rbb) {
  bool swap = ICmpSRiscvInstr::ICmpOpSName.count(icmpInstr->icmp_op_) == 0;
  if (swap) {
    std::swap(icmpInstr->operands_[0], icmpInstr->operands_[1]);
    icmpInstr->icmp_op_ =
        ICmpRiscvInstr::ICmpOpEquiv.find(icmpInstr->icmp_op_)->second;
  }
  bool inv = false;
  switch (icmpInstr->icmp_op_) {
  case ICmpInst::ICMP_SGE:
  case ICmpInst::ICMP_SLE:
  case ICmpInst::ICMP_UGE:
  case ICmpInst::ICMP_ULE:
    inv = true;
  default:
    break;
  }
  ICmpSRiscvInstr *instr = new ICmpSRiscvInstr(
      icmpInstr->icmp_op_,
      regAlloca->findReg(icmpInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(icmpInstr->operands_[1], rbb, nullptr, 1),
      regAlloca->findReg(icmpInstr, rbb, nullptr, 1, 0), rbb);
  rbb->addInstrBack(instr);
  if (inv) {
    auto instr_reg = regAlloca->findReg(icmpInstr, rbb, nullptr, 1, 0);
    rbb->addInstrBack(new BinaryRiscvInst(RiscvInstr::XORI, instr_reg,
                                          new RiscvConst(1), instr_reg, rbb));
  }
  return instr;
}

RiscvInstr *RiscvBuilder::createFCMPInstr(RegAlloca *regAlloca,
                                          FCmpInst *fcmpInstr,
                                          RiscvBasicBlock *rbb) {
  // Deal with always true
  if (fcmpInstr->fcmp_op_ == fcmpInstr->FCMP_TRUE ||
      fcmpInstr->fcmp_op_ == fcmpInstr->FCMP_FALSE) {
    auto instr =
        new MoveRiscvInst(regAlloca->findReg(fcmpInstr, rbb, nullptr, 1, 0),
                          fcmpInstr->fcmp_op_ == fcmpInstr->FCMP_TRUE, rbb);
    rbb->addInstrBack(instr);
    return instr;
  }
  bool swap = FCmpRiscvInstr::FCmpOpName.count(fcmpInstr->fcmp_op_) == 0;
  if (swap) {
    std::swap(fcmpInstr->operands_[0], fcmpInstr->operands_[1]);
    fcmpInstr->fcmp_op_ =
        FCmpRiscvInstr::FCmpOpEquiv.find(fcmpInstr->fcmp_op_)->second;
  }
  bool inv = false;
  bool inv_classify = false;
  switch (fcmpInstr->fcmp_op_) {
  case FCmpInst::FCMP_ONE:
  case FCmpInst::FCMP_UNE:
    inv = true;
  default:
    break;
  }
  switch (fcmpInstr->fcmp_op_) {
  case FCmpInst::FCMP_OEQ:
  case FCmpInst::FCMP_OGT:
  case FCmpInst::FCMP_OGE:
  case FCmpInst::FCMP_OLT:
  case FCmpInst::FCMP_OLE:
  case FCmpInst::FCMP_ONE:
  case FCmpInst::FCMP_ORD:
    inv_classify = true;
  default:
    break;
  }

  if (inv_classify) {
    std::cerr << "[Warning] Not implemented FCLASS yet.\n";
  }
  FCmpRiscvInstr *instr = new FCmpRiscvInstr(
      fcmpInstr->fcmp_op_,
      regAlloca->findReg(fcmpInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(fcmpInstr->operands_[1], rbb, nullptr, 1),
      regAlloca->findReg(fcmpInstr, rbb, nullptr, 1, 0), rbb);
  rbb->addInstrBack(instr);
  if (inv) {
    auto instr_reg = regAlloca->findReg(fcmpInstr, rbb, nullptr, 1, 0);
    rbb->addInstrBack(new BinaryRiscvInst(RiscvInstr::XORI, instr_reg,
                                          new RiscvConst(1), instr_reg, rbb));
    return instr;
  }
  return instr;
}

CallRiscvInst *RiscvBuilder::createCallInstr(RegAlloca *regAlloca,
                                             CallInst *callInstr,
                                             RiscvBasicBlock *rbb) {
  // push 指令需要寄存器
  int argnum = callInstr->operands_.size() - 1;
  // 涉及从Function 到RISCV function转换问题（第一个参数）
  CallRiscvInst *instr =
      new CallRiscvInst(createRiscvFunction(static_cast<Function *>(
                            callInstr->operands_[argnum])),
                        rbb);
  return instr;
}

// 注意：return语句本身并不负责返回值的传递，该语句由storeRet函数实现
ReturnRiscvInst *RiscvBuilder::createRetInstr(RegAlloca *regAlloca,
                                              ReturnInst *returnInstr,
                                              RiscvBasicBlock *rbb,
                                              RiscvFunction *rfoo) {
  RiscvOperand *reg_to_save = nullptr;

  // If ret i32 %4
  if (returnInstr->num_ops_ > 0) {
    // 写返回值到 a0/fa0 中
    auto operand = returnInstr->operands_[0];
    if (operand->type_->tid_ == Type::TypeID::IntegerTyID)
      reg_to_save = regAlloca->findSpecificReg(operand, "a0", rbb);
    else if (operand->type_->tid_ == Type::TypeID::FloatTyID)
      reg_to_save = regAlloca->findSpecificReg(operand, "fa0", rbb);
    // auto instr = regAlloca->writeback(reg_to_save, rbb);

    rbb->addInstrBack(new MoveRiscvInst(
        reg_to_save, regAlloca->findReg(operand, rbb, nullptr), rbb));
  }

  return new ReturnRiscvInst(rbb);
}

BranchRiscvInstr *RiscvBuilder::createBrInstr(RegAlloca *regAlloca,
                                              BranchInst *brInstr,
                                              RiscvBasicBlock *rbb) {

  BranchRiscvInstr *instr;
  if (brInstr->num_ops_ == 1) {
    instr = new BranchRiscvInstr(
        nullptr, nullptr,
        createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[0])),
        rbb);
  } else {
    instr = new BranchRiscvInstr(
        regAlloca->findReg(brInstr->operands_[0], rbb, nullptr, 1),
        createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
        createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
        rbb);
  }
  return instr;
}

SiToFpRiscvInstr *RiscvBuilder::createSiToFpInstr(RegAlloca *regAlloca,
                                                  SiToFpInst *sitofpInstr,
                                                  RiscvBasicBlock *rbb) {
  return new SiToFpRiscvInstr(
      regAlloca->findReg(sitofpInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(static_cast<Value *>(sitofpInstr), rbb, nullptr, 1, 0),
      rbb);
}

FpToSiRiscvInstr *RiscvBuilder::createFptoSiInstr(RegAlloca *regAlloca,
                                                  FpToSiInst *fptosiInstr,
                                                  RiscvBasicBlock *rbb) {
  return new FpToSiRiscvInstr(
      regAlloca->findReg(fptosiInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(static_cast<Value *>(fptosiInstr), rbb, nullptr, 1, 0),
      rbb);
}

// 固定采用x30作为偏移量，x31作为乘法的LI指令地址
RiscvInstr *RiscvBuilder::solveGetElementPtr(RegAlloca *regAlloca,
                                             GetElementPtrInst *instr,
                                             RiscvBasicBlock *rbb) {
  Value *op0 = instr->get_operand(0);
  RiscvOperand *dest = getRegOperand("t2");
  bool isConst = 1; // 能否用确定的形如 -12(sp)访问
  int finalOffset = 0;
  if (dynamic_cast<GlobalVariable *>(op0) != nullptr) {
    // 全局变量：使用la指令取基础地址
    isConst = 0;
    rbb->addInstrBack(new LoadAddressRiscvInstr(dest, op0->name_, rbb));
  } else if (auto oi = dynamic_cast<Instruction *>(op0)) {
    // 获取指针指向的地址
    int varOffset = 0;

    rbb->addInstrBack(new MoveRiscvInst(
        dest, regAlloca->findReg(op0, rbb, nullptr, 1, 1), rbb));

    finalOffset += varOffset;
  }
  int curTypeSize = 0;
  unsigned int num_operands = instr->num_ops_;
  int indexVal, totalOffset = 0;
  Type *cur_type =
      static_cast<PointerType *>(instr->get_operand(0)->type_)->contained_;
  for (unsigned int i = 1; i <= num_operands - 1; i++) {
    if (i > 1)
      cur_type = static_cast<ArrayType *>(cur_type)->contained_;
    Value *opi = instr->get_operand(i);
    curTypeSize = calcTypeSize(cur_type);
    if (auto ci = dynamic_cast<ConstantInt *>(opi)) {
      indexVal = ci->value_;
      totalOffset += indexVal * curTypeSize;
    } else {
      // 存在变量参与偏移量计算
      isConst = 0;
      // 考虑目标数是int还是float
      RiscvOperand *mulTempReg = getRegOperand("t3");
      rbb->addInstrBack(new MoveRiscvInst(mulTempReg, curTypeSize, rbb));
      rbb->addInstrBack(new BinaryRiscvInst(
          RiscvInstr::InstrType::MUL, regAlloca->findReg(opi, rbb, nullptr, 1),
          mulTempReg, mulTempReg, rbb));
      rbb->addInstrBack(new BinaryRiscvInst(RiscvInstr::InstrType::ADD,
                                            mulTempReg, dest, dest, rbb));
    }
  }
  // if (totalOffset > 0)
  rbb->addInstrBack(new BinaryRiscvInst(RiscvInstr::InstrType::ADDI, dest,
                                        new RiscvConst(totalOffset), dest,
                                        rbb));
  rbb->addInstrBack(
      new StoreRiscvInst(instr->type_, dest, regAlloca->findMem(instr), rbb));
  return nullptr;
}

void RiscvBuilder::initRetInstr(RegAlloca *regAlloca, RiscvInstr *returnInstr,
                                RiscvBasicBlock *rbb, RiscvFunction *foo) {
  // 将被保护的寄存器还原
  // ! FP 必须被最后还原。
  int curSP = foo->querySP();
  auto reg_to_recover = regAlloca->savedRegister;
  auto reg_used = regAlloca->getUsedReg();
  reverse(reg_to_recover.begin(), reg_to_recover.end());
  for (auto reg : reg_to_recover)
    if (reg_used.find(reg) != reg_used.end()) {
      if (reg->getType() == reg->IntReg)
        rbb->addInstrBefore(new LoadRiscvInst(new Type(Type::PointerTyID), reg,
                                              new RiscvIntPhiReg("fp", curSP),
                                              rbb),
                            returnInstr);
      else
        rbb->addInstrBefore(new LoadRiscvInst(new Type(Type::FloatTyID), reg,
                                              new RiscvIntPhiReg("fp", curSP),
                                              rbb),
                            returnInstr);
      curSP += VARIABLE_ALIGN_BYTE;
    }

  // 还原 fp
  rbb->addInstrBefore(new LoadRiscvInst(new Type(Type::PointerTyID),
                                        getRegOperand("fp"),
                                        new RiscvIntPhiReg("fp", curSP), rbb),
                      returnInstr);

  // 释放栈帧
  rbb->addInstrBefore(new BinaryRiscvInst(RiscvInstr::ADDI, getRegOperand("sp"),
                                          new RiscvConst(-foo->querySP()),
                                          getRegOperand("sp"), rbb),
                      returnInstr);
}

RiscvBasicBlock *RiscvBuilder::transferRiscvBasicBlock(BasicBlock *bb,
                                                       RiscvFunction *foo) {
  int translationCount = 0;
  RiscvBasicBlock *rbb = createRiscvBasicBlock(bb);
  Instruction *forward = nullptr; // 前置指令，用于icmp、fcmp和branch指令合并
  bool brFound = false;
  for (Instruction *instr : bb->instr_list_) {
    switch (instr->op_id_) {
    case Instruction::Ret:
      // Before leaving basic block writeback all registers
      foo->regAlloca->writeback_all(rbb);
      brFound = true;
      // 在翻译过程中先指ret，恢复寄存器等操作在第二遍扫描的时候再插入
      rbb->addInstrBack(this->createRetInstr(
          foo->regAlloca, static_cast<ReturnInst *>(instr), rbb, foo));
      break;
    // 分支指令
    case Instruction::Br:
      // Before leaving basic block writeback all registers
      foo->regAlloca->writeback_all(rbb);
      brFound = true;

      rbb->addInstrBack(this->createBrInstr(
          foo->regAlloca, static_cast<BranchInst *>(instr), rbb));
      break;
    case Instruction::Add:
    case Instruction::Sub:
    case Instruction::Mul:
    case Instruction::SDiv:
    case Instruction::SRem:
    case Instruction::UDiv:
    case Instruction::URem:
    case Instruction::FAdd:
    case Instruction::FSub:
    case Instruction::FMul:
    case Instruction::FDiv:
    case Instruction::Shl:
    case Instruction::LShr:
    case Instruction::AShr:
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor:
      rbb->addInstrBack(this->createBinaryInstr(
          foo->regAlloca, static_cast<BinaryInst *>(instr), rbb));
      // foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::FNeg:
      rbb->addInstrBack(this->createUnaryInstr(
          foo->regAlloca, static_cast<UnaryInst *>(instr), rbb));
      // foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::PHI:
      break;
    // 直接删除的指令
    case Instruction::BitCast:
      break;
    case Instruction::ZExt:
      // 等价一条合流语句操作
      break;
    case Instruction::Alloca:
      break;
    case Instruction::GetElementPtr: {
      this->solveGetElementPtr(foo->regAlloca,
                               static_cast<GetElementPtrInst *>(instr), rbb);
      // Writeback inside solveGetElementPtr().
      break;
    }
    case Instruction::FPtoSI:
      rbb->addInstrBack(this->createFptoSiInstr(
          foo->regAlloca, static_cast<FpToSiInst *>(instr), rbb));
      // foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::SItoFP:
      rbb->addInstrBack(this->createSiToFpInstr(
          foo->regAlloca, static_cast<SiToFpInst *>(instr), rbb));
      // foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::Load: {
      auto instrSet = this->createLoadInstr(
          foo->regAlloca, static_cast<LoadInst *>(instr), rbb);
      for (auto x : instrSet)
        rbb->addInstrBack(x);
      // foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    }
    case Instruction::Store: {
      auto instrSet = this->createStoreInstr(
          foo->regAlloca, static_cast<StoreInst *>(instr), rbb);
      for (auto *x : instrSet)
        rbb->addInstrBack(x);
      // Store Instruction returns void value.
      break;
    }
    case Instruction::ICmp:
      createICMPSInstr(foo->regAlloca, static_cast<ICmpInst *>(instr), rbb);
      // foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::FCmp:
      createFCMPInstr(foo->regAlloca, static_cast<FCmpInst *>(instr), rbb);
      // foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::Call: {
      // 注意：该部分并未单独考虑系统函数！
      // 注意：区分float还是int调用是看寄存器分配部分实现
      // 说明：call函数部分本身需要进行栈寄存器调整，调整到0栈帧供新函数使用
      // 除此之外不在任何地方调整sp的值
      // 在call语句结束之后要手动恢复回原来的栈帧
      CallInst *curInstr = static_cast<CallInst *>(instr);
      RiscvFunction *calleeFoo = createRiscvFunction(
          static_cast<Function *>(curInstr->operands_.back()));

      // 根据函数调用约定，按需传递参数。

      int sp_shift_for_paras = 0;
      int sp_shift_alignment_padding = 0; // Align sp pointer to 16-byte
      int paraShift = 0;

      int intRegCount = 0, floatRegCount = 0;

      // 计算存储参数需要的额外栈帧大小
      for (int i = 0; i < curInstr->operands_.size() - 1; i++) {
        sp_shift_for_paras += VARIABLE_ALIGN_BYTE;
      }

      sp_shift_alignment_padding =
          16 - ((abs(foo->querySP()) + sp_shift_for_paras) & 15);
      sp_shift_for_paras += sp_shift_alignment_padding;

      // 为参数申请栈帧
      rbb->addInstrBack(new BinaryRiscvInst(
          BinaryRiscvInst::ADDI, getRegOperand("sp"),
          new RiscvConst(-sp_shift_for_paras), getRegOperand("sp"), rbb));

      // 将参数移动至寄存器与内存中
      for (int i = 0; i < curInstr->operands_.size() - 1; i++) {
        std::string name = "";
        auto operand = curInstr->operands_[i];
        if (operand->type_->tid_ != Type::FloatTyID) {
          if (intRegCount < 8)
            name = "a" + std::to_string(intRegCount);
          intRegCount++;
        } else if (operand->type_->tid_ == Type::FloatTyID) {
          if (floatRegCount < 8)
            name = "fa" + std::to_string(floatRegCount);
          floatRegCount++;
        }
        // 将额外的参数直接写入内存中
        if (name.empty()) {
          if (operand->type_->tid_ != Type::FloatTyID) {
            rbb->addInstrBack(new StoreRiscvInst(
                operand->type_,
                foo->regAlloca->findSpecificReg(operand, "t1", rbb),
                new RiscvIntPhiReg("sp", paraShift), rbb));
          } else {
            rbb->addInstrBack(new StoreRiscvInst(
                operand->type_,
                foo->regAlloca->findSpecificReg(operand, "ft1", rbb),
                new RiscvIntPhiReg("sp", paraShift), rbb));
          }
        } else {
          foo->regAlloca->findSpecificReg(operand, name, rbb, nullptr);
        }
        paraShift += VARIABLE_ALIGN_BYTE; // Add operand size lastly
      }

      // Call the function.
      rbb->addInstrBack(this->createCallInstr(foo->regAlloca, curInstr, rbb));

      // 为参数释放栈帧
      rbb->addInstrBack(new BinaryRiscvInst(
          BinaryRiscvInst::ADDI, getRegOperand("sp"),
          new RiscvConst(sp_shift_for_paras), getRegOperand("sp"), rbb));

      // At last, save return value (a0) to target value.
      if (curInstr->type_->tid_ != curInstr->type_->VoidTyID) {
        if (curInstr->type_->tid_ != curInstr->type_->FloatTyID) {
          rbb->addInstrBack(
              new StoreRiscvInst(new IntegerType(32), getRegOperand("a0"),
                                 foo->regAlloca->findMem(curInstr), rbb));
        } else {
          rbb->addInstrBack(new StoreRiscvInst(
              new Type(Type::FloatTyID), getRegOperand("fa0"),
              foo->regAlloca->findMem(curInstr), rbb));
        }
      }
      break;
    }
    }
    // std::cout << "FINISH TRANSFER " << ++translationCount << "Codes\n";
  }
  // If br instruction is not found, then leave basic block at the block's end.
  if (!brFound) {
    foo->regAlloca->writeback_all(rbb);
  }
  return rbb;
}

// 总控程序
std::string RiscvBuilder::buildRISCV(Module *m) {
  this->rm = new RiscvModule();
  std::string data = ".align 2\n.section .data\n"; // Add align attribute
  // 全局变量
  if (m->global_list_.size()) {
    for (GlobalVariable *gb : m->global_list_) {
      auto curType = static_cast<PointerType *>(gb->type_)->contained_;
      RiscvGlobalVariable *curGB = nullptr;
      Type *containedType = nullptr;
      switch (curType->tid_) {
      case Type::PointerTyID:
        assert(false);
        break;
      case Type::ArrayTyID:
        containedType = curType;
        while (1) {
          if (containedType->tid_ == Type::TypeID::ArrayTyID)
            containedType = static_cast<ArrayType *>(containedType)->contained_;
          else
            break;
        }
        if (containedType->tid_ == Type::IntegerTyID) {
          curGB = new RiscvGlobalVariable(RiscvOperand::IntImm, gb->name_,
                                          gb->is_const_, gb->init_val_,
                                          calcTypeSize(curType) / 4);
          rm->addGlobalVariable(curGB);
          data += curGB->print();
        } else if (containedType->tid_ == Type::FloatTyID) {
          curGB = new RiscvGlobalVariable(RiscvOperand::FloatImm, gb->name_,
                                          gb->is_const_, gb->init_val_,
                                          calcTypeSize(curType) / 4);
          rm->addGlobalVariable(curGB);
          data += curGB->print();
        }
        break;
      case Type::TypeID::IntegerTyID: {
        auto curGB =
            new RiscvGlobalVariable(RiscvOperand::OpTy::IntImm, gb->name_,
                                    gb->is_const_, gb->init_val_);
        assert(curGB != nullptr);
        rm->addGlobalVariable(curGB);
        data += curGB->print();
        break;
      }
      case Type::TypeID::FloatTyID: {
        auto curGB =
            new RiscvGlobalVariable(RiscvOperand::OpTy::FloatImm, gb->name_,
                                    gb->is_const_, gb->init_val_);
        rm->addGlobalVariable(curGB);
        data += curGB->print();
        break;
      }
      }
    }
  }
  // 浮点常量进入内存
  int ConstFloatCount = 0;
  std::string code = ".section .text\n";
  // 函数体
  // 预处理：首先合并所有的合流语句操作，然后在分配单元（storeOnStack）部分使用DSU合并
  for (Function *foo : m->function_list_) {
    auto rfoo = createRiscvFunction(foo);
    rm->addFunction(rfoo);
    if (rfoo->is_libfunc()) {
      auto *libFunc = createSyslibFunc(foo);
      if (libFunc != nullptr)
        code += libFunc->print();
      continue;
    }
    for (BasicBlock *bb : foo->basic_blocks_)
      for (Instruction *instr : bb->instr_list_)
        if (instr->op_id_ == Instruction::OpID::PHI) {
          for (auto *operand : instr->operands_)
            rfoo->regAlloca->DSU_for_Variable.merge(
                operand, static_cast<Value *>(instr));
        } else if (instr->op_id_ == Instruction::OpID::ZExt) {
          rfoo->regAlloca->DSU_for_Variable.merge(instr->operands_[0],
                                                  static_cast<Value *>(instr));
        } else if (instr->op_id_ == Instruction::OpID::BitCast) {
          // std::cerr << "[Debug] [DSU] Bitcast Instruction: Merge value "
          //           << static_cast<Value *>(instr)->print() << " to "
          //           << instr->operands_[0]->print() << " ." << std::endl;
          rfoo->regAlloca->DSU_for_Variable.merge(static_cast<Value *>(instr),
                                                  instr->operands_[0]);
        }
    // 将该函数内的浮点常量全部处理出来并告知寄存器分配单元
    for (BasicBlock *bb : foo->basic_blocks_)
      for (Instruction *instr : bb->instr_list_)
        for (auto *Operand : instr->operands_)
          // 找到浮点常数，存入内存，写入全局变量区
          if (dynamic_cast<ConstantFloat *>(Operand) != nullptr) {
            std::string curFloatName =
                "FloatConst" + std::to_string(ConstFloatCount);
            ConstFloatCount++;
            std::string valString =
                dynamic_cast<ConstantFloat *>(Operand)->print32();
            while (valString.length() < 10)
              valString += "0";
            data +=
                curFloatName + ":\n\t.word\t" + valString.substr(0, 10) + "\n";
            rfoo->regAlloca->setPosition(Operand,
                                         new RiscvFloatPhiReg(curFloatName, 0));
          }
    // 首先检查所有的alloca指令，加入一个基本块进行寄存器保护以及栈空间分配
    RiscvBasicBlock *initBlock = createRiscvBasicBlock();
    std::map<Value *, int> haveAllocated;
    int IntParaCount = 0, FloatParaCount = 0;
    int sp_shift_for_paras = 0;
    int paraShift = 0;

    rfoo->setSP(0); // set sp to 0 initially.

    // Lambda function to write value to stack frame.
    auto storeOnStack = [&](Value **val) {
      if (val == nullptr)
        return;
      assert(*val != nullptr);
      if (haveAllocated.count(*val))
        return;
      // 几种特殊类型，不需要分栈空间
      if (dynamic_cast<Function *>(*val) != nullptr)
        return;
      if (dynamic_cast<BasicBlock *>(*val) != nullptr)
        return;
      // 注意：函数参数不用分配，而是直接指定！
      // 这里设定是：v开头的是局部变量，arg开头的是函数寄存器变量
      // 无名常量
      if ((*val)->name_.empty())
        return;
      // 全局变量不用给他保存栈上地址，它本身就有对应的内存地址，直接忽略
      if (dynamic_cast<GlobalVariable *>(*val) != nullptr) {
        auto curType = (*val)->type_;
        while (1) {
          if (curType->tid_ == Type::TypeID::ArrayTyID)
            curType = static_cast<ArrayType *>(curType)->contained_;
          else if (curType->tid_ == Type::TypeID::PointerTyID)
            curType = static_cast<PointerType *>(curType)->contained_;
          else
            break;
        }
        if (curType->tid_ != Type::TypeID::FloatTyID)
          rfoo->regAlloca->setPosition(*val,
                                       new RiscvIntPhiReg((*val)->name_, 0, 1));
        else
          rfoo->regAlloca->setPosition(
              *val, new RiscvFloatPhiReg((*val)->name_, 0, 1));
        return;
      }
      // 除了全局变量之外的参数
      if (dynamic_cast<Argument *>(*val) != nullptr) {
        // 不用额外分配空间
        // 整型参数
        if ((*val)->type_->tid_ == Type::TypeID::IntegerTyID ||
            (*val)->type_->tid_ == Type::TypeID::PointerTyID) {
          // Pointer type's size is set to 8 byte.
          if (IntParaCount < 8)
            rfoo->regAlloca->setPositionReg(
                *val, getRegOperand("a" + std::to_string(IntParaCount)));
          rfoo->regAlloca->setPosition(
              *val, new RiscvIntPhiReg(NamefindReg("fp"), paraShift));
          IntParaCount++;
        }
        // 浮点参数
        else {
          assert((*val)->type_->tid_ == Type::TypeID::FloatTyID);
          // 寄存器有
          if (FloatParaCount < 8) {
            rfoo->regAlloca->setPositionReg(
                *val, getRegOperand("fa" + std::to_string(FloatParaCount)));
          }
          rfoo->regAlloca->setPosition(
              *val, new RiscvFloatPhiReg(NamefindReg("fp"), paraShift));
          FloatParaCount++;
        }
        paraShift += VARIABLE_ALIGN_BYTE;
      }
      // 函数内变量
      else {
        int curSP = rfoo->querySP();
        RiscvOperand *stackPos = static_cast<RiscvOperand *>(
            new RiscvIntPhiReg(NamefindReg("fp"), curSP - VARIABLE_ALIGN_BYTE));
        rfoo->regAlloca->setPosition(static_cast<Value *>(*val), stackPos);
        rfoo->addTempVar(stackPos);
      }
      haveAllocated[*val] = 1;
    };

    // 关联函数参数、寄存器与内存
    for (Value *arg : foo->arguments_)
      storeOnStack(&arg);

    for (BasicBlock *bb : foo->basic_blocks_)
      for (Instruction *instr : bb->instr_list_)
        if (instr->op_id_ != Instruction::OpID::PHI &&
            instr->op_id_ != Instruction::OpID::ZExt &&
            instr->op_id_ != Instruction::OpID::Alloca) {
          // 所有的函数局部变量都要压入栈
          Value *tempPtr = static_cast<Value *>(instr);
          storeOnStack(&tempPtr);
          for (auto *val : instr->operands_) {
            tempPtr = static_cast<Value *>(val);
            storeOnStack(&tempPtr);
          }
        }
    for (BasicBlock *bb : foo->basic_blocks_)
      for (Instruction *instr : bb->instr_list_)
        if (instr->op_id_ == Instruction::OpID::Alloca) {
          // 分配指针，并且将指针地址也同步保存
          auto curInstr = static_cast<AllocaInst *>(instr);
          int curTypeSize = calcTypeSize(curInstr->alloca_ty_);
          rfoo->storeArray(curTypeSize);
          int curSP = rfoo->querySP();
          RiscvOperand *ptrPos = new RiscvIntPhiReg(NamefindReg("fp"), curSP);
          rfoo->regAlloca->setPosition(static_cast<Value *>(instr), ptrPos);
          rfoo->regAlloca->setPointerPos(static_cast<Value *>(instr), ptrPos);
        }

    // 添加初始化基本块
    rfoo->addBlock(initBlock);
    // 翻译语句并计算被使用的寄存器
    for (BasicBlock *bb : foo->basic_blocks_)
      rfoo->addBlock(this->transferRiscvBasicBlock(bb, rfoo));
    rfoo->ChangeBlock(initBlock, 0);

    // 保护寄存器
    rfoo->shiftSP(-VARIABLE_ALIGN_BYTE);
    int fp_sp = rfoo->querySP(); // 为保护 fp 分配空间
    auto &reg_to_save = rfoo->regAlloca->savedRegister;
    auto reg_used = rfoo->regAlloca->getUsedReg();
    for (auto reg : reg_to_save)
      if (reg_used.find(reg) != reg_used.end()) {
        rfoo->shiftSP(-VARIABLE_ALIGN_BYTE);
        if (reg->getType() == reg->IntReg)
          initBlock->addInstrBack(new StoreRiscvInst(
              new Type(Type::PointerTyID), reg,
              new RiscvIntPhiReg(NamefindReg("fp"), rfoo->querySP()),
              initBlock));
        else
          initBlock->addInstrBack(new StoreRiscvInst(
              new Type(Type::FloatTyID), reg,
              new RiscvIntPhiReg(NamefindReg("fp"), rfoo->querySP()),
              initBlock));
      }

    // 分配整体的栈空间，并设置s0为原sp
    initBlock->addInstrFront(new BinaryRiscvInst(
        RiscvInstr::ADDI, getRegOperand("sp"), new RiscvConst(-rfoo->querySP()),
        getRegOperand("fp"),
        initBlock)); // 3: fp <- t0
    initBlock->addInstrFront(new StoreRiscvInst(
        new Type(Type::PointerTyID), getRegOperand("fp"),
        new RiscvIntPhiReg(NamefindReg("sp"), fp_sp - rfoo->querySP()),
        initBlock)); // 2: 保护 fp
    initBlock->addInstrFront(new BinaryRiscvInst(
        RiscvInstr::ADDI, getRegOperand("sp"), new RiscvConst(rfoo->querySP()),
        getRegOperand("sp"), initBlock)); // 1: 分配栈帧

    // 扫描所有的返回语句并插入寄存器还原等相关内容
    for (RiscvBasicBlock *rbb : rfoo->blk)
      for (RiscvInstr *rinstr : rbb->instruction)
        if (rinstr->type_ == rinstr->RET) {
          initRetInstr(rfoo->regAlloca, rinstr, rbb, rfoo);
          break;
        }

    code += rfoo->print();
  }
  return data + code;
}

/**
 * 辅助函数，计算一个类型的字节大小。
 * @param ty 将要计算的类型 `ty` 。
 * @return 返回类型的字节大小。
 */
int calcTypeSize(Type *ty) {
  int totalsize = 1;
  while (ty->tid_ == Type::ArrayTyID) {
    totalsize *= static_cast<ArrayType *>(ty)->num_elements_;
    ty = static_cast<ArrayType *>(ty)->contained_;
  }
  assert(ty->tid_ == Type::IntegerTyID || ty->tid_ == Type::FloatTyID ||
         ty->tid_ == Type::PointerTyID);
  if (ty->tid_ == Type::IntegerTyID || ty->tid_ == Type::FloatTyID)
    totalsize *= 4;
  else if (ty->tid_ == Type::PointerTyID)
    totalsize *= 8;
  return totalsize;
}