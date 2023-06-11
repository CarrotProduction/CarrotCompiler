#include "register.h"
#include "ir.h"

// 初始化寄存器堆
// https://blog.csdn.net/m0_47329175/article/details/127160108 对照此进行寄存器分配
void InitRegFile() {
  // 整型
  for (int i = 0; i < REG_NUMBER; i++) {
    std::string cur = "x" + std::to_string(i);
    Register::RegType curRegType = Register::RegType::UND;
    Int.push_back(Register(&Type(Type::IntegerTyID), Register::RegType::UND, cur));
  }
  // 浮点
  for (int i = 0; i < REG_NUMBER; i++) {
    std::string cur = "f" + std::to_string(i);
    Float.push_back(Register(&Type(Type::FloatTyID), Register::RegType::UND, cur));
  }
}

std::string Register::print() {
  // 浮点直接输出
  if (this->type_->tid_ == Type::FloatTyID)
    return this->name_;
  // 整型各类输出
  if (this->name_ == "x0")
    return "zero";
  if (this->name_ == "x1")
    return "ra";
  if (this->name_ == "x2")
    return "sp";
  if (this->name_ == "x3")
    return "gp";
  if (this->name_ == "x4")
    return "tp";
  if (this->name_ == "x8")
    return "fp";
  if (this->name_ == "x9")
    return "s1";
  int num = atoi(this->name_.substr(1).c_str());
  if (num >= 5 && num <= 7)
    return "t" + std::to_string(num - 5);
  if (num >= 10 && num <= 17)
    return "a" + std::to_string(num - 10);
  if (num >= 18 && num <= 27)
    return "s" + std::to_string(num - 16);
  return "t" + std::to_string(num - 25);
}

// 优化后的映射
void OptimizedAddrMapping() {

}

// 无优化映射
void NormalAddrMapping() {

}

void addrMapping(int op) {
  InitRegFile();
  if (op)
    OptimizedAddrMapping();
  else
    NormalAddrMapping();
}
