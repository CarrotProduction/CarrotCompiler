#include "riscv.h"

// 需要调整到RISCV
std::map<Instruction::OpID, std::string> instr_id2riscv = {
    {Instruction::Ret, "ret"},
    {Instruction::Br, "br"},
    {Instruction::FNeg, "fneg"},
    {Instruction::Add, "add"},
    {Instruction::Sub, "sub"},
    {Instruction::Mul, "mul"},
    {Instruction::SDiv, "sdiv"},
    {Instruction::SRem, "srem"},
    {Instruction::UDiv, "idiv"},
    {Instruction::URem, "urem"},
    {Instruction::FAdd, "fadd"},
    {Instruction::FSub, "fsub"},
    {Instruction::FMul, "fmul"},
    {Instruction::FDiv, "fdiv"},
    {Instruction::Shl, "shl"},
    {Instruction::LShr, "lshr"},
    {Instruction::AShr, "ashr"},
    {Instruction::And, "and"},
    {Instruction::Or, "or"},
    {Instruction::Xor, "xor"},
    {Instruction::Alloca, "alloca"},
    {Instruction::Load, "lw"},
    {Instruction::Store, "sw"},
    {Instruction::GetElementPtr, "getelementptr"},
    {Instruction::ZExt, "zext"},
    {Instruction::FPtoSI, "fptosi"},
    {Instruction::SItoFP, "sitofp"},
    {Instruction::BitCast, "bitcast"},
    {Instruction::ICmp, "icmp"},
    {Instruction::FCmp, "fcmp"},
    {Instruction::PHI, "phi"},
    {Instruction::Call, "call"}}; // Instruction from opid to string
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
  assert(this->operands_.size() == 2);
  std::string riscv_instr = "\t";
  riscv_instr += instr_id2riscv[this->op_id_];
  riscv_instr += "\t";
  riscv_instr += this->name_;
  riscv_instr += ", ";
  riscv_instr += this->operands_[0]->name_;
  riscv_instr += ", ";
  riscv_instr += this->operands_[1]->name_;
  riscv_instr += "\n";
  return riscv_instr;
}

std::string UnaryRiscvInst::print() {
  assert(this->operands_.size() == 1);
  std::string riscv_instr = "\t";
  riscv_instr += instr_id2riscv[this->op_id_];
  riscv_instr += "\t";
  riscv_instr += this->name_;
  riscv_instr += ", ";
  riscv_instr += this->operands_[0]->name_;
  riscv_instr += "\n";
  return riscv_instr;
}

// unfinished
std::string CallRiscvInst::print() {
  std::string riscv_instr = "\t";
  // 若干条push
  return riscv_instr;
}

std::string ICmpRiscvInstr::print() {
  std::string riscv_instr = "\t";
  // 注意：由于RISCV不支持全部的比较运算，因而需要根据比较条件对式子进行等价变换
  if (ICmpOpName.count(this->icmp_op_) == 0) {
    swap(this->operands_[0]->name_, this->operands_[1]->name_);
    this->icmp_op_ = static_cast<ICmpInst::ICmpOp>((int)this->icmp_op_ ^ 2);
  }
  riscv_instr += ICmpOpName.at(this->icmp_op_);
  riscv_instr += this->operands_[0]->name_;
  riscv_instr += ", ";
  riscv_instr += this->operands_[1]->name_;
  riscv_instr += ", ";
  riscv_instr += this->TrueLink_->name_;
  riscv_instr += "\n";
  return riscv_instr;
}

std::string FCmpRiscvInstr::print() {
  std::string riscv_instr = "\t";
  if (FCmpOpName.count(this->fcmp_op_) == 0) {
    swap(this->operands_[0]->name_, this->operands_[1]->name_);
    this->fcmp_op_ = static_cast<FCmpInst::FCmpOp>((int)this->fcmp_op_ ^ 2);
  }
  riscv_instr += FCmpOpName.at(this->fcmp_op_);
  riscv_instr += this->operands_[0]->name_;
  riscv_instr += ", ";
  riscv_instr += this->operands_[1]->name_;
  riscv_instr += ", ";
  riscv_instr += this->TrueLink_->name_;
  riscv_instr += "\n";
  return riscv_instr;
}

std::string StoreRiscvInst::print() {
  std::string riscv_instr = "\t";
  if (this->type.tid_ == Type::FloatTyID)
    riscv_instr += "fsw\t";
  else
    riscv_instr += "sw\t";
  riscv_instr += this->operands_[0]->name_;
  riscv_instr += ", ";
  if (this->shift_)
    riscv_instr += std::to_string(this->shift_);
  riscv_instr += "(" + this->operands_[1]->name_ + ")";
  riscv_instr += "\n";
  return riscv_instr;
}

std::string LoadRiscvInst::print() {
  std::string riscv_instr = "\t";
  if (this->type.tid_ == Type::FloatTyID)
    riscv_instr += "flw\t";
  else
    riscv_instr += "lw\t";
  riscv_instr += this->operands_[0]->name_;
  riscv_instr += ", ";
  if (this->shift_)
    riscv_instr += std::to_string(this->shift_);
  riscv_instr += "(" + this->operands_[1]->name_ + ")";
  riscv_instr += "\n";
  return riscv_instr;
}
