#include "backend.h"
#include <cassert>

void RiscvBuilder::initializeRegisterFile() {
  // todo：分配寄存器堆，初始化寄存器堆各项参数
  assert(false);
}

const std::map<Instruction::OpID, RiscvInstr::InstrType> toRiscvOp = {};
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

BinaryRiscvInst *RiscvBuilder::createBinaryInstr(BinaryInst *binaryInstr,
                                                 RiscvBasicBlock *rbb) {
  BinaryRiscvInst *instr = new BinaryRiscvInst(
      toRiscvOp.at(binaryInstr->op_id_),
      regAlloca->find(binaryInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->find(binaryInstr->operands_[1], rbb, nullptr, 1),
      regAlloca->find(binaryInstr, rbb, nullptr, 1), rbb);
  return instr;
}

// Alloca 语句实质是在栈上预留空间，因而为该变量建立合适的地址映射即可。
void RiscvBuilder::solveAlloca(AllocaInst *instr, RiscvFunction *foo,
                               RiscvBasicBlock *rbb) {
  int curBP = foo->querySP();
  RiscvOperand *stackPos =
      static_cast<RiscvOperand *>(new RiscvIntPhiReg(findReg.at("fp"), curBP));
  this->regAlloca->setPosition(static_cast<Value *>(instr), stackPos);
  foo->addTempVar(stackPos);
}

UnaryRiscvInst *RiscvBuilder::createUnaryInstr(UnaryInst *unaryInstr,
                                               RiscvBasicBlock *rbb) {
  UnaryRiscvInst *instr = new UnaryRiscvInst(
      toRiscvOp.at(unaryInstr->op_id_),
      regAlloca->find(unaryInstr->operands_[0], rbb, nullptr, 1),
      regAlloca->find(unaryInstr, rbb, nullptr, 1), rbb);
  return instr;
}

StoreRiscvInst *RiscvBuilder::createStoreInstr(StoreInst *storeInstr,
                                               RiscvBasicBlock *rbb) {
  StoreRiscvInst *instr = new StoreRiscvInst(
      storeInstr->type_, regAlloca->find(storeInstr->operands_[0], rbb),
      regAlloca->find(storeInstr->operands_[1], rbb, nullptr, 1), rbb);
  return instr;
}

LoadRiscvInst *RiscvBuilder::createLoadInstr(LoadInst *loadInstr,
                                             RiscvBasicBlock *rbb) {
  LoadRiscvInst *instr = new LoadRiscvInst(
      loadInstr->type_, regAlloca->find(loadInstr->operands_[0], rbb),
      regAlloca->find(loadInstr->operands_[1], rbb, nullptr, 1), rbb);
  return instr;
}

ICmpRiscvInstr *RiscvBuilder::createICMPInstr(ICmpInst *icmpInstr,
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
FCmpRiscvInstr *RiscvBuilder::createFCMPInstr(FCmpInst *fcmpInstr,
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

CallRiscvInst *RiscvBuilder::createCallInstr(CallInst *callInstr,
                                             RiscvBasicBlock *rbb) {
  std::vector<RiscvOperand *> args;
  // push 指令需要寄存器
  int argnum = callInstr->operands_.size() - 1;
  for (int i = 1; i < callInstr->operands_.size(); i++) {
    args.push_back(regAlloca->find(callInstr->operands_[i], rbb, nullptr, 1));
    // 子函数栈帧
    RiscvOperand *stackPos = static_cast<RiscvOperand *>(
        new RiscvIntPhiReg(findReg.at("fp"), 4 * (argnum - i + 1)));
    // 为 ra 和 BP 腾出两个空间出来
    this->regAlloca->setPosition(callInstr->operands_[i], stackPos);
  }
  // 涉及从Function 到RISCV function转换问题（第一个参数）
  CallRiscvInst *instr = new CallRiscvInst(
      createRiscvFunction(static_cast<Function *>(callInstr->operands_[0])),
      rbb, args);
  return instr;
}

ReturnRiscvInst *RiscvBuilder::createRetInstr(ReturnInst *returnInstr,
                                              RiscvBasicBlock *rbb) {
  // 无返回值
  if (returnInstr->operands_.empty())
    return new ReturnRiscvInst(rbb);
  else
    // 有返回值
    return new ReturnRiscvInst(regAlloca->storeRet(returnInstr->operands_[0]),
                               rbb);
}

SiToFpRiscvInstr *RiscvBuilder::createSiToFpInstr(SiToFpInst *sitofpInstr,
                                                  RiscvBasicBlock *rbb) {
  return new SiToFpRiscvInstr(
      regAlloca->find(sitofpInstr->operands_[0], rbb, nullptr, 1), rbb);
}

FpToSiRiscvInstr *RiscvBuilder::createFptoSiInstr(FpToSiInst *fptosiInstr,
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
      rbb->addInstrBack(
          this->createRetInstr(static_cast<ReturnInst *>(instr), rbb));
      break;
    // 分支指令
    case Instruction::Br:
      if (forward->op_id_ == Instruction::FCmp)
        rbb->addInstrBack(
            this->createFCMPInstr(static_cast<FCmpInst *>(forward),
                                  static_cast<BranchInst *>(instr), rbb));
      else
        rbb->addInstrBack(
            this->createICMPInstr(static_cast<ICmpInst *>(forward),
                                  static_cast<BranchInst *>(instr), rbb));
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
      rbb->addInstrBack(
          this->createBinaryInstr(static_cast<BinaryInst *>(instr), rbb));
      break;
    case Instruction::FNeg:
      rbb->addInstrBack(
          this->createUnaryInstr(static_cast<UnaryInst *>(instr), rbb));
      break;
    case Instruction::PHI:
      this->solvePhiInstr(static_cast<PhiInst *>(instr));
      break;
    // 直接删除的指令
    case Instruction::BitCast:
    case Instruction::ZExt:
      break;
    // 找偏移量，待完成
    case Instruction::GetElementPtr:
      break;
    case Instruction::Alloca:
      this->solveAlloca(static_cast<AllocaInst *>(instr), foo, rbb);
      break;
    case Instruction::FPtoSI:
      rbb->addInstrBack(
          this->createFptoSiInstr(static_cast<FpToSiInst *>(instr), rbb));
      break;
    case Instruction::SItoFP:
      rbb->addInstrBack(
          this->createSiToFpInstr(static_cast<SiToFpInst *>(instr), rbb));
      break;
    case Instruction::Load:
      rbb->addInstrBack(
          this->createLoadInstr(static_cast<LoadInst *>(instr), rbb));
      break;
    case Instruction::Store:
      rbb->addInstrBack(
          this->createStoreInstr(static_cast<StoreInst *>(instr), rbb));
      break;
    case Instruction::ICmp:
    case Instruction::FCmp:
      forward = instr;
      break;
    case Instruction::Call:
      rbb->addInstrBack(
          this->createCallInstr(static_cast<CallInst *>(instr), rbb));
      break;
    }
  }
  return rbb;
}

// 系统函数，需提前约定其栈调用问题
void RiscvBuilder::resolveLibfunc(RiscvFunction *foo) {
  // 对不同函数进行分类讨论
  assert(false);
}

// 总控程序
std::string RiscvBuilder::buildRISCV(Module *m) {
  this->rm = new RiscvModule();
  std::string code = "";
  // 全局变量
  if (m->global_list_.size()) {
    code += ".section .data\n";
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
          code += curGB->print();
        } else {
          curGB = new RiscvGlobalVariable(
              RiscvOperand::FloatImm, gb->name_, gb->is_const_,
              gb->init_val_, containedType->num_elements_);
          rm->addGlobalVariable(curGB);
          code += curGB->print();
        }
        break;
      case Type::TypeID::IntegerTyID:
        curGB = new RiscvGlobalVariable(RiscvOperand::OpTy::IntImm, gb->name_,
                                             gb->is_const_, gb->init_val_);
        rm->addGlobalVariable(curGB);
        code += curGB->print();
        break;
      case Type::TypeID::FloatTyID:
        curGB = new RiscvGlobalVariable(RiscvOperand::OpTy::FloatImm, gb->name_,
                                             gb->is_const_, gb->init_val_);
        rm->addGlobalVariable(curGB);
        code += curGB->print();
        break;
      }
    }
  }
  code += ".section .text\n.globl main\n";
  // 函数体
  for (Function *foo : m->function_list_) {
    auto rfoo = createRiscvFunction(foo);
    rm->addFunction(rfoo);
    if (rfoo->is_libfunc())
      continue;
    for (BasicBlock *bb : foo->basic_blocks_)
      rfoo->addBlock(this->transferRiscvBasicBlock(bb, rfoo));
    code += rfoo->print();
  }
  return code;
}