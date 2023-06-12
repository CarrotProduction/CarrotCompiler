#include "riscv.h"
#include "instruction.h"
#include "ir.h"

class RiscvBuilder {

public:
  Module *m;
  RiscvModule *rm;

  // zext 删掉
  // alloca 删掉，在寄存器分配完成
  
};