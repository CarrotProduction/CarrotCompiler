#include "instruction.h"
#include "riscv.h"
#include <cassert>
#include <string>
// IR的指令转变到RISV的指令
std::map<RiscvInstr::InstrType, std::string> instrTy2Riscv = {
    {RiscvInstr::ADD, "ADD"},         {RiscvInstr::ADDI, "ADDI"},
    {RiscvInstr::SUB, "SUB"},         {RiscvInstr::SUBI, "SUBI"},
    {RiscvInstr::FADD, "FADD.S"},     {RiscvInstr::FSUB, "FSUB.S"},
    {RiscvInstr::FMUL, "FMUL.S"},     {RiscvInstr::FDIV, "FDIV.S"},
    {RiscvInstr::MUL, "MUL"},         {RiscvInstr::DIV, "DIV"},
    {RiscvInstr::REM, "REM"},         {RiscvInstr::AND, "AND"},
    {RiscvInstr::OR, "OR"},           {RiscvInstr::ANDI, "ANDI"},
    {RiscvInstr::ORI, "ORI"},         {RiscvInstr::XOR, "XOR"},
    {RiscvInstr::XORI, "XORI"},       {RiscvInstr::RET, "RET"},
    {RiscvInstr::FPTOSI, "FCVT.W.S"}, {RiscvInstr::SITOFP, "FCVT.S.W"},
    {RiscvInstr::FMV, "FMV.S"},       {RiscvInstr::CALL, "CALL"},
    {RiscvInstr::LI, "LI"},           {RiscvInstr::MOV, "MV"},
    {RiscvInstr::PUSH, "PUSH"},       {RiscvInstr::POP, "POP"},
    {RiscvInstr::SW, "SW"},           {RiscvInstr::LW, "LW"},
    {RiscvInstr::FSW, "FSW"},         {RiscvInstr::FLW, "FLW"},
    {RiscvInstr::SHL, "SLL"},         {RiscvInstr::ASHR, "SRA"},
    {RiscvInstr::SHLI, "SLLI"},       {RiscvInstr::LSHR, "SRL"},
    {RiscvInstr::ASHRI, "SRAI"},      {RiscvInstr::LSHRI, "SRLI"},
};
// Instruction from opid to string
const std::map<ICmpInst::ICmpOp, std::string> ICmpRiscvInstr::ICmpOpName = {
    {ICmpInst::ICmpOp::ICMP_EQ, "BEQ"},   {ICmpInst::ICmpOp::ICMP_NE, "BNE"},
    {ICmpInst::ICmpOp::ICMP_UGE, "BGEU"}, {ICmpInst::ICmpOp::ICMP_ULT, "BLTU"},
    {ICmpInst::ICmpOp::ICMP_SGE, "BGE"},  {ICmpInst::ICmpOp::ICMP_SLT, "BLT"},
    {ICmpInst::ICmpOp::ICMP_SLE, "BLE"}};
const std::map<ICmpInst::ICmpOp, std::string> ICmpRiscvInstr::ICmpOpSName = {
    {ICmpInst::ICmpOp::ICMP_EQ, "SEQZ"},  {ICmpInst::ICmpOp::ICMP_NE, "SNEZ"},
    {ICmpInst::ICmpOp::ICMP_UGE, "SLTU"}, {ICmpInst::ICmpOp::ICMP_ULT, "SLTU"},
    {ICmpInst::ICmpOp::ICMP_SGE, "SLT"},  {ICmpInst::ICmpOp::ICMP_SLT, "SLT"}};
const std::map<ICmpInst::ICmpOp, ICmpInst::ICmpOp> ICmpRiscvInstr::ICmpOpEquiv =
    {{ICmpInst::ICmpOp::ICMP_ULE, ICmpInst::ICmpOp::ICMP_UGE},
     {ICmpInst::ICmpOp::ICMP_UGT, ICmpInst::ICmpOp::ICMP_ULT},
     {ICmpInst::ICmpOp::ICMP_SLE, ICmpInst::ICmpOp::ICMP_SGE},
     {ICmpInst::ICmpOp::ICMP_SGT, ICmpInst::ICmpOp::ICMP_SLT}};
const std::map<FCmpInst::FCmpOp, std::string> FCmpRiscvInstr::FCmpOpName = {
    {FCmpInst::FCmpOp::FCMP_OLT, "FLT.S"},
    {FCmpInst::FCmpOp::FCMP_ULT, "FLT.S"},
    {FCmpInst::FCmpOp::FCMP_OGE, "FLT.S"},
    {FCmpInst::FCmpOp::FCMP_UGE, "FLT.S"},
    {FCmpInst::FCmpOp::FCMP_ORD, "FCLASS.S"},
    {FCmpInst::FCmpOp::FCMP_UNO, "FCLASS.S"}, // 取反
    {FCmpInst::FCmpOp::FCMP_OLE, "FLE.S"},
    {FCmpInst::FCmpOp::FCMP_ULE, "FLE.S"},
    {FCmpInst::FCmpOp::FCMP_OGT, "FLE.S"}, // 取反
    {FCmpInst::FCmpOp::FCMP_UGT, "FLE.S"}, // 取反
    {FCmpInst::FCmpOp::FCMP_OEQ, "FEQ.S"},
    {FCmpInst::FCmpOp::FCMP_UEQ, "FEQ.S"},
    {FCmpInst::FCmpOp::FCMP_ONE, "FEQ.S"}, // 取反
    {FCmpInst::FCmpOp::FCMP_UNE, "FEQ.S"}  // 取反
};
std::string print_as_op(Value *v, bool print_ty);
std::string print_cmp_type(ICmpInst::ICmpOp op);
std::string print_fcmp_type(FCmpInst::FCmpOp op);

RiscvInstr::RiscvInstr(InstrType type, int op_nums)
    : type_(type), parent_(nullptr) {
  operand_.resize(op_nums);
}

RiscvInstr::RiscvInstr(InstrType type, int op_nums, RiscvBasicBlock *bb)
    : type_(type), parent_(bb) {
  operand_.resize(op_nums);
}

// 格式：op  tar, v1, v2->tar=v1 op v2
std::string BinaryRiscvInst::print() {
  // 这里需要将每个参数根据当前需要进行upcasting
  assert(this->operand_.size() == 2);
  std::string riscv_instr = "\t\t";
  riscv_instr += instrTy2Riscv.at(this->type_);
  riscv_instr += "\t";
  riscv_instr += this->result_->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += "\n";
  return riscv_instr;
}

std::string UnaryRiscvInst::print() {
  assert(this->operand_.size() == 1);
  std::string riscv_instr = "\t\t";
  riscv_instr += instrTy2Riscv[this->type_];
  riscv_instr += "\t";
  riscv_instr += this->result_->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  return riscv_instr;
}

std::string CallRiscvInst::print() {
  std::string riscv_instr = "\t\tCALL\t";
  riscv_instr += static_cast<RiscvFunction *>(this->operand_[0])->name_;
  riscv_instr += "\n";
  return riscv_instr;
}

// 注意：return 语句不会进行相应的寄存器约定检查
std::string ReturnRiscvInst::print() {
  std::string riscv_instr = "\t\tRET\n";
  return riscv_instr;
}

std::string PushRiscvInst::print() {
  std::string riscv_instr = "";
  int shift = this->basicShift_;
  for (auto x : this->operand_) {
    shift -= 4;
    riscv_instr +=
        "\t\tSW\t" + x->print() + ", " + std::to_string(shift) + "(sp)\n";
  }
  return riscv_instr;
}

std::string PopRiscvInst::print() {
  std::string riscv_instr = "";
  int shift = this->basicShift_;
  for (auto x : this->operand_) {
    shift -= 4;
    riscv_instr +=
        "\t\tLW\t" + x->print() + ", " + std::to_string(shift) + "(sp)\n";
  }
  riscv_instr += "\t\tADDI\tsp, " + std::to_string(-shift) + "\n";
  return riscv_instr;
}

std::string ICmpRiscvInstr::print() {
  std::string riscv_instr = "\t\t";
  // 注意：由于RISCV不支持全部的比较运算，因而需要根据比较条件对式子进行等价变换
  if (ICmpOpName.count(this->icmp_op_) == 0) {
    std::swap(this->operand_[0], this->operand_[1]);
    this->icmp_op_ = ICmpRiscvInstr::ICmpOpEquiv.find(this->icmp_op_)->second;
  }
  riscv_instr += ICmpOpName.at(this->icmp_op_) + "\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += ", ";
  riscv_instr += static_cast<RiscvBasicBlock *>(this->operand_[2])->name_;
  riscv_instr += "\n";
  auto falseLink = dynamic_cast<RiscvBasicBlock *>(this->operand_[3]);
  // Force Jump
  if (falseLink != nullptr)
    riscv_instr += "\t\tJ\t" + falseLink->name_ + "\n";
  return riscv_instr;
}

std::string ICmpSRiscvInstr::print() {
  std::string riscv_instr = "\t\t";

  // If equal or nequal instruction
  bool eorne = false;
  switch (icmp_op_) {
  case ICmpInst::ICMP_EQ:
  case ICmpInst::ICMP_NE:
    eorne = true;
  default:
    break;
  }

  if (eorne) {
    riscv_instr += "SUB\t";
    riscv_instr += "t5";
    riscv_instr += ", ";
    riscv_instr += operand_[0]->print();
    riscv_instr += ", ";
    riscv_instr += operand_[1]->print();
    riscv_instr += "\n\t\t";
  }

  riscv_instr += ICmpOpSName.at(this->icmp_op_) + "\t";
  riscv_instr += this->result_->print();
  riscv_instr += ", ";
  if (!eorne) {
    riscv_instr += this->operand_[0]->print();
    riscv_instr += ", ";
    riscv_instr += this->operand_[1]->print();
    riscv_instr += "\n";
  } else {
    riscv_instr += "t5\n";
  }
  return riscv_instr;
}

std::string FCmpRiscvInstr::print() {
  if (this->fcmp_op_ == FCmpInst::FCMP_FALSE)
    return "\t\tJ\t" +
           static_cast<RiscvBasicBlock *>(this->operand_[4])->name_ + "\n";
  else if (this->fcmp_op_ == FCmpInst::FCMP_TRUE)
    return "\t\tJ\t" +
           static_cast<RiscvBasicBlock *>(this->operand_[3])->name_ + "\n";
  // 第一条指令
  std::string riscv_instr = "\t\t";
  riscv_instr += FCmpOpName.at(this->fcmp_op_) + "\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[2]->print();
  riscv_instr += "\n\t\t";
  // 第二条指令
  // 取反指令
  if (this->fcmp_op_ == FCmpInst::FCMP_UNO ||
      this->fcmp_op_ == FCmpInst::FCMP_OGT ||
      this->fcmp_op_ == FCmpInst::FCMP_UGT ||
      this->fcmp_op_ == FCmpInst::FCMP_ONE ||
      this->fcmp_op_ == FCmpInst::FCMP_UNE)
    std::swap(this->operand_[3], this->operand_[4]);
  riscv_instr += "BNE\t" + this->operand_[2]->print() + ", ZERO, " +
                 static_cast<RiscvBasicBlock *>(this->operand_[3])->name_ +
                 "\n";
  auto falseLink = dynamic_cast<RiscvBasicBlock *>(this->operand_[4]);
  // Force Jump
  riscv_instr += "\t\tJ\t" + falseLink->name_ + "\n";
  return riscv_instr;
}

std::string JumpRiscvInstr::print() {
  std::string riscv_instr =
      "\t\tJ\t" + static_cast<RiscvBasicBlock *>(this->operand_[0])->name_ +
      "\n";
  return riscv_instr;
}

std::string StoreRiscvInst::print() {
  std::string riscv_instr = "\t\t";
  if (this->type.tid_ == Type::FloatTyID)
    riscv_instr += "FSW\t";
  else
    riscv_instr += "SW\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += "\n";
  return riscv_instr;
}

// 注意：简易版 load。如果不存在目标内存地址，本条指令不执行
std::string LoadRiscvInst::print() {
  if (this->operand_[0] == nullptr || this->operand_[1] == nullptr)
    return "";
  std::string riscv_instr = "\t\t";
  if (this->type.tid_ == Type::FloatTyID)
    riscv_instr += "FLW\t";
  else
    riscv_instr += "LW\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  assert(this->operand_[1] != nullptr);
  riscv_instr += this->operand_[1]->print();
  riscv_instr += "\n";
  return riscv_instr;
}

std::string MoveRiscvInst::print() {
  std::string riscv_instr = "\t\t";
  // li 指令
  if (this->operand_[1]->tid_ == RiscvOperand::IntImm)
    riscv_instr += "LI\t";
  // 寄存器传寄存器
  else if (this->operand_[1]->tid_ == RiscvOperand::IntReg)
    riscv_instr += "MV\t";
  // 浮点数
  else
    riscv_instr += "FMV\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += "\n";
  return riscv_instr;
}

std::string SiToFpRiscvInstr::print() {
  std::string riscv_instr = "\t\tFCVT.S.W\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += "\n";
  return riscv_instr;
}

std::string FpToSiRiscvInstr::print() {
  std::string riscv_instr = "\t\tFCVT.W.S\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += "\n";
  return riscv_instr;
}

std::string LoadAddressRiscvInstr::print() {
  std::string riscv_instr =
      "\t\tLA\t" + this->operand_[0]->print() + ", " + this->name_ + "\n";
  return riscv_instr;
}
