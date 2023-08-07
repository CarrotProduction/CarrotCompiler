#include "regalloc.h"
#include "instruction.h"
#include "riscv.h"

int IntRegID = 32, FloatRegID = 32; // 测试阶段使用

// 寄存器堆分配工作

// 根据输入的寄存器的名字`reg`返回相应的寄存器类指针。
Register *NamefindReg(std::string reg) {
  if (reg.size() > 4)
    return nullptr;

  Register *reg_to_ret = new Register(Register::Int, 0);

  // Check if int registers
  for (int i = 0; i < 32; i++) {
    reg_to_ret->rid_ = i;
    if (reg_to_ret->print() == reg)
      return reg_to_ret;
  }

  // Else then float registers
  reg_to_ret->regtype_ = reg_to_ret->Float;
  for (int i = 0; i < 32; i++) {
    reg_to_ret->rid_ = i;
    if (reg_to_ret->print() == reg)
      return reg_to_ret;
  }

  return nullptr;
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
                                 RiscvOperand *specified, bool direct) {
  val = this->DSU_for_Variable.query(val);
  bool isGVar = dynamic_cast<GlobalVariable *>(val) != nullptr;
  bool isAlloca = dynamic_cast<AllocaInst *>(val) != nullptr;

  // If there is no register allocated for value then get a new one
  if (specified != nullptr)
    setPositionReg(val, specified, bb, instr);
  else if (curReg.find(val) == curReg.end() || isAlloca ||
           val->is_constant()) { // Alloca and constant value is always unsafe.
    if (val->type_->tid_ != Type::FloatTyID) {
      ++IntRegID;
      if (IntRegID > 17)
        IntRegID = 10;
      RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, IntRegID));
      setPositionReg(val, cur, bb, instr);
    } else {
      assert(val->type_->tid_ == Type::TypeID::FloatTyID);
      ++FloatRegID;
      // if (FloatRegID == 10)
      //   FloatRegID += 8;
      if (FloatRegID > 17)
        FloatRegID = 10;
      RiscvFloatReg *cur =
          new RiscvFloatReg(new Register(Register::Float, FloatRegID));
      setPositionReg(val, cur, bb, instr);
    }
  } else if (!load)
    return curReg[val];

  // ! Though all registers are considered unsafe, there is no way
  // ! to writeback registers properly in findReg() for now.
  // ! Therefore unsafe part below is not being executed for now.
  // ! Maybe should consider using writeback() instead.
  // For now, all registers are considered unsafe thus registers should always
  // load from memory before using and save to memory after using.
  auto mem_addr =
      findMem(val, bb, instr, direct); // Value's direct memory address
  auto current_reg = curReg[val];      // Value's current register
  auto load_type = val->type_;
  if (load) {
    // Load before usage.
    if (mem_addr != nullptr) {
      bb->addInstrBefore(
          new LoadRiscvInst(load_type, current_reg, mem_addr, bb), instr);

    } else if (val->is_constant()) {
      // If value is a int constant, create a LI instruction.
      auto cval = dynamic_cast<ConstantInt *>(val);
      if (cval != nullptr)
        bb->addInstrBefore(new MoveRiscvInst(current_reg, cval->value_, bb),
                           instr);
      else if (dynamic_cast<ConstantFloat *>(val) != nullptr)
        bb->addInstrBefore(
            new MoveRiscvInst(current_reg, this->findMem(val), bb), instr);
      else {
        std::cerr << "[Warning] Trying to find a register for unknown type of "
                     "constant value which is not implemented for now."
                  << std::endl;
      }
    } else if (isAlloca) {
      bb->addInstrBefore(
          new BinaryRiscvInst(
              BinaryRiscvInst::ADDI, getRegOperand("fp"),
              new RiscvConst(static_cast<RiscvIntPhiReg *>(pos[val])->shift_),
              current_reg, bb),
          instr);
      // std::cerr << "[Debug] Get a alloca position <" << val->print() << ", "
      //           << static_cast<RiscvIntPhiReg *>(pos[val])->print()
      //           << "> into the register <" << current_reg->print() << ">"
      //           << std::endl;
    } else {
      std::cerr << "[Error] Unknown error in findReg()." << std::endl;
      std::terminate();
    }

    // Save after usage. (not being executed for now.)

    // if (mem_addr != nullptr)
    //   bb->addInstrAfter(
    //       new StoreRiscvInst(load_type, current_reg, mem_addr, bb), instr);
  }

  return current_reg;
}

RiscvOperand *RegAlloca::findMem(Value *val, RiscvBasicBlock *bb,
                                 RiscvInstr *instr, bool direct) {
  val = this->DSU_for_Variable.query(val);
  if (pos.count(val) == 0 && !val->is_constant()) {
    std::cerr << "[Warning] Value " << std::hex << val << " (" << val->name_
              << ")'s memory map not found." << std::endl;
  }
  bool isGVar = dynamic_cast<GlobalVariable *>(val) != nullptr;
  bool isPointer = val->type_->tid_ == val->type_->PointerTyID;
  bool isAlloca = dynamic_cast<AllocaInst *>(val) != nullptr;
  // All float constant considered as global variables for now.
  isGVar = isGVar || dynamic_cast<ConstantFloat *>(val) != nullptr;
  // Always loading global variable's address into t5 when execute findMem().
  if (isGVar) {
    if (bb == nullptr) {
      std::cerr << "[Warning] Trying to add global var addressing "
                   "instruction, but basic block pointer is null."
                << std::endl;
      return nullptr;
    }
    bb->addInstrBefore(
        new LoadAddressRiscvInstr(getRegOperand("t5"), pos[val]->print(), bb),
        instr);
    return new RiscvIntPhiReg("t5");
  }
  // If not loading pointer's address directly, then use indirect addressing.
  // Ignore alloca due to the instruction only being dealt by findReg()
  if (isPointer && !isAlloca && !direct) {
    if (bb == nullptr) {
      std::cerr << "[Warning] Trying to add indirect pointer addressing "
                   "instruction, but basic block pointer is null."
                << std::endl;
      return nullptr;
    }

    bb->addInstrBefore(new LoadRiscvInst(new Type(Type::PointerTyID),
                                         getRegOperand("t5"), pos[val], bb),
                       instr);
    return new RiscvIntPhiReg("t5");
  }
  // Cannot access to alloca's memory directly.
  else if (direct && isAlloca)
    return nullptr;

  if (pos.find(val) == pos.end())
    return nullptr;

  return pos[val];
}

RiscvOperand *RegAlloca::findMem(Value *val) {
  return findMem(val, nullptr, nullptr, true);
}

RiscvOperand *RegAlloca::findNonuse(Type *ty, RiscvBasicBlock *bb,
                                    RiscvInstr *instr) {
  if (ty->tid_ == Type::IntegerTyID || ty->tid_ == Type::PointerTyID) {
    ++IntRegID;
    if (IntRegID > 27)
      IntRegID = 9;
    RiscvIntReg *cur = new RiscvIntReg(new Register(Register::Int, IntRegID));
    return cur;
  } else {
    ++FloatRegID;
    if (FloatRegID >= 32)
      FloatRegID = 0;
    RiscvFloatReg *cur =
        new RiscvFloatReg(new Register(Register::Float, FloatRegID));
    return cur;
  }
}

void RegAlloca::setPosition(Value *val, RiscvOperand *riscvVal) {
  val = this->DSU_for_Variable.query(val);
  if (pos.find(val) != pos.end()) {
    std::cerr << "[Warning] Trying overwriting memory address map of value "
              << std::hex << val << " (" << val->name_ << ") ["
              << riscvVal->print() << " -> " << pos[val]->print() << "]"
              << std::endl;
    // std::terminate();
  }

  std::cerr << "[Debug] [RegAlloca] Map value <" << val->print()
            << "> to operand <" << riscvVal->print() << ">" << std::endl;

  pos[val] = riscvVal;
}

RiscvOperand *RegAlloca::findSpecificReg(Value *val, std::string RegName,
                                         RiscvBasicBlock *bb, RiscvInstr *instr,
                                         bool direct) {
  val = this->DSU_for_Variable.query(val);
  Register *reg = NamefindReg(RegName);
  RiscvOperand *retOperand = nullptr;

  if (reg->regtype_ == reg->Int)
    retOperand = new RiscvIntReg(reg);
  else if (reg->regtype_ == reg->Float)
    retOperand = new RiscvFloatReg(reg);
  else
    throw "Unknown register type in findSpecificReg().";
  return findReg(val, bb, instr, 0, 1, retOperand, direct);
}

void RegAlloca::setPositionReg(Value *val, RiscvOperand *riscvReg,
                               RiscvBasicBlock *bb, RiscvInstr *instr) {
  val = this->DSU_for_Variable.query(val);
  Value *old_val = getRegPosition(riscvReg);
  RiscvOperand *old_reg = getPositionReg(val);
  if (old_val != nullptr && old_val != val)
    writeback(riscvReg, bb, instr);
  if (old_reg != nullptr && old_reg != riscvReg)
    writeback(old_reg, bb, instr);
  setPositionReg(val, riscvReg);
}

void RegAlloca::setPositionReg(Value *val, RiscvOperand *riscvReg) {
  val = this->DSU_for_Variable.query(val);
  if (riscvReg->isRegister() == false) {
    std::cerr << "[Fatal error] Trying to map value " << std::hex << val
              << " to not a register operand." << std::endl;
    std::terminate();
  }

  std::cerr << "[Debug] Map register <" << riscvReg->print() << "> to value <"
            << val->print() << ">\n";

  curReg[val] = riscvReg;
  regPos[riscvReg] = val;
}

RiscvInstr *RegAlloca::writeback(RiscvOperand *riscvReg, RiscvBasicBlock *bb,
                                 RiscvInstr *instr) {
  Value *value = getRegPosition(riscvReg);
  if (value == nullptr)
    return nullptr; // Value not found in map

  value = this->DSU_for_Variable.query(value);

  // Erase map info
  regPos.erase(riscvReg);
  curReg.erase(value);

  RiscvOperand *mem_addr = findMem(value);

  if (mem_addr == nullptr)
    return nullptr; // Maybe an immediate value or dicrect accessing alloca

  auto store_type = value->type_;
  auto store_instr = new StoreRiscvInst(value->type_, riscvReg, mem_addr, bb);

  // Write store instruction
  if (instr != nullptr)
    bb->addInstrBefore(store_instr, instr);
  else
    bb->addInstrBack(store_instr);

  return store_instr;
}

RegAlloca::RegAlloca() {
  // fp 的保护单独进行处理
  savedRegister.push_back(getRegOperand("ra")); // 保护 ra
  // 保护 s1-s11
  // for(int i=1; i<=11; i++)
  //   savedRegister.push_back(getRegOperand("s"+std::to_string(i)));
  // 保护 fs0-fs11
  // for(int i=0; i<=11; i++)
  //   savedRegister.push_back(getRegOperand("fs"+std::to_string(i)));
}

RiscvInstr *RegAlloca::writeback(Value *val, RiscvBasicBlock *bb,
                                 RiscvInstr *instr) {
  auto reg = getPositionReg(val);
  return writeback(reg, bb, instr);
}

Value *RegAlloca::getRegPosition(RiscvOperand *reg) {
  if (regPos.find(reg) == regPos.end())
    return nullptr;
  return this->DSU_for_Variable.query(regPos[reg]);
}

RiscvOperand *RegAlloca::getPositionReg(Value *val) {
  val = this->DSU_for_Variable.query(val);
  if (curReg.find(val) == curReg.end())
    return nullptr;
  return curReg[val];
}

RiscvOperand *RegAlloca::findPtr(Value *val, RiscvBasicBlock *bb,
                                 RiscvInstr *instr) {
  val = this->DSU_for_Variable.query(val);
  if (ptrPos.find(val) == ptrPos.end()) {
    std::cerr << "[Fatal Error] Value's pointer position not found."
              << std::endl;
    std::terminate();
  }
  return ptrPos[val];
}

void RegAlloca::writeback_all(RiscvBasicBlock *bb, RiscvInstr *instr) {
  std::vector<RiscvOperand *> regs_to_writeback;
  for (auto p : regPos)
    regs_to_writeback.push_back(p.first);
  for (auto r : regs_to_writeback)
    writeback(r, bb, instr);
}

void RegAlloca::setPointerPos(Value *val, RiscvOperand *PointerMem) {
  val = this->DSU_for_Variable.query(val);
  assert(val->type_->tid_ == Type::TypeID::PointerTyID ||
         val->type_->tid_ == Type::TypeID::ArrayTyID);
  // std::cerr << "SET POINTER: " << val->name_ << "!" << PointerMem->print()
  //           << "\n";
  this->ptrPos[val] = PointerMem;
}