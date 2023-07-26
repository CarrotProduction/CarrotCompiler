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

// 全部合流
void RiscvBuilder::solvePhiInstr(PhiInst *instr) {
  int n = instr->operands_.size();
  for (int i = 1; i < n; i++)
    DSU_for_Variable.merge(instr->operands_[i]->name_,
                           instr->operands_[0]->name_);
}

BinaryRiscvInst *RiscvBuilder::createBinaryInstr(RegAlloca *regAlloca,
                                                 BinaryInst *binaryInstr,
                                                 RiscvBasicBlock *rbb) {
  auto id = toRiscvOp.at(binaryInstr->op_id_);
  // 立即数区分
  if (dynamic_cast<ConstantInt *>(binaryInstr->operands_[1]) != nullptr)
    id = static_cast<RiscvInstr::InstrType>(id ^ 1);
  BinaryRiscvInst *instr = new BinaryRiscvInst(
      id, regAlloca->find(binaryInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->find(binaryInstr->operands_[1], rbb, nullptr, 1),
      regAlloca->find(binaryInstr, rbb, nullptr, 1), rbb);
  return instr;
}

UnaryRiscvInst *RiscvBuilder::createUnaryInstr(RegAlloca *regAlloca,
                                               UnaryInst *unaryInstr,
                                               RiscvBasicBlock *rbb) {
  UnaryRiscvInst *instr = new UnaryRiscvInst(
      toRiscvOp.at(unaryInstr->op_id_),
      regAlloca->find(unaryInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->find(unaryInstr, rbb, nullptr, 1), rbb);
  return instr;
}

// IR中的Store对应到RISCV为MOV指令或LI指令
MoveRiscvInst *RiscvBuilder::createStoreInstr(RegAlloca *regAlloca,
                                              StoreInst *storeInstr,
                                              RiscvBasicBlock *rbb) {
  auto testConstInt = dynamic_cast<ConstantInt *>(storeInstr->operands_[0]);
  if (testConstInt != nullptr) {
    // 整数部分可以直接li指令
    MoveRiscvInst *instr =
        new MoveRiscvInst(regAlloca->find(storeInstr->operands_[1], rbb),
                          new RiscvConst(testConstInt->value_), rbb);
    return instr;
  }
  // 浮点部分需要提前写入内存中，然后等效于直接mov
  // TODO:先把浮点常数以全局变量形式存入内存中，再直接fmv
  MoveRiscvInst *instr = new MoveRiscvInst(
      regAlloca->find(storeInstr->operands_[1], rbb),
      regAlloca->find(storeInstr->operands_[0], rbb, nullptr, 1), rbb);
  return instr;
}

// Load 指令仅一个参数！它本身就是一个value
MoveRiscvInst *RiscvBuilder::createLoadInstr(RegAlloca *regAlloca,
                                             LoadInst *loadInstr,
                                             RiscvBasicBlock *rbb) {
  MoveRiscvInst *instr = new MoveRiscvInst(
      regAlloca->find(static_cast<Value *>(loadInstr), rbb),
      regAlloca->find(loadInstr->operands_[0], rbb, nullptr, 1), rbb);
  return instr;
}

ICmpRiscvInstr *RiscvBuilder::createICMPInstr(RegAlloca *regAlloca,
                                              ICmpInst *icmpInstr,
                                              BranchInst *brInstr,
                                              RiscvBasicBlock *rbb) {
  ICmpRiscvInstr *instr = new ICmpRiscvInstr(
      icmpInstr->icmp_op_,
      regAlloca->find(icmpInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->find(icmpInstr->operands_[1], rbb, nullptr, 1),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
      rbb);
  return instr;
}

// FCMP处理上和ICMP类似，但是在最后生成语句的时候是输出两句
FCmpRiscvInstr *RiscvBuilder::createFCMPInstr(RegAlloca *regAlloca,
                                              FCmpInst *fcmpInstr,
                                              BranchInst *brInstr,
                                              RiscvBasicBlock *rbb) {
  FCmpRiscvInstr *instr = new FCmpRiscvInstr(
      fcmpInstr->fcmp_op_,
      regAlloca->find(fcmpInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->find(fcmpInstr->operands_[1], rbb, nullptr, 1),
      regAlloca->findNonuse(rbb, nullptr),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[1])),
      createRiscvBasicBlock(static_cast<BasicBlock *>(brInstr->operands_[2])),
      rbb);
  return instr;
}

// 无条件跳转指令：JMP
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
  std::vector<RiscvOperand *> args;
  // push 指令需要寄存器
  int argnum = callInstr->operands_.size() - 1;
  for (int i = 1; i < callInstr->operands_.size(); i++) {
    args.push_back(regAlloca->find(callInstr->operands_[i], rbb, nullptr, 1));
    // 子函数栈帧
    RiscvOperand *stackPos = static_cast<RiscvOperand *>(
        new RiscvIntPhiReg(findReg("fp"), 4 * (argnum - i + 1)));
    // 为 ra 和 BP 腾出两个空间出来
    regAlloca->setPosition(callInstr->operands_[i], stackPos);
  }
  // 涉及从Function 到RISCV function转换问题（第一个参数）
  CallRiscvInst *instr = new CallRiscvInst(
      createRiscvFunction(static_cast<Function *>(callInstr->operands_[0])),
      rbb, args);
  return instr;
}

// 注意：return语句本身并不负责返回值的传递，该语句由storeRet函数实现
ReturnRiscvInst *RiscvBuilder::createRetInstr(RegAlloca *regAlloca,
                                              ReturnInst *returnInstr,
                                              RiscvBasicBlock *rbb) {
  // return 仅用于return，在打印过程再进行恢复栈帧+push操作
  // 无返回值
  if (returnInstr->operands_.empty())
    return new ReturnRiscvInst(rbb);
  else
    // 有返回值
    return new ReturnRiscvInst(regAlloca->storeRet(returnInstr->operands_[0]),
                               rbb);
}

SiToFpRiscvInstr *RiscvBuilder::createSiToFpInstr(RegAlloca *regAlloca,
                                                  SiToFpInst *sitofpInstr,
                                                  RiscvBasicBlock *rbb) {
  return new SiToFpRiscvInstr(
      regAlloca->find(sitofpInstr->operands_[0], rbb, nullptr, 1), rbb);
}

FpToSiRiscvInstr *RiscvBuilder::createFptoSiInstr(RegAlloca *regAlloca,
                                                  FpToSiInst *fptosiInstr,
                                                  RiscvBasicBlock *rbb) {
  return new FpToSiRiscvInstr(
      regAlloca->find(fptosiInstr->operands_[0], rbb, nullptr, 1), rbb);
}

RiscvBasicBlock *RiscvBuilder::transferRiscvBasicBlock(BasicBlock *bb,
                                                       RiscvFunction *foo) {
  RiscvBasicBlock *rbb = createRiscvBasicBlock(bb);
  Instruction *forward = nullptr; // 前置指令，用于icmp、fcmp和branch指令合并
  for (Instruction *instr : bb->instr_list_) {
    switch (instr->op_id_) {
    case Instruction::Ret:
      // 先恢复
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
      this->solvePhiInstr(static_cast<PhiInst *>(instr));
      break;
    // 直接删除的指令
    case Instruction::BitCast:
    case Instruction::ZExt:
    case Instruction::Alloca:
      break;
    // 找偏移量，待完成
    case Instruction::GetElementPtr:
      // TODO
      assert(false);
      break;
    case Instruction::FPtoSI:
      rbb->addInstrBack(this->createFptoSiInstr(
          foo->regAlloca, static_cast<FpToSiInst *>(instr), rbb));
      break;
    case Instruction::SItoFP:
      rbb->addInstrBack(this->createSiToFpInstr(
          foo->regAlloca, static_cast<SiToFpInst *>(instr), rbb));
      break;
    case Instruction::Load:
      rbb->addInstrBack(this->createLoadInstr(
          foo->regAlloca, static_cast<LoadInst *>(instr), rbb));
      break;
    case Instruction::Store:
      rbb->addInstrBack(this->createStoreInstr(
          foo->regAlloca, static_cast<StoreInst *>(instr), rbb));
      break;
    case Instruction::ICmp:
    case Instruction::FCmp:
      forward = instr;
      break;
    case Instruction::Call: {
      // 注意：该部分并未单独考虑系统函数！
      // 注意：区分float还是int调用是看寄存器分配部分实现
      CallInst *curInstr = static_cast<CallInst *>(instr);
      // 第一步：函数参数压栈，对于函数f(a0,a1,...,a7)，a7在高地址（0(sp)），a0在低地址（-4(sp)）
      // 注意：该步中并未约定一定要求是函数寄存器a0-a7
      int SPShift = 0;
      std::vector<RiscvOperand *> parameters;
      int intRegCount = 0, floatRegCount = 0;
      for (int i = 0; i < curInstr->operands_.size() - 1; i++) {
        std::string name = "";
        if (curInstr->operands_[i]->type_->tid_ == Type::IntegerTyID) {
          if (intRegCount < 8)
            name = "a" + std::to_string(intRegCount);
          intRegCount++;
        } else {
          if (floatRegCount < 8)
            name = "fa" + std::to_string(floatRegCount);
          floatRegCount++;
        }
        // 如果寄存器超过数额，放入栈中
        if (name.empty())
          parameters.push_back(
              foo->regAlloca->find(curInstr->operands_[i], rbb, nullptr, 1));
        // 否则放到指定寄存器a0-a7 fa0-fa7
        else
          parameters.push_back(foo->regAlloca->findSpecificReg(
              curInstr->operands_[i], name, rbb));
        SPShift += 4;
      }
      reverse(parameters.begin(), parameters.end());
      rbb->addInstrBack(new PushRiscvInst(parameters, rbb));
      // 第二步：进行函数调用
      rbb->addInstrBack(this->createCallInstr(foo->regAlloca, curInstr, rbb));
      // 第三步：caller恢复栈帧，清除所有的函数参数
      rbb->addInstrBack(new BinaryRiscvInst(
          RiscvInstr::ADDI,
          static_cast<RiscvOperand *>(new RiscvIntPhiReg(findReg("fp"))),
          static_cast<RiscvOperand *>(new RiscvConst(SPShift)),
          static_cast<RiscvOperand *>(new RiscvIntPhiReg(findReg("fp"))), rbb));
      break;
    }
    }
  }
  return rbb;
}

// // 系统函数，需提前约定其栈调用问题
// void RiscvBuilder::resolveLibfunc(RiscvFunction *foo) {
//   // 对不同函数进行分类讨论
//   assert(false);
// }

// 总控程序
std::string RiscvBuilder::buildRISCV(Module *m) {
  this->rm = new RiscvModule();
  std::string data = ".section .data\n";
  // 全局变量
  if (m->global_list_.size()) {
    for (GlobalVariable *gb : m->global_list_) {
      RiscvGlobalVariable *curGB = nullptr;
      ArrayType *containedType = nullptr;
      switch (gb->type_->tid_) {
      case Type::ArrayTyID:
        containedType = static_cast<ArrayType *>(gb->type_);
        if (containedType->tid_ == Type::IntegerTyID) {
          curGB = new RiscvGlobalVariable(RiscvOperand::IntImm, gb->name_,
                                          gb->is_const_, gb->init_val_,
                                          containedType->num_elements_);
          rm->addGlobalVariable(curGB);
          data += curGB->print();
        } else {
          curGB = new RiscvGlobalVariable(RiscvOperand::FloatImm, gb->name_,
                                          gb->is_const_, gb->init_val_,
                                          containedType->num_elements_);
          rm->addGlobalVariable(curGB);
          data += curGB->print();
        }
        break;
      case Type::TypeID::IntegerTyID:
        curGB = new RiscvGlobalVariable(RiscvOperand::OpTy::IntImm, gb->name_,
                                        gb->is_const_, gb->init_val_);
        rm->addGlobalVariable(curGB);
        data += curGB->print();
        break;
      case Type::TypeID::FloatTyID:
        curGB = new RiscvGlobalVariable(RiscvOperand::OpTy::FloatImm, gb->name_,
                                        gb->is_const_, gb->init_val_);
        rm->addGlobalVariable(curGB);
        data += curGB->print();
        break;
      }
    }
  }
  // 浮点常量进入内存
  int ConstFloatCount = 0;
  std::string code = ".section .text\n";
  // 函数体
  for (Function *foo : m->function_list_) {
    auto rfoo = createRiscvFunction(foo);
    rm->addFunction(rfoo);
    if (rfoo->is_libfunc())
      continue;
    // 将该函数内的浮点变量全部处理出来并告知寄存器分配单元
    for (BasicBlock *bb : foo->basic_blocks_)
      for (Instruction *instr : bb->instr_list_)
        for (auto *Operand : instr->operands_)
          // 找到浮点常数，存入内存，写入全局变量区
          if (dynamic_cast<ConstantFloat *>(Operand) != nullptr) {
            std::string curFloatName = "FloatConst" + std::to_string(ConstFloatCount);
            ConstFloatCount++;
            data += curFloatName + "\t.float\t" + std::to_string(dynamic_cast<ConstantFloat *>(Operand)->value_) + "\n";
            rfoo->regAlloca->setPosition(Operand, new RiscvFloatPhiReg(curFloatName, 0));
          }
    // 首先检查所有的alloca指令，加入一个基本块进行寄存器保护以及栈空间分配
    RiscvBasicBlock *initBlock = createRiscvBasicBlock();
    for (BasicBlock *bb : foo->basic_blocks_)
      for (Instruction *instr : bb->instr_list_)
        // 处理AllocaInstr
        if (typeid(*instr) == typeid(AllocaInst)) {
          int curSP = rfoo->querySP();
          RiscvOperand *stackPos = static_cast<RiscvOperand *>(
              new RiscvIntPhiReg(findReg("fp"), curSP));
          rfoo->regAlloca->setPosition(static_cast<Value *>(instr), stackPos);
          rfoo->addTempVar(stackPos);
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
    |  分配变量  |
    +-----------+
    | 保护寄存器 |
    +-----------+
    因而需要先下移腾出空间，再进行push操作
    */
    int diff = curSP - rfoo->querySP();
    if (diff)
      initBlock->addInstrBack(new BinaryRiscvInst(
          RiscvInstr::SUBI,
          static_cast<RiscvOperand *>(new RiscvIntPhiReg(findReg("fp"))),
          static_cast<RiscvOperand *>(new RiscvConst(diff)),
          static_cast<RiscvOperand *>(new RiscvIntPhiReg(findReg("fp"))),
          initBlock));
    // 然后再考虑把这些寄存器进行保护生成对应的代码
    initBlock->addInstrBack(new PushRiscvInst(savedRegister, initBlock));
    rfoo->ChangeBlock(initBlock, 0);
    // 第二遍扫描，处理所有的return语句之前的恢复现场等操作
    // 由于是倒过来压栈，因而现在要反序
    reverse(savedRegister.begin(), savedRegister.end());
    for (RiscvBasicBlock *rbb : rfoo->blk) {
      for (RiscvInstr *instr : rbb->instruction)
        // 对于return语句的返回操作进行处理
        if (typeid(*instr) == typeid(RiscvInstr::RET)) {
          rbb->addInstrBefore(new PopRiscvInst(savedRegister, rbb), instr);
          rbb->addInstrBefore(
              new BinaryRiscvInst(
                  RiscvInstr::ADDI,
                  static_cast<RiscvOperand *>(
                      new RiscvIntPhiReg(findReg("sp"))),
                  static_cast<RiscvOperand *>(new RiscvConst(diff)),
                  static_cast<RiscvOperand *>(
                      new RiscvIntPhiReg(findReg("sp"))),
                  initBlock),
              instr);
        }
    }
    code += rfoo->print();
  }
  return data + code;
}