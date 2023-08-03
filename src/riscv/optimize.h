#ifndef OPTIMIZEH
#define OPTIMIZEH

#include "riscv.h"
#include "ir.h"

// 进行数据流的优化
// 在此之前先分配各寄存器
// 可选
void OptimizeBlock();
#endif // !OPTIMIZEH