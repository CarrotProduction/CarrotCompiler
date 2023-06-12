#include "instruction.h"
#include "riscv.h"

// 需要调整到RISCV
std::map<RiscvInstr::InstrType, std::string> instrTy2Riscv = {
    {RiscvInstr::ADD, "add"},
    {RiscvInstr::SUB, "sub"},
    {RiscvInstr::MUL, "mul"},
    {RiscvInstr::DIV, "div"},
    {RiscvInstr::REM, "rem"},
    {RiscvInstr::AND, "and"},
    {RiscvInstr::OR, "or"},
    {RiscvInstr::ANDI, "andi"},
    {RiscvInstr::ORI, "ori"},
    {RiscvInstr::XOR, "xor"},
    {RiscvInstr::XORI, "xori"},
    {RiscvInstr::RET, "ret"},
    {RiscvInstr::ADDI, "addi"},
    {RiscvInstr::SUBI, "subi"},
    {RiscvInstr::RET, "ret"},
    {RiscvInstr::CALL, "call"}}; // Instruction from opid to string
const std::map<ICmpInst::ICmpOp, std::string> ICmpInst::ICmpOpName = {
    {ICmpInst::ICmpOp::ICMP_EQ, "beq"},   {ICmpInst::ICmpOp::ICMP_NE, "bne"},
    {ICmpInst::ICmpOp::ICMP_UGE, "bgeu"}, {ICmpInst::ICmpOp::ICMP_ULT, "bltu"},
    {ICmpInst::ICmpOp::ICMP_SGE, "bge"},  {ICmpInst::ICmpOp::ICMP_SLT, "blt"}};
// 需要调整到RISCV
const std::map<FCmpInst::FCmpOp, std::string> FCmpInst::FCmpOpName = {
    {FCmpInst::FCmpOp::FCMP_FALSE, "nv"}, {FCmpInst::FCmpOp::FCMP_OEQ, "eq"},
    {FCmpInst::FCmpOp::FCMP_OGT, "gt"},   {FCmpInst::FCmpOp::FCMP_OGE, "ge"},
    {FCmpInst::FCmpOp::FCMP_OLT, "cc"},   {FCmpInst::FCmpOp::FCMP_OLE, "ls"},
    {FCmpInst::FCmpOp::FCMP_ONE, "ne"},   {FCmpInst::FCmpOp::FCMP_ORD, "vc"},
    {FCmpInst::FCmpOp::FCMP_UNO, "vs"},   {FCmpInst::FCmpOp::FCMP_UEQ, "eq"},
    {FCmpInst::FCmpOp::FCMP_UGT, "hi"},   {FCmpInst::FCmpOp::FCMP_UGE, "cs"},
    {FCmpInst::FCmpOp::FCMP_ULT, "lt"},   {FCmpInst::FCmpOp::FCMP_ULE, "le"},
    {FCmpInst::FCmpOp::FCMP_UNE, "ne"},   {FCmpInst::FCmpOp::FCMP_TRUE, "al"}};
std::string print_as_op(Value *v, bool print_ty);
std::string print_cmp_type(ICmpInst::ICmpOp op);
std::string print_fcmp_type(FCmpInst::FCmpOp op);

// 格式：op  tar, v1, v2->tar=v1 op v2
std::string BinaryRiscvInst::print() {
  // 这里需要将每个参数根据当前需要进行upcasting
  assert(this->operand_.size() == 2);
  std::string riscv_instr = "\t\t";
  riscv_instr += instrTy2Riscv[this->type_];
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
  std::string riscv_instr = "\t\tcall\t";
  riscv_instr += this->operand_[0]->print();
  return riscv_instr;
}

std::string ReturnRiscvInst::print() { return "\t\tret\n"; }

std::string PushRiscvInst::print() {
  std::string riscv_instr = "";
  for (auto x : this->operand_)
    riscv_instr += "\t\tpush\t" + x->print() + "\n";
  return riscv_instr;
}

std::string PopRiscvInst::print() {
  std::string riscv_instr = "";
  for (auto x : this->operand_)
    riscv_instr += "\t\tpop\t" + x->print() + "\n";
  return riscv_instr;
}

std::string ICmpRiscvInstr::print() {
  std::string riscv_instr = "\t\t";
  // 注意：由于RISCV不支持全部的比较运算，因而需要根据比较条件对式子进行等价变换
  if (ICmpOpName.count(this->icmp_op_) == 0) {
    std::swap(this->operand_[0], this->operand_[1]);
    this->icmp_op_ = static_cast<ICmpInst::ICmpOp>((int)this->icmp_op_ ^ 2);
  }
  riscv_instr += ICmpOpName.at(this->icmp_op_);
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[2]->print();
  riscv_instr += "\n";
  auto falseLink = dynamic_cast<RiscvBasicBlock *>(this->operand_[3]);
  // 不连续则假链也要跳转
  if (this->parent_->blockInd_ + 1 != falseLink->blockInd_)
    riscv_instr += "\t\tjmp\t" + falseLink->print() + "\n";
  return riscv_instr;
}

std::string FCmpRiscvInstr::print() {
  std::string riscv_instr = "\t\t";
  if (FCmpOpName.count(this->fcmp_op_) == 0) {
    std::swap(this->operand_[0], this->operand_[1]);
    this->fcmp_op_ = static_cast<FCmpInst::FCmpOp>((int)this->fcmp_op_ ^ 2);
  }
  riscv_instr += FCmpOpName.at(this->fcmp_op_);
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[2]->print();
  riscv_instr += "\n";
  auto falseLink = dynamic_cast<RiscvBasicBlock *>(this->operand_[3]);
  // 不连续则假链也要跳转
  if (this->parent_->blockInd_ + 1 != falseLink->blockInd_)
    riscv_instr += "\t\tjmp\t" + falseLink->print() + "\n";
  return riscv_instr;
}

std::string StoreRiscvInst::print() {
  std::string riscv_instr = "\t\t";
  if (this->type.tid_ == Type::FloatTyID)
    riscv_instr += "fsw\t";
  else
    riscv_instr += "sw\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += "\n";
  return riscv_instr;
}

std::string LoadRiscvInst::print() {
  std::string riscv_instr = "\t\t";
  if (this->type.tid_ == Type::FloatTyID)
    riscv_instr += "flw\t";
  else
    riscv_instr += "lw\t";
  riscv_instr += this->operand_[0]->print();
  riscv_instr += ", ";
  riscv_instr += this->operand_[1]->print();
  riscv_instr += "\n";
  return riscv_instr;
}