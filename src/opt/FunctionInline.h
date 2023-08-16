#ifndef FUNCTIONINLINEH
#define FUNCTIONINLINEH
#include "BasicOperation.h"

class FunctionInline : public Optimization {
    void findRecursiveFunction();
    void buildCallerGraph();
    std::map<Function *, std::set<Function *>> FunctionCall;
    std::set<Function *> recursiveFunc;
    void inlineFunc(CallInst *call);

public:
    FunctionInline(Module *m) : Optimization(m) {}
    void execute();
};


#endif // !FUNCTIONINLINEH