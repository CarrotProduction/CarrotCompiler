#include "riscv.h"
#include "backend.h"
#include "ir.h"

const int REG_NUMBER = 32;

RiscvFunction::RiscvFunction(std::string name, int num_args,
                             OpTy Ty) // 返回值，无返回使用void类型
    : RiscvLabel(Function, name), num_args_(num_args), resType_(Ty), base_(-4) {
  regAlloca = new RegAlloca();
}
// 输出函数对应的全部riscv语句序列
// 由于一个函数可能有若干个出口，因而恢复现场的语句根据basic block
// 语句中的ret语句前面附带出现，因而不在此出现
std::string RiscvFunction::print() {
  // TODO: temporaily add '.global' to declare function
  // Don't know if '.type' is needed
  std::string riscvInstr =
      ".global " + this->name_ + "\n" + this->name_ + ":\n"; // 函数标号打印
  // 对各个basic block进行拼接
  for (auto x : this->blk)
    riscvInstr += x->print();
  return riscvInstr;
}

std::string RiscvBasicBlock::print() {
  std::string riscvInstr = this->name_ + ":\n";
  for (auto x : this->instruction)
    riscvInstr += x->print();
  return riscvInstr;
}

// 出栈顺序和入栈相反
// 建议不使用pop语句，直接从栈中取值，最后直接修改sp的值即可
// 使用一个单独的return block以防止多出口return
extern int LabelCount;

RiscvOperand::OpTy RiscvOperand::getType() { return tid_; }

bool RiscvOperand::isRegister() { return tid_ == FloatReg || tid_ == IntReg; }

Type *findPtrType(Type *ty) {
  while (ty->tid_ == Type::PointerTyID) {
    ty = static_cast<PointerType *>(ty)->contained_;
  }
  while (ty->tid_ == Type::ArrayTyID) {
    ty = static_cast<ArrayType *>(ty)->contained_;
  }
  assert(ty->tid_ == Type::IntegerTyID || ty->tid_ == Type::FloatTyID);
  return ty;
}

std::string RiscvGlobalVariable::print(bool print_name, Constant *initVal) {
  std::string code = "";
  // 如果在调用的第一层，初始化 initVal
  if (print_name) {
    code += this->name_ + ":\n";
    initVal = initValue_;
  }
  // 如果无初始值，或初始值为0（IR中有ConstZero类），则直接用zero命令
  if (initVal == nullptr || dynamic_cast<ConstantZero *>(initVal) != nullptr) {
    code += ".zero\t" + std::to_string(calcTypeSize(initVal->type_)) + "\n";
    return code;
  }

  // 下面是非零的处理
  // 整型
  if (initVal->type_->tid_ == Type::TypeID::IntegerTyID)
    code += ".word\t";
  // 浮点
  else if (initVal->type_->tid_ == Type::TypeID::FloatTyID)
    code += ".word\t";
  else if (initVal->type_->tid_ == Type::TypeID::ArrayTyID) {
    ConstantArray *const_arr = dynamic_cast<ConstantArray *>(initVal);
    assert(const_arr != nullptr);
    for (auto elements : const_arr->const_array)
      code += print(false, elements);
  } else {
    std::cerr
        << "[Fatal Error] Unknown RiscvGlobalVariable::print() initValue type."
        << std::endl;
    std::terminate();
  }

  int zeroNumber = this->elementNum_;
  if (initVal == nullptr) {
    if (zeroNumber == 1)
      code += "0";
    // else
    //   code += "[" + std::to_string(zeroNumber) + " dup(0)]";
  } else {
    if (typeid(*initVal) == typeid(ConstantArray)) {
      zeroNumber -= static_cast<ArrayType *>(initVal->type_)->num_elements_;
      if (code.back() == '\n')
        code.pop_back();
      // 补充冗余0
      // if (zeroNumber > 0)
      // code += "[" + std::to_string(zeroNumber) + " dup(0)]";
    } else {
      code += initVal->print();
    }
  }
  code += "\n";
  if (print_name) {
    code += ".size\t";
    code += name_;
    code += ", ";
    code += std::to_string(calcTypeSize(initVal->type_));
    code += "\n";
  }
  return code;
}

std::string RiscvGlobalVariable::print() { return print(true, nullptr); }
RiscvFunction* createSyslibFunc(Function *foo) {
  if (foo->name_ == "__aeabi_memclr4") {
    auto *rfoo = createRiscvFunction(foo);
    // 预处理块
    auto *bb1 = createRiscvBasicBlock();
    bb1->addInstrBack(new MoveRiscvInst(new RiscvIntReg(NamefindReg("t5")), getRegOperand("a0"), bb1));
    bb1->addInstrBack(new BinaryRiscvInst(RiscvInstr::SHLI, getRegOperand("a1"), new RiscvConst(2), getRegOperand("t6"), bb1));
    bb1->addInstrBack(new BinaryRiscvInst(RiscvInstr::ADD, getRegOperand("a0"), getRegOperand("t6"), getRegOperand("t6"), bb1));
    bb1->addInstrBack(new MoveRiscvInst(getRegOperand("a0"), new RiscvConst(0), bb1));
    auto *bb2 = createRiscvBasicBlock();
    // 循环块
    // 默认clear为全0
    bb2->addInstrBack(new StoreRiscvInst(new Type(Type::TypeID::IntegerTyID), getRegOperand("a0"), new RiscvIntPhiReg(NamefindReg("t5")), bb2));
    bb2->addInstrBack(new BinaryRiscvInst(RiscvInstr::ADDI, getRegOperand("t5"), new RiscvConst(4), getRegOperand("t5"), bb1));
    bb2->addInstrBack(new ICmpRiscvInstr(ICmpInst::ICMP_SLT, getRegOperand("t5"), getRegOperand("t6"), bb2, bb2));
    bb2->addInstrBack(new ReturnRiscvInst(bb2));
    rfoo->addBlock(bb1);
    rfoo->addBlock(bb2);
    return rfoo;
  }
  return nullptr;
}
