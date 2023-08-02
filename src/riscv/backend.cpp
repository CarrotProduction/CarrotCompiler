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
      binaryInstr->operands_[1]->is_constant()) {
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
          regAlloca->findPtr(storeInstr->operands_[1], rbb, nullptr), rbb));
    else
      ans.push_back(new StoreRiscvInst(
          storeInstr->operands_[0]->type_, regPos,
          regAlloca->findMem(storeInstr->operands_[1], rbb, nullptr), rbb));
    return ans;
  }
  // 真正的store：第二操作数为一个指针类型
  if (storeInstr->operands_[1]->type_->tid_ == Type::TypeID::PointerTyID) {
    auto curType = static_cast<PointerType *>(storeInstr->operands_[1]->type_);
    StoreRiscvInst *instr = new StoreRiscvInst(
        curType->contained_,
        regAlloca->findReg(storeInstr->operands_[0], rbb, nullptr, 1),
        regAlloca->findPtr(storeInstr->operands_[1], rbb, nullptr), rbb);
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
        regAlloca->findPtr(loadInstr->operands_[0], rbb, nullptr), rbb));
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
      regAlloca->findNonuse(rbb, nullptr),
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
  int argnum = callInstr->operands_.size() - 1, intRegCount = 0,
      floatRegCount = 0;
  for (int i = 0; i < argnum; i++) {
    std::string name = "";
    if (callInstr->operands_[i]->type_->tid_ == Type::IntegerTyID) {
      if (intRegCount < 8)
        name = "a" + std::to_string(intRegCount);
      intRegCount++;
    } else {
      if (floatRegCount < 8)
        name = "fa" + std::to_string(floatRegCount);
      floatRegCount++;
    }
    // 如果寄存器不超过数额，约定寄存器
    if (!name.empty()) {
      if (name.front() == 'a')
        regAlloca->setPositionReg(callInstr->operands_[i],
                                  new RiscvIntReg(NamefindReg(name)));
      else
        regAlloca->setPositionReg(callInstr->operands_[i],
                                  new RiscvFloatReg(NamefindReg(name)));
    }
    // 相对于子函数栈帧进行栈分配
    RiscvOperand *stackPos = static_cast<RiscvOperand *>(
        new RiscvIntPhiReg(NamefindReg("sp"), 4 * i));
    regAlloca->setPosition(callInstr->operands_[i], stackPos);
  }
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
  RiscvOperand *dest = regAlloca->findReg(instr, rbb);
  std::vector<RiscvInstr *> ans;
  bool isConst = 1; // 能否用确定的形如 -12(sp)访问
  if (dynamic_cast<GlobalVariable *>(op0) != nullptr) {
    // 全局变量：使用la指令取基础地址
    isConst = 0;
    ans.push_back(new LoadAddressRiscvInstr(dest, op0->name_, rbb));
  } else if (auto oi = dynamic_cast<Instruction *>(op0)) {
    // 指令是在栈上分配的临时地址，则dest=当前的sp+现在分配的变量的偏移地址
    int varOffset = 0;
    if (op0->type_->tid_ == Type::TypeID::FloatTyID)
      varOffset =
          static_cast<RiscvFloatPhiReg *>(regAlloca->findMem(op0))->shift_;
    else
      varOffset =
          static_cast<RiscvIntPhiReg *>(regAlloca->findMem(op0))->shift_;
    ans.push_back(new BinaryRiscvInst(RiscvInstr::InstrType::ADDI,
                                      getRegOperand("sp"),
                                      new RiscvConst(varOffset), dest, rbb));
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
      assert(opi->type_->tid_ == Type::IntegerTyID);
      RiscvOperand *mulTempReg = new RiscvIntReg(NamefindReg("t6"));
      // 先把常量塞进x31寄存器，然后做乘法dest=dest*x31
      ans.push_back(new MoveRiscvInst(mulTempReg, curTypeSize, rbb));
      ans.push_back(new BinaryRiscvInst(RiscvInstr::InstrType::MUL, dest,
                                        mulTempReg, dest, rbb));
    }
  }
  if (totalOffset > 0)
    ans.push_back(new BinaryRiscvInst(RiscvInstr::InstrType::ADDI, dest,
                                      new RiscvConst(totalOffset), dest, rbb));
  regAlloca->setPointerPos(static_cast<Value *>(instr), dest);
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
      break;
    case Instruction::FNeg:
      rbb->addInstrBack(this->createUnaryInstr(
          foo->regAlloca, static_cast<UnaryInst *>(instr), rbb));
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
      break;
    }
    case Instruction::FPtoSI:
      rbb->addInstrBack(this->createFptoSiInstr(
          foo->regAlloca, static_cast<FpToSiInst *>(instr), rbb));
      break;
    case Instruction::SItoFP:
      rbb->addInstrBack(this->createSiToFpInstr(
          foo->regAlloca, static_cast<SiToFpInst *>(instr), rbb));
      break;
    case Instruction::Load: {
      auto instrSet = this->createLoadInstr(
          foo->regAlloca, static_cast<LoadInst *>(instr), rbb);
      for (auto x : instrSet)
        rbb->addInstrBack(x);
      break;
    }
    case Instruction::Store: {
      auto instrSet = this->createStoreInstr(
          foo->regAlloca, static_cast<StoreInst *>(instr), rbb);
      for (auto *x : instrSet)
        rbb->addInstrBack(x);
      break;
    }
    case Instruction::ICmp:
      createICMPSInstr(foo->regAlloca, static_cast<ICmpInst *>(instr), rbb);
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
                                       new RiscvIntReg(NamefindReg("a0")));
      else if (curInstr->type_->tid_ == Type::TypeID::FloatTyID)
        foo->regAlloca->setPositionReg(static_cast<Value *>(instr),
                                       new RiscvIntReg(NamefindReg("fa0")));
      // 第一步：函数参数压栈，对于函数f(a0,a1,...,a7)，a7在高地址（24(sp)），a0在低地址（0(sp)）
      // 注意：该步中并未约定一定要求是函数寄存器a0-a7

      // 这里问题很大！函数调用参数可能是常数
      std::vector<RiscvOperand *> parameters;
      int intRegCount = 0, floatRegCount = 0;
      for (int i = 0; i < curInstr->operands_.size() - 1; i++) {
        std::string name = "";
        if (curInstr->operands_[i]->type_->tid_ == Type::IntegerTyID ||
            curInstr->operands_[i]->type_->tid_ == Type::PointerTyID) {
          if (intRegCount < 8)
            name = "a" + std::to_string(intRegCount);
          intRegCount++;
        } else if (curInstr->operands_[i]->type_->tid_ == Type::FloatTyID) {
          if (floatRegCount < 8)
            name = "fa" + std::to_string(floatRegCount);
          floatRegCount++;
        }
        // 如果寄存器超过数额，由caller放入栈中
        if (name.empty())
          parameters.push_back(
              foo->regAlloca->findReg(curInstr->operands_[i], rbb, nullptr, 1));
        // 否则由caller的regAlloca，放到指定寄存器a0-a7 fa0-fa7。
        // 此处为了防止运算过程中可能没地方放，因而还是进行了栈空间的分配
        else {
          parameters.push_back(foo->regAlloca->findSpecificReg(
              curInstr->operands_[i], name, rbb));
        }
      }
      // 存放参数造成的额外开销。每个函数参数都放到了栈上，前8+8个参数也在寄存器中有备份
      int SPShift = parameters.size() * 4 + 4;
      reverse(parameters.begin(), parameters.end());
      rbb->addInstrBack(new PushRiscvInst(parameters, rbb, foo->querySP()));
      // 调整到新栈空间
      rbb->addInstrBack(new BinaryRiscvInst(
          RiscvInstr::ADDI, static_cast<RiscvOperand *>(getRegOperand("sp")),
          static_cast<RiscvOperand *>(new RiscvConst(foo->querySP() - SPShift)),
          static_cast<RiscvOperand *>(getRegOperand("sp")), rbb));
      // ra的保护由caller去做
      rbb->addInstrBack(
          new StoreRiscvInst(new Type(Type::TypeID::IntegerTyID),
                             new RiscvIntReg(NamefindReg("ra")),
                             new RiscvIntPhiReg(NamefindReg("sp")), rbb));
      // 第二步：进行函数调用，告知callee的regAlloca分配单元。
      rbb->addInstrBack(
          this->createCallInstr(calleeFoo->regAlloca, curInstr, rbb));
      // 第三步：caller恢复栈帧，清除所有的函数参数
      // 首先恢复ra
      rbb->addInstrBack(new LoadRiscvInst(new Type(Type::TypeID::IntegerTyID),
                                          new RiscvIntReg(NamefindReg("ra")),
                                          new RiscvIntPhiReg(NamefindReg("sp")),
                                          rbb));
      rbb->addInstrBack(new BinaryRiscvInst(
          RiscvInstr::ADDI, static_cast<RiscvOperand *>(getRegOperand("sp")),
          static_cast<RiscvOperand *>(
              new RiscvConst(-foo->querySP() + SPShift)),
          static_cast<RiscvOperand *>(getRegOperand("sp")), rbb));
      // At last, save return value (a0) to target value.
      if (curInstr->type_->tid_ != curInstr->type_->VoidTyID) {
        rbb->addInstrBack(
            new StoreRiscvInst(new IntegerType(32), getRegOperand("a0"),
                               foo->regAlloca->findMem(curInstr), rbb));
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
        } else {
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
    if (rfoo->is_libfunc())
      continue;
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
          rfoo->regAlloca->DSU_for_Variable.merge(instr->operands_[0],
                                                  static_cast<Value *>(instr));
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
            data +=
                curFloatName + "\t.float\t" +
                std::to_string(dynamic_cast<ConstantFloat *>(Operand)->value_) +
                "\n";
            rfoo->regAlloca->setPosition(Operand,
                                         new RiscvFloatPhiReg(curFloatName, 0));
          }
    // 首先检查所有的alloca指令，加入一个基本块进行寄存器保护以及栈空间分配
    RiscvBasicBlock *initBlock = createRiscvBasicBlock();
    std::map<Value *, int> haveAllocated;
    int IntParaCount = 0, FloatParaCount = 0, ParaShift = -4;

    // 函数起始栈帧就从-4开始，在riscvFunction提前修订base的值
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
        if (curType->tid_ == Type::TypeID::IntegerTyID)
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
          if (IntParaCount < 8)
            rfoo->regAlloca->setPositionReg(
                *val, new RiscvIntReg(
                          NamefindReg("a" + std::to_string(IntParaCount))));
          rfoo->regAlloca->setPosition(
              *val, new RiscvIntPhiReg(NamefindReg("sp"), ParaShift));
          IntParaCount++;
        }
        // 浮点参数
        else {
          assert((*val)->type_->tid_ == Type::TypeID::FloatTyID);
          // 寄存器有
          if (FloatParaCount < 8) {
            rfoo->regAlloca->setPositionReg(
                *val, new RiscvFloatReg(
                          NamefindReg("fa" + std::to_string(FloatParaCount))));
          }
          rfoo->regAlloca->setPositionReg(
              *val, new RiscvFloatPhiReg(NamefindReg("sp"), ParaShift));
          FloatParaCount++;
        }
        ParaShift += 4;
      }
      // 函数内参数
      else {
        int curSP = rfoo->querySP();
        RiscvOperand *stackPos = static_cast<RiscvOperand *>(
            new RiscvIntPhiReg(NamefindReg("sp"), curSP));
        rfoo->regAlloca->setPosition(static_cast<Value *>(*val), stackPos);
        rfoo->addTempVar(stackPos);
      }
      haveAllocated[*val] = 1;
    };

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
          int curTypeSize = this->calcTypeSize(curInstr->alloca_ty_);
          rfoo->storeArray(curTypeSize);
          int curSP = rfoo->querySP();
          RiscvOperand *ptrPos = new RiscvIntPhiReg(NamefindReg("sp"), curSP);
          rfoo->regAlloca->setPointerPos(static_cast<Value *>(instr), ptrPos);
        }
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
    +-----------+
    | 新局部变量 |
    +-----------+
    | 保护寄存器 |
    +-----------+
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

int RiscvBuilder::calcTypeSize(Type *ty) {
  int totalsize = 1;
  while (ty->tid_ == Type::ArrayTyID) {
    totalsize *= static_cast<ArrayType *>(ty)->num_elements_;
    ty = static_cast<ArrayType *>(ty)->contained_;
  }
  assert(ty->tid_ == Type::IntegerTyID || ty->tid_ == Type::FloatTyID ||
         ty->tid_ == Type::PointerTyID);
  totalsize *= 4;
  return totalsize;
}