#include "regalloc.h"

int IntRegID = 0, FloatRegID = 0; // 测试阶段使用
const std::map<std::string, Register *> findReg;
// 寄存器堆分配工作
