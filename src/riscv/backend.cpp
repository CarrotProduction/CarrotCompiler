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
      regAlloca->findReg(binaryInstr, rbb, nullptr, 1, 0), rbb);
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
    auto regPos =
        regAlloca->findReg(storeInstr->operands_[1], rbb, nullptr, 0, 0);
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
  // 如果是指针类型，则是RISCV中lw指令
  if (loadInstr->operands_[0]->type_->tid_ == Type::TypeID::PointerTyID) {
    auto curType = static_cast<PointerType *>(loadInstr->operands_[0]->type_);
    std::vector<RiscvInstr *> ans;
    auto regPos =
        regAlloca->findReg(static_cast<Value *>(loadInstr), rbb, nullptr, 1, 0);
    ans.push_back(new LoadRiscvInst(
        curType->contained_, regPos,
        regAlloca->findMem(loadInstr->operands_[0], rbb, nullptr, false), rbb));
    ans.push_back(new StoreRiscvInst(
        curType->contained_, regPos,
        regAlloca->findMem(static_cast<Value *>(loadInstr)), rbb));
    return ans;
  }
  // 否则是mov。其实可能不存在这一类
  std::vector<RiscvInstr *> ans;
  auto regPos = regAlloca->findReg(loadInstr->operands_[0], rbb, nullptr, 1);
  ans.push_back(new MoveRiscvInst(
      regAlloca->findReg(static_cast<Value *>(loadInstr), rbb), regPos, rbb));
  ans.push_back(new StoreRiscvInst(
      static_cast<Value *>(loadInstr)->type_, regPos,
      regAlloca->findMem(static_cast<Value *>(loadInstr)), rbb));
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
      regAlloca->findReg(icmpInstr, rbb, nullptr, 1, 0));
  rbb->addInstrBack(instr);
  if (inv) {
    auto instr_reg = regAlloca->findReg(icmpInstr, rbb, nullptr, 1, 0);
    rbb->addInstrBack(new BinaryRiscvInst(RiscvInstr::XORI, instr_reg,
                                          new RiscvConst(1), instr_reg, rbb));
  }
  return instr;
}

// FCMP处理上和ICMP类似，但是在最后生成语句的时候是输出两句
FCmpRiscvInstr *RiscvBuilder::createFCMPInstr(RegAlloca *regAlloca,
                                              FCmpInst *fcmpInstr,
                                              BranchInst *brInstr,
                                              RiscvBasicBlock *rbb) {
  FCmpRiscvInstr *instr = new FCmpRiscvInstr(
      fcmpInstr->fcmp_op_,
      regAlloca->findReg(fcmpInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(fcmpInstr->operands_[1], rbb, nullptr, 1),
      regAlloca->findNonuse(new Type(Type::IntegerTyID), rbb, nullptr),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
      rbb);
  return instr;
}

// 无条件跳转指令：JMP -> J
JumpRiscvInstr *RiscvBuilder::createJumpInstr(RegAlloca *regAlloca,
                                              BranchInst *brInstr,
                                              RiscvBasicBlock *rbb) {
  JumpRiscvInstr *instr = new JumpRiscvInstr(
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[0])),
      rbb);
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
                                              RiscvBasicBlock *rbb) {
  RiscvOperand *reg_to_save = nullptr;

  // If ret i32 %4
  if (returnInstr->num_ops_ > 0) {
    auto &operand = returnInstr->operands_[0];
    if (operand->type_->tid_ == Type::TypeID::IntegerTyID)
      reg_to_save = regAlloca->findSpecificReg(operand, "a0", rbb);
    else if (operand->type_->tid_ == Type::TypeID::FloatTyID)
      reg_to_save = regAlloca->findSpecificReg(operand, "fa0", rbb);
    auto instr = regAlloca->writeback(reg_to_save, rbb);
    rbb->addInstrAfter(
        new MoveRiscvInst(reg_to_save, regAlloca->findReg(operand, rbb, instr),
                          rbb),
        instr);
  }

  return new ReturnRiscvInst(rbb);
}

SiToFpRiscvInstr *RiscvBuilder::createSiToFpInstr(RegAlloca *regAlloca,
                                                  SiToFpInst *sitofpInstr,
                                                  RiscvBasicBlock *rbb) {
  return new SiToFpRiscvInstr(
      regAlloca->findReg(sitofpInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(static_cast<Value *>(sitofpInstr), rbb, nullptr, 1),
      rbb);
}

FpToSiRiscvInstr *RiscvBuilder::createFptoSiInstr(RegAlloca *regAlloca,
                                                  FpToSiInst *fptosiInstr,
                                                  RiscvBasicBlock *rbb) {
  return new FpToSiRiscvInstr(
      regAlloca->findReg(fptosiInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->findReg(static_cast<Value *>(fptosiInstr), rbb, nullptr, 1),
      rbb);
}

// 固定采用x30作为偏移量，x31作为乘法的LI指令地址
std::vector<RiscvInstr *>
RiscvBuilder::solveGetElementPtr(RegAlloca *regAlloca, GetElementPtrInst *instr,
                                 RiscvBasicBlock *rbb) {
  Value *op0 = instr->get_operand(0);
  RiscvOperand *dest = regAlloca->findReg(instr, rbb, nullptr, 0, 0);
  std::vector<RiscvInstr *> ans;
  bool isConst = 1; // 能否用确定的形如 -12(sp)访问
  int finalOffset = 0;
  if (dynamic_cast<GlobalVariable *>(op0) != nullptr) {
    // 全局变量：使用la指令取基础地址
    isConst = 0;
    ans.push_back(new LoadAddressRiscvInstr(dest, op0->name_, rbb));
  } else if (auto oi = dynamic_cast<Instruction *>(op0)) {
    // 获取指针指向的地址
    int varOffset = 0;

    ans.push_back(new MoveRiscvInst(
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
      RiscvOperand *mulTempReg = new RiscvIntReg(NamefindReg("t6"));
      ans.push_back(new MoveRiscvInst(mulTempReg, curTypeSize, rbb));
      ans.push_back(new BinaryRiscvInst(
          RiscvInstr::InstrType::MUL, regAlloca->findReg(opi, rbb, nullptr, 1),
          mulTempReg, mulTempReg, rbb));
      ans.push_back(new BinaryRiscvInst(RiscvInstr::InstrType::ADD, mulTempReg,
                                        dest, dest, rbb));
    }
  }
  // if (totalOffset > 0)
  ans.push_back(new BinaryRiscvInst(RiscvInstr::InstrType::ADDI, dest,
                                    new RiscvConst(totalOffset), dest, rbb));
  ans.push_back(
      new StoreRiscvInst(instr->type_, dest, regAlloca->findMem(instr), rbb));
  return ans;
}

RiscvBasicBlock *RiscvBuilder::transferRiscvBasicBlock(BasicBlock *bb,
                                                       RiscvFunction *foo) {
  int translationCount = 0;
  RiscvBasicBlock *rbb = createRiscvBasicBlock(bb);
  Instruction *forward = nullptr; // 前置指令，用于icmp、fcmp和branch指令合并
  for (Instruction *instr : bb->instr_list_) {
    switch (instr->op_id_) {
    case Instruction::Ret:
      // 在翻译过程中先指ret，恢复寄存器等操作在第二遍扫描的时候再插入
      rbb->addInstrBack(this->createRetInstr(
          foo->regAlloca, static_cast<ReturnInst *>(instr), rbb));
      break;
    // 分支指令
    case Instruction::Br:
      if (forward == nullptr) {
        rbb->addInstrBack(this->createJumpInstr(
            foo->regAlloca, static_cast<BranchInst *>(instr), rbb));
      } else if (forward->op_id_ == Instruction::FCmp) {
        rbb->addInstrBack(this->createFCMPInstr(
            foo->regAlloca, static_cast<FCmpInst *>(forward),
            static_cast<BranchInst *>(instr), rbb));
      } else if (forward->op_id_ == Instruction::ICmp) {
        rbb->addInstrBack(this->createICMPInstr(
            foo->regAlloca, static_cast<ICmpInst *>(forward),
            static_cast<BranchInst *>(instr), rbb));
      } else {
        assert(0);
      }
      forward = nullptr;
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
      foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::FNeg:
      rbb->addInstrBack(this->createUnaryInstr(
          foo->regAlloca, static_cast<UnaryInst *>(instr), rbb));
      foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
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
      std::vector<RiscvInstr *> instrSet = this->solveGetElementPtr(
          foo->regAlloca, static_cast<GetElementPtrInst *>(instr), rbb);
      for (auto *x : instrSet)
        rbb->addInstrBack(x);
      // Writeback inside solveGetElementPtr().
      break;
    }
    case Instruction::FPtoSI:
      rbb->addInstrBack(this->createFptoSiInstr(
          foo->regAlloca, static_cast<FpToSiInst *>(instr), rbb));
      foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::SItoFP:
      rbb->addInstrBack(this->createSiToFpInstr(
          foo->regAlloca, static_cast<SiToFpInst *>(instr), rbb));
      foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      break;
    case Instruction::Load: {
      auto instrSet = this->createLoadInstr(
          foo->regAlloca, static_cast<LoadInst *>(instr), rbb);
      for (auto x : instrSet)
        rbb->addInstrBack(x);
      foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
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
      foo->regAlloca->writeback(static_cast<Value *>(instr), rbb);
      forward = instr;
      break;
    case Instruction::FCmp:
      forward = instr;
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
      // 如果存在返回值，则要将返回值的寄存器地址告知caller的regalloca
      if (curInstr->type_->tid_ == Type::TypeID::IntegerTyID ||
          curInstr->type_->tid_ == Type::TypeID::PointerTyID)
        foo->regAlloca->setPositionReg(static_cast<Value *>(instr),
                                       new RiscvIntReg(NamefindReg("a0")), rbb);
      else if (curInstr->type_->tid_ == Type::TypeID::FloatTyID)
        foo->regAlloca->setPositionReg(static_cast<Value *>(instr),
                                       new RiscvIntReg(NamefindReg("fa0")),
                                       rbb);

      // 根据函数调用约定，按需传递参数。

      int sp_shift_for_paras = 0;
      int sp_shift_for_regs =
          VARIABLE_ALIGN_BYTE; // 保护寄存器使用的额外栈帧大小
      int paraShift = 0;

      int intRegCount = 0, floatRegCount = 0;

      // 计算存储参数需要的额外栈帧大小
      for (int i = 0; i < curInstr->operands_.size() - 1; i++) {
        sp_shift_for_paras += calcTypeSize(curInstr->operands_[i]->type_);
      }

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
        // 如果寄存器耗尽，则将函数参数使用store指令存入指定的内存中。
        if (name.empty()) {
          rbb->addInstrBack(new StoreRiscvInst(
              operand->type_,
              foo->regAlloca->findSpecificReg(operand, "t1", rbb),
              new RiscvIntPhiReg("sp", foo->querySP() - sp_shift_for_regs -
                                           sp_shift_for_paras + paraShift),
              rbb));
        } else {
          foo->regAlloca->findSpecificReg(operand, name, rbb, nullptr);
        }
        paraShift += calcTypeSize(operand->type_);
      }
      // 调整到新栈空间
      rbb->addInstrBack(new BinaryRiscvInst(
          RiscvInstr::ADDI, static_cast<RiscvOperand *>(getRegOperand("sp")),
          static_cast<RiscvOperand *>(new RiscvConst(
              foo->querySP() - sp_shift_for_regs - sp_shift_for_paras)),
          static_cast<RiscvOperand *>(getRegOperand("sp")), rbb));
      // ra的保护由caller去做
      rbb->addInstrBack(new StoreRiscvInst(
          new Type(Type::TypeID::PointerTyID),
          new RiscvIntReg(NamefindReg("ra")),
          new RiscvIntPhiReg("sp", sp_shift_for_paras), rbb));
      // 第二步：进行函数调用。
      rbb->addInstrBack(this->createCallInstr(foo->regAlloca, curInstr, rbb));
      // 第三步：caller恢复栈帧，清除所有的函数参数
      // 首先恢复ra
      rbb->addInstrBack(
          new LoadRiscvInst(new Type(Type::TypeID::PointerTyID),
                            new RiscvIntReg(NamefindReg("ra")),
                            new RiscvIntPhiReg("sp", sp_shift_for_paras), rbb));
      rbb->addInstrBack(new BinaryRiscvInst(
          RiscvInstr::ADDI, static_cast<RiscvOperand *>(getRegOperand("sp")),
          static_cast<RiscvOperand *>(new RiscvConst(
              -foo->querySP() + sp_shift_for_regs + sp_shift_for_paras)),
          static_cast<RiscvOperand *>(getRegOperand("sp")), rbb));
      // At last, save return value (a0) to target value.
      if (curInstr->type_->tid_ != curInstr->type_->VoidTyID) {
        rbb->addInstrBack(
            new StoreRiscvInst(new IntegerType(32), getRegOperand("a0"),
                               foo->regAlloca->findMem(curInstr), rbb));
        foo->regAlloca->writeback(static_cast<Value *>(curInstr), rbb);
      }
      break;
    }
    }
    // std::cout << "FINISH TRANSFER " << ++translationCount << "Codes\n";
  }
  return rbb;
}

// 总控程序
std::string RiscvBuilder::buildRISCV(Module *m) {
  this->rm = new RiscvModule();
  std::string data = ".section .data\n";
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
                dynamic_cast<ConstantFloat *>(Operand)->print();
            while (valString.length() < 10)
              valString += "0";
            data += curFloatName + "\t.word\t" + valString.substr(0, 10) + "\n";
            rfoo->regAlloca->setPosition(Operand,
                                         new RiscvFloatPhiReg(curFloatName, 0));
          }
    // 首先检查所有的alloca指令，加入一个基本块进行寄存器保护以及栈空间分配
    RiscvBasicBlock *initBlock = createRiscvBasicBlock();
    std::map<Value *, int> haveAllocated;
    int IntParaCount = 0, FloatParaCount = 0, ParaShift = 0;

    // 函数起始栈帧就从-4开始，在riscvFunction提前修订base的值
    auto storeOnStack = [&](Value *val) {
      if (val == nullptr)
        return;
      if (haveAllocated.count(val))
        return;
      // 几种特殊类型，不需要分栈空间
      if (dynamic_cast<Function *>(val) != nullptr)
        return;
      if (dynamic_cast<BasicBlock *>(val) != nullptr)
        return;
      if (val->type_->tid_ == Type::LabelTyID || val->type_->tid_ == Type::VoidTyID)
        return;
      // 注意：函数参数不用分配，而是直接指定！
      // 这里设定是：v开头的是局部变量，arg开头的是函数寄存器变量
      // 无名常量
      if (val->name_.empty())
        return;
      // 全局变量不用给他保存栈上地址，它本身就有对应的内存地址，直接忽略
      if (dynamic_cast<GlobalVariable *>(val) != nullptr) {
        auto curType = (val)->type_;
        while (1) {
          if (curType->tid_ == Type::TypeID::ArrayTyID)
            curType = static_cast<ArrayType *>(curType)->contained_;
          else if (curType->tid_ == Type::TypeID::PointerTyID)
            curType = static_cast<PointerType *>(curType)->contained_;
          else
            break;
        }
        if (curType->tid_ != Type::TypeID::FloatTyID)
          rfoo->regAlloca->setPosition(val,
                                       new RiscvIntPhiReg(val->name_, 0, 1));
        else
          rfoo->regAlloca->setPosition(
              val, new RiscvFloatPhiReg((val)->name_, 0, 1));
        return;
      }
      // 除了全局变量之外的参数
      if (dynamic_cast<Argument *>(val) != nullptr) {
        // 不用额外分配空间
        // 整型参数
        if (val->type_->tid_ == Type::TypeID::IntegerTyID ||
            val->type_->tid_ == Type::TypeID::PointerTyID) {
          // Pointer type's size is set to 8 byte.
          if (IntParaCount < 8)
            rfoo->regAlloca->setPositionReg(
                val, getRegOperand("a" + std::to_string(IntParaCount)));
          rfoo->regAlloca->setPosition(
              val, new RiscvIntPhiReg(NamefindReg("sp"), ParaShift));
          IntParaCount++;
          if (val->type_->tid_ == Type::TypeID::PointerTyID)
            ParaShift += 4;
        }
        // 浮点参数
        else {
          assert(val->type_->tid_ == Type::TypeID::FloatTyID);
          // 寄存器有
          if (FloatParaCount < 8) {
            rfoo->regAlloca->setPositionReg(
                val, getRegOperand("fa" + std::to_string(FloatParaCount)));
          }
          rfoo->regAlloca->setPosition(
              val, new RiscvFloatPhiReg(NamefindReg("sp"), ParaShift));
          FloatParaCount++;
        }
        ParaShift += 4;
      }
      // 函数内变量
      else {
        int curSP = rfoo->querySP();
        RiscvOperand *stackPos = static_cast<RiscvOperand *>(
            new RiscvIntPhiReg(NamefindReg("sp"), curSP));
        rfoo->regAlloca->setPosition(static_cast<Value *>(val), stackPos);
        rfoo->addTempVar(stackPos);
      }
      haveAllocated[val] = 1;
    };

    // 关联函数参数、寄存器与内存
    for (Value *arg : foo->arguments_)
      storeOnStack(arg);
    for (BasicBlock *bb : foo->basic_blocks_)
      for (Instruction *instr : bb->instr_list_)
        if (instr->op_id_ != Instruction::OpID::PHI &&
            instr->op_id_ != Instruction::OpID::ZExt &&
            instr->op_id_ != Instruction::OpID::Alloca) {
          // 所有的函数局部变量都要压入栈
          storeOnStack(static_cast<Value *>(instr));
          for (auto *val : instr->operands_)
            storeOnStack(static_cast<Value *>(val));
        }
    for (BasicBlock *bb : foo->basic_blocks_)
      for (Instruction *instr : bb->instr_list_)
        if (instr->op_id_ == Instruction::OpID::Alloca) {
          // 分配指针，并且将指针地址也同步保存
          auto curInstr = static_cast<AllocaInst *>(instr);
          int curTypeSize = calcTypeSize(curInstr->alloca_ty_);
          rfoo->storeArray(curTypeSize >> 2);
          int curSP = rfoo->querySP();
          RiscvOperand *ptrPos = new RiscvIntPhiReg(NamefindReg("sp"), curSP);
          rfoo->regAlloca->setPosition(static_cast<Value *>(instr), ptrPos);
          rfoo->regAlloca->setPointerPos(static_cast<Value *>(instr), ptrPos);
        }
    // Temporarily protect all registers realted to arguments.
    for (auto arg : foo->arguments_) {
      auto reg = rfoo->regAlloca->getPositionReg(arg);
      if (reg != nullptr) {
        initBlock->addInstrBack(new StoreRiscvInst(
            arg->type_, reg, rfoo->regAlloca->findMem(arg), initBlock));
      }
    }

    // 添加备用的初始化基本块
    rfoo->addBlock(initBlock);

    for (BasicBlock *bb : foo->basic_blocks_)
      rfoo->addBlock(this->transferRiscvBasicBlock(bb, rfoo));
    // 分配完成寄存器后，考虑将需要保护的寄存器进行保护
    std::vector<RiscvOperand *> savedRegister = rfoo->regAlloca->savedRegister;
    // 对于每一个要保存的寄存器，可以类似于allocaInstr的处理方式
    int curSP = rfoo->querySP();
    for (RiscvOperand *op : savedRegister)
      rfoo->addTempVar(op);
    // 整个栈帧如下图所示：
    /*
    +-----------+
    |    a7     |
    +-----------+
    |    a6     |
    +-----------+
    |    ...    |
    +-----------+
    |    a0     |
    +-----------+ <-
    caller和callee分界线。新函数栈帧从这里开始向下扩展，以此为基准 |
    分配变量。新函数栈帧从这里开始为0，向上为正，向下为负
    +------------+
    | 新局部变量 |
    +------------+
    | 保护寄存器 |
    +------------+
    不需要为sp保存寄存器，只需要时刻维护即可
    */
    // 然后再考虑把这些寄存器进行保护生成对应的代码
    initBlock->addInstrBack(new PushRiscvInst(savedRegister, initBlock, curSP));
    rfoo->ChangeBlock(initBlock, 0);
    // 第二遍扫描，处理所有的return语句之前的恢复现场等操作
    // 由于是倒过来压栈，因而现在要反序
    reverse(savedRegister.begin(), savedRegister.end());
    for (RiscvBasicBlock *rbb : rfoo->blk) {
      for (RiscvInstr *instr : rbb->instruction)
        // 对于return语句的返回操作进行处理
        if (typeid(*instr) == typeid(RiscvInstr::RET))
          rbb->addInstrBefore(new PopRiscvInst(savedRegister, rbb, curSP),
                              instr);
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