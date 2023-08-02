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
                        std::atoi(reg.substr(2).c_str()) + 10);
  } else if (reg == "sp") {
    return new Register(Register::RegType::Int, 2); // sp is x2
  } else if (reg == "ra") {
    return new Register(Register::RegType::Int, 1); // ra is x1
  } else if (reg == "t5") {
    return new Register(Register::RegType::Int, 30);
  } else {
    std::cout << "FAIL REG " << reg << "\n";
    assert(false);
  }
}

RiscvOperand *getRegOperand(std::string reg) {
  auto cur_reg = NamefindReg(reg);
  auto reg_type = cur_reg->regtype_;
  switch (reg_type) {
  case Register::Float:
    return new RiscvFloatReg(cur_reg);
  case Register::Int:
    return new RiscvIntReg(cur_reg);
  default:
    std::cerr << "[Fatal error] Not a register." << std::endl;
    std::terminate();
  }
}

Type *getStoreTypeFromRegType(RiscvOperand *riscvReg) {
  return riscvReg->getType() == RiscvOperand::OpTy::FloatReg
             ? new Type(Type::TypeID::FloatTyID)
             : new Type(Type::TypeID::IntegerTyID);
}

RiscvOperand *RegAlloca::findReg(Value *val, RiscvBasicBlock *bb,
                                 RiscvInstr *instr, int inReg, int load,
                                 RiscvOperand *specified) {
  val = this->DSU_for_Variable.query(val);
  bool isGVar = dynamic_cast<GlobalVariable *>(val) != nullptr;

  // If there is no register allocated for value then get a new one
  if (specified != nullptr)
    setPositionReg(val, specified, bb, instr);
  else if (curReg.find(val) == curReg.end()) {
    if (val->type_->tid_ != Type::FloatTyID) {
      ++IntRegID;
      if (IntRegID > 29)
        IntRegID = 5;
      RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, IntRegID));
      writeback(cur, bb);
      setPositionReg(val, cur, bb, instr);
    } else {
      assert(val->type_->tid_ == Type::TypeID::FloatTyID);
      ++FloatRegID;
      RiscvFloatReg *cur =
          new RiscvFloatReg(new Register(Register::Float, FloatRegID));
      writeback(cur, bb);
      setPositionReg(val, cur, bb, instr);
    }
  } else
    return curReg[val];

  // ! Though all registers are considered unsafe, there is no way
  // ! to writeback registers properly in findReg() for now.
  // ! Therefore unsafe part below is not being executed for now.
  // ! Maybe should consider using writeback() instead.
  // For now, all registers are considered unsafe thus registers should always
  // load from memory before using and save to memory after using.
  auto mem_addr = findMem(val, bb, instr); // Value's memory address
  auto current_reg = curReg[val];          // Value's current register
  auto load_type = getStoreTypeFromRegType(current_reg);
  if (load) {
    // Load before usage.
    if (mem_addr != nullptr)
      bb->addInstrBefore(
          new LoadRiscvInst(load_type, current_reg, mem_addr, bb), instr);
    else if (val->is_constant()) {
      // If value is a int constant, create a LI instruction.
      auto cval = dynamic_cast<ConstantInt *>(val);
      if (cval != nullptr)
        bb->addInstrBefore(new MoveRiscvInst(current_reg, cval->value_, bb),
                           instr);
      else {
        std::cerr << "[Warning] Trying to find a register for unknown type of "
                     "constant value which is not implemented for now."
                  << std::endl;
      }
    }

    // Save after usage. (not being executed for now.)

    // if (mem_addr != nullptr)
    //   bb->addInstrAfter(
    //       new StoreRiscvInst(load_type, current_reg, mem_addr, bb), instr);
  }

  return current_reg;
}

RiscvOperand *RegAlloca::findMem(Value *val, RiscvBasicBlock *bb,
                                 RiscvInstr *instr) {
  val = this->DSU_for_Variable.query(val);
  bool isGVar = dynamic_cast<GlobalVariable *>(val) != nullptr;
  if (pos.count(val) == 0 && !val->is_constant()) {
    std::cerr << "[Warning] Value " << std::hex << val << " (" << val->name_
              << ")'s memory map not found." << std::endl;
  }
  // Always loading global variable's address into t5 when execute findMem().
  if (isGVar) {
    if (bb == nullptr) {
      std::cerr << "[Fatal Error] Trying to add global var addressing "
                   "instruction, but basic block pointer is null."
                << std::endl;
      std::terminate();
    }
    bb->addInstrBefore(new LoadAddressRiscvInstr(pos[val], "t5", bb), instr);
    return new RiscvIntPhiReg(NamefindReg("t5"), 0);
  }
  // assert(pos.count(val) == 1);
  if (pos.find(val) == pos.end())
    return nullptr;
  return pos[val];
}

RiscvOperand *RegAlloca::findMem(Value *val) {
  return findMem(val, nullptr, nullptr);
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
  val = this->DSU_for_Variable.query(val);
  Register *reg = NamefindReg(RegName);
  RiscvOperand *retOperand = nullptr;

  if (reg->regtype_ == reg->Int)
    retOperand = new RiscvIntReg(reg);
  else if (reg->regtype_ == reg->Float)
    retOperand = new RiscvFloatReg(reg);
  else
    throw "Unknown register type in findSpecificReg().";
  return findReg(val, bb, instr, 0, 1, retOperand);
}

void RegAlloca::setPositionReg(Value *val, RiscvOperand *riscvReg,
                               RiscvBasicBlock *bb, RiscvInstr *instr) {
  Value *old_val = getRegPosition(riscvReg);
  if (old_val != nullptr && old_val != val)
    writeback(riscvReg, bb, instr);
  setPositionReg(val, riscvReg);
}

void RegAlloca::setPositionReg(Value *val, RiscvOperand *riscvReg) {
  val = this->DSU_for_Variable.query(val);
  if (riscvReg->isRegister() == false) {
    std::cerr << "[Fatal error] Trying to map value " << std::hex << val
              << " to not a register operand." << std::endl;
    std::terminate();
  }
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

RegAlloca::RegAlloca() {
  // savedRegister.push_back(getRegOperand("ra"));
}

Value *RegAlloca::getRegPosition(RiscvOperand *reg) {
  if (regPos.find(reg) == regPos.end())
    return nullptr;
  return DSU_for_Variable.query(regPos[reg]);
}

RiscvOperand *RegAlloca::findPtr(Value *val, RiscvBasicBlock *bb,
                                 RiscvInstr *instr) {
  val = this->DSU_for_Variable.query(val);
  if (dynamic_cast<GlobalVariable *>(val) != nullptr) {
    bb->addInstrBack(new LoadAddressRiscvInstr(new RiscvIntReg(NamefindReg("t5")), val->name_, bb));
    return new RiscvIntPhiReg(NamefindReg("t5"));
  }
  // std::cout << val->name_ << "\n";
  assert(ptrPos.count(val));
  RiscvOperand *PointerTo = ptrPos[val];
  // std::cout << PointerTo->print() << "\n";
  if (dynamic_cast<RiscvIntPhiReg *>(PointerTo) != nullptr) {
    // 栈上确定地址
    // std::cout << "TYPE:INT\n";
    // std::cout << dynamic_cast<RiscvIntPhiReg *>(PointerTo)->base_->print() << "\n";
    if (dynamic_cast<RiscvIntPhiReg *>(PointerTo)->base_->print() == "sp")
      return PointerTo;
  } else if (dynamic_cast<RiscvFloatPhiReg *>(PointerTo) != nullptr) {
    if (dynamic_cast<RiscvFloatPhiReg *>(PointerTo)->base_->print() == "sp")
      return PointerTo;
  }

  // 不确定地址，或者由寄存器保护的地址
  // 认为必须从内存取该指针
  bb->addInstrBack(new LoadRiscvInst(new Type(Type::IntegerTyID),
                                     getRegOperand("t5"), this->findMem(val), 
                                     bb));
  auto containedType = findPtrType(val->type_);
  if (containedType->tid_ == Type::FloatTyID)
    return new RiscvFloatPhiReg(NamefindReg("t5"));
  else
    return new RiscvIntPhiReg(NamefindReg("t5"));
}

void RegAlloca::setPointerPos(Value *val, RiscvOperand *PointerMem) {
  val = this->DSU_for_Variable.query(val);
  assert(val->type_->tid_ == Type::TypeID::PointerTyID ||
         val->type_->tid_ == Type::TypeID::ArrayTyID);
  // std::cout << "SET POINTER: " << val->name_ << "!" << PointerMem->print() << "\n";
  this->ptrPos[val] = PointerMem;
}