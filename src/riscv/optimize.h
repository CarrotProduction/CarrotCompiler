#include "riscv.h"
#include "ir.h"

// 进行数据流的优化
// 在此之前先分配各寄存器
// 可选
void OptimizeBlock();

// 将IR basic block转化为继承类的riscv basic block并对每个块进行标号
// 必做
int LableCount = 0;
void TransferBasicBlock();