#ifndef COMBINEINSTRH
#define COMBINEINSTRH
#include "opt.h"

class CombineInstr : public Optimization {

public:
    CombineInstr(Module *m) : Optimization(m) {}
    void execute();
    void checkBlock(BasicBlock *bb);
};

#endif // !COMBINEINSTRH