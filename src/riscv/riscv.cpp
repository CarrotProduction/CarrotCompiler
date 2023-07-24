#include "riscv.h"
#include "backend.h"
#include "ir.h"

const int REG_NUMBER = 32;

// 输出函数对应的全部riscv语句序列
// 由于一个函数可能有若干个出口，因而恢复现场的语句根据basic block
// 语句中的ret语句前面附带出现，因而不在此出现
std::string RiscvFunction::print() {
  std::string riscvInstr = this->name_ + ":\n"; // 函数标号打印
  riscvInstr += "\t\tPUSH\tra\n";
  riscvInstr += "\t\tPUSH\tfp\n";
  riscvInstr += "\t\tMV\tsp, fp\n";
  // 为函数局部变量腾出空间
  if (this->base_)
    riscvInstr += "\t\tADD\tsp, " + std::to_string(this->base_) + "\n";
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
