#include "regalloc.h"
#include "instruction.h"

int IntRegID = 8, FloatRegID = 0; // 测试阶段使用
// 寄存器堆分配工作

// 根据输入的寄存器的名字`reg`返回相应的寄存器类指针。
// TODO:
// 需要涉及sp、fp、a0-a7（整数函数参数）、fa0-fa7（浮点函数参数）等寄存器。
Register *NamefindReg(std::string reg) {
  if (reg[0] == 'a') {
    return new Register(Register::RegType::Int,
                        10 + std::atoi(reg.substr(1).c_str())); // a0 is x10
  } else if (reg.substr(0, 2) == "fa") {
    return new Register(Register::RegType::Float,
                        std::atoi(reg.substr(2).c_str()));
  } else if (reg == "sp") {
    return new Register(Register::RegType::Int, 2); // sp is x2
  } else {
    std::cout << "FAIL REG " << reg << "\n";
    assert(false);
  }
}

Type *getStoreTypeFromRegType(RiscvOperand *riscvReg) {
  return riscvReg->getType() == RiscvOperand::OpTy::FloatReg
             ? new Type(Type::TypeID::FloatTyID)
             : new Type(Type::TypeID::IntegerTyID);
}

RiscvOperand *RegAlloca::findReg(Value *val, RiscvBasicBlock *bb,
                                 RiscvInstr *instr, int inReg, int load) {
  if (curReg.count(val))
    return curReg[val];
  // 目前下面是一个没有考虑任何寄存器分配的工作，认为所有的变量都是寄存器存储，所有值可以直接使用的
  if (val->type_->tid_ == Type::IntegerTyID ||
      val->type_->tid_ == Type::PointerTyID) {
    ++IntRegID;
    if (IntRegID > 29)
      IntRegID = 8;
    RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, IntRegID));
    setPositionReg(val, cur);
  } else {
    assert(val->type_->tid_ == Type::TypeID::FloatTyID);
    ++FloatRegID;
    RiscvFloatReg *cur =
        new RiscvFloatReg(new Register(Register::Float, FloatRegID));
    setPositionReg(val, cur);
  }

  // If value
  auto mem_addr = findMem(val);
  if (mem_addr != nullptr && load) {
    auto current_reg = curReg[val];
    auto load_type = getStoreTypeFromRegType(current_reg);

    bb->addInstrBefore(new LoadRiscvInst(load_type, current_reg, mem_addr, bb),
                       instr);
  }

  return curReg[val];

  /*下面的代码是模拟一个大致结构
      // 首先找 空闲寄存器

      if (inReg) {
      // 如果找不到，需要到对应的bb处对应指令发射一条push指令或lw、sw指令
      }
  */
}

RiscvOperand *RegAlloca::findMem(Value *val) {
  if (pos.count(val) == 0 && val->name_[0] != 0) {
    std::cerr << "[Warning] Value " << std::hex << val << " (" << val->name_
              << ")'s memory map not found." << std::endl;
  }
  // assert(pos.count(val) == 1);
  if (pos.find(val) == pos.end())
    return nullptr;
  return pos[val];
}

RiscvOperand *RegAlloca::findNonuse(RiscvBasicBlock *bb, RiscvInstr *instr) {
  ++IntRegID;
  RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, ++IntRegID));
  return cur;
}

void RegAlloca::setPosition(Value *val, RiscvOperand *riscvVal) {
  if (pos.find(val) != pos.end()) {
    std::cerr << "[Warning] Trying overwriting memory address map of value "
              << std::hex << val << " (" << val->name_ << ") [" << riscvVal
              << " -> " << pos[val] << "]" << std::endl;
    // std::terminate();
  }
  pos[val] = riscvVal;
}

RiscvOperand *RegAlloca::findSpecificReg(Value *val, std::string RegName,
                                         RiscvBasicBlock *bb,
                                         RiscvInstr *instr) {
  if (curReg.find(val) != curReg.end())
    writeback(curReg[val], bb, instr);
  Register *reg = NamefindReg(RegName);
  RiscvOperand *retOperand = nullptr;
  if (reg->regtype_ == reg->Int)
    retOperand = new RiscvIntReg(reg);
  else if (reg->regtype_ == reg->Float)
    retOperand = new RiscvFloatReg(reg);
  else
    throw "Unknown register type in findSpecificReg().";
  return retOperand;
}

void RegAlloca::setPositionReg(Value *val, RiscvOperand *riscvReg) {
  curReg[val] = riscvReg;
  regPos[riscvReg] = val;
}

RiscvInstr *RegAlloca::writeback(RiscvOperand *riscvReg, RiscvBasicBlock *bb,
                                 RiscvInstr *instr) {
  Value *value = findRegVal(riscvReg);
  if (value == nullptr)
    return nullptr; // Value not found in map
  RiscvOperand *mem_addr = findMem(value);
  if (mem_addr == nullptr)
    return nullptr; // Maybe an immediate value

  auto store_type = getStoreTypeFromRegType(riscvReg);
  auto store_instr = new StoreRiscvInst(store_type, riscvReg, mem_addr, bb);

  // Write store instruction
  if (instr != nullptr)
    bb->addInstrBefore(store_instr, instr);
  else
    bb->addInstrBack(store_instr);

  // Erase map info
  regPos.erase(riscvReg);
  curReg.erase(value);

  return store_instr;
}

Value *RegAlloca::findRegVal(RiscvOperand *riscvReg) {
  if (regPos.find(riscvReg) == regPos.end())
    return nullptr;
  return regPos[riscvReg];
}
