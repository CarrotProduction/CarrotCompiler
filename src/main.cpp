#include <fstream>
#include <iostream>
#include "genIR.h"
#include "ast.h"
#include "define.h"

extern unique_ptr<CompUnitAST> root;
extern int yyparse();
extern FILE *yyin;

int main(int argc, char **argv) {
    // TODO: argument parser
    assert(argc >= 2);
    char *filename;
    bool print_ir = false;
    if(argc == 2) {
        filename = argv[1];
    }else if(argc == 3){
        print_ir = true;
        filename = argv[2];
    }
    yyin = fopen(filename, "r");
    if (yyin == nullptr) {
        std::cout << "yyin open" << filename << "failed" << std::endl;
        return -1;
    }

    // Frontend parser
    yyparse();

    // Generate IR from AST
    GenIR genIR;
    root -> accept(genIR);
    std::unique_ptr<Module> m = genIR.getModule();

    // Run IR optimization
    // TODO

    // Print IR result
    const std::string IR = m -> print();
    if (print_ir){
        std::cout << IR << std::endl;
    }

    // Generate object file
    // TODO
    
    return 0;
}
