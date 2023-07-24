#include "regalloc.h"

int IntRegID = 0, FloatRegID = 0; // 测试阶段使用
// 寄存器堆分配工作

// 根据输入的寄存器的名字`reg`返回相应的寄存器类指针。
Register * findReg(string reg) {
    if(reg == "a0") {
        return new Register(Register::RegType::Int, 10);    // a0 is x10
    }
    else if(reg == "f0") {
        return new Register(Register::RegType::Float, 0);

    }
    else if(reg == "fp") {
        return new Register(Register::RegType::Int, 10);    // fp is x8
    }
    else {
        assert(true);
    }
}
