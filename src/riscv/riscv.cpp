#include "riscv.h"

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
    {Instruction::Call, "call"}
}; // Instruction from opid to string
const std::map<ICmpInst::ICmpOp, std::string> ICmpInst::ICmpOpName = {
    {ICmpInst::ICmpOp::ICMP_EQ, "eq"},  {ICmpInst::ICmpOp::ICMP_NE, "ne"},
    {ICmpInst::ICmpOp::ICMP_UGT, "hi"}, {ICmpInst::ICmpOp::ICMP_UGE, "cs"},
    {ICmpInst::ICmpOp::ICMP_ULT, "cc"}, {ICmpInst::ICmpOp::ICMP_ULE, "ls"},
    {ICmpInst::ICmpOp::ICMP_SGT, "gt"}, {ICmpInst::ICmpOp::ICMP_SGE, "ge"},
    {ICmpInst::ICmpOp::ICMP_SLT, "lt"}, {ICmpInst::ICmpOp::ICMP_SLE, "le"}};
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

std::string CallRiscvInst::print() {
  std::string riscv_instr = "\t";
  // 若干条push
  return riscv_instr;
}

std::string StoreRiscvInst::print() {
  std::string riscv_instr = "\t";
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
  riscv_instr += "lw\t";
  riscv_instr += this->operands_[0]->name_;
  riscv_instr += ", ";
  if (this->shift_)
    riscv_instr += std::to_string(this->shift_);
  riscv_instr += "(" + this->operands_[1]->name_ + ")";
  riscv_instr += "\n";
  return riscv_instr;
}
