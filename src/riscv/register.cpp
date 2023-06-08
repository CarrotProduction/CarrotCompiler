#include "register.h"
#include "ir.h"

// 初始化寄存器堆
void InitRegFile() {
  // 整型
  for (int i = 0; i < REG_NUMBER; i++) {
    std::string cur = "r" + std::to_string(i);
    Int.push_back(Register(&Type(Type::IntegerTyID), Register::RegType::UND, cur));
  }
  // 浮点
  for (int i = 0; i < REG_NUMBER; i++) {
    std::string cur = "f" + std::to_string(i);
    Float.push_back(Register(&Type(Type::FloatTyID), Register::RegType::UND, cur));
  }
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
