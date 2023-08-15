#ifndef BACKENDH
#define BACKENDH
#include "instruction.h"
#include "ir.h"
#include "optimize.h"
#include "regalloc.h"
#include "riscv.h"
#include <memory.h>

// 建立IR到RISCV指令集的映射
const extern std::map<Instruction::OpID, RiscvInstr::InstrType> toRiscvOp;

extern int LabelCount;
extern std::map<BasicBlock *, RiscvBasicBlock *> rbbLabel;
extern std::map<Function *, RiscvFunction *> functionLabel;

// 下面的函数仅为一个basic
// block产生一个标号，指令集为空，需要使用总控程序中具体遍历该bb才会产生内部指令
RiscvBasicBlock *createRiscvBasicBlock(BasicBlock *bb = nullptr);
RiscvFunction *createRiscvFunction(Function *foo = nullptr);
std::string toLabel(int ind);
int calcTypeSize(Type *ty);

// 总控程序
class RiscvBuilder {
private:
  void initializeRegisterFile();

public:
  RiscvBuilder() {
    rm = new RiscvModule();
    initializeRegisterFile();
  }
  RiscvModule *rm;
  // phi语句的合流：此处建立一个并查集DSU_for_Variable维护相同的变量。
  // 例如，对于if (A) y1=do something else y2=do another thing. Phi y3 y1, y2
  std::string buildRISCV(Module *m);

  // 下面的语句是需要生成对应riscv语句
  // Zext语句零扩展，因而没有必要
  // ZExtRiscvInstr createZextInstr(ZextInst *instr);
  // void resolveLibfunc(RiscvFunction *foo);
  BinaryRiscvInst *createBinaryInstr(RegAlloca *regAlloca,
                                     BinaryInst *binaryInstr,
                                     RiscvBasicBlock *rbb);
  UnaryRiscvInst *createUnaryInstr(RegAlloca *regAlloca, UnaryInst *unaryInstr,
                                   RiscvBasicBlock *rbb);
  std::vector<RiscvInstr *> createStoreInstr(RegAlloca *regAlloca,
                                             StoreInst *storeInstr,
                                             RiscvBasicBlock *rbb);
  std::vector<RiscvInstr *> createLoadInstr(RegAlloca *regAlloca,
                                            LoadInst *loadInstr,
                                            RiscvBasicBlock *rbb);
  ICmpRiscvInstr *createICMPInstr(RegAlloca *regAlloca, ICmpInst *icmpInstr,
                                  BranchInst *brInstr, RiscvBasicBlock *rbb);
  ICmpRiscvInstr *createICMPSInstr(RegAlloca *regAlloca, ICmpInst *icmpInstr,
                                   RiscvBasicBlock *rbb);
  RiscvInstr *createFCMPInstr(RegAlloca *regAlloca, FCmpInst *fcmpInstr,
                              RiscvBasicBlock *rbb);
  SiToFpRiscvInstr *createSiToFpInstr(RegAlloca *regAlloca,
                                      SiToFpInst *sitofpInstr,
                                      RiscvBasicBlock *rbb);
  FpToSiRiscvInstr *createFptoSiInstr(RegAlloca *regAlloca,
                                      FpToSiInst *fptosiInstr,
                                      RiscvBasicBlock *rbb);
  CallRiscvInst *createCallInstr(RegAlloca *regAlloca, CallInst *callInstr,
                                 RiscvBasicBlock *rbb);
  RiscvBasicBlock *transferRiscvBasicBlock(BasicBlock *bb, RiscvFunction *foo);
  ReturnRiscvInst *createRetInstr(RegAlloca *regAlloca, ReturnInst *returnInstr,
                                  RiscvBasicBlock *rbb, RiscvFunction *rfoo);
  BranchRiscvInstr *createBrInstr(RegAlloca *regAlloca, BranchInst *brInstr,
                                  RiscvBasicBlock *rbb);
  RiscvInstr *solveGetElementPtr(RegAlloca *regAlloca, GetElementPtrInst *instr,
                                 RiscvBasicBlock *rbb);

  /**
   * 在返回语句前插入必要的语句。
   */
  void initRetInstr(RegAlloca *regAlloca, RiscvInstr *returnInstr,
                    RiscvBasicBlock *rbb, RiscvFunction *foo);
};
#endif // !BACKENDH