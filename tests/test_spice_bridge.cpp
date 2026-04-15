#include "flux/compiler/netlist_generator.h"
#include "flux/compiler/ast.h"
#include <iostream>
#include <memory>

int main() {
    using namespace Flux;
    NetlistGenerator gen;
    
    // 1. Test Fixed to SPICE
    auto fix = std::make_unique<FixedExprAST>(5.5, 32, 16);
    std::cout << "Fixed(5.5) -> SPICE: " << gen.expressionToSpice(fix.get()) << std::endl;
    
    // 2. Test If to SPICE
    auto cond = std::make_unique<BinaryExprAST>(
        static_cast<int>(TokenType::tok_equal),
        std::make_unique<NumberExprAST>(1.0, ""),
        std::make_unique<NumberExprAST>(1.0, "")
    );
    auto ifexpr = std::make_unique<IfExprAST>(
        std::move(cond),
        std::make_unique<NumberExprAST>(5.0, ""),
        std::make_unique<NumberExprAST>(0.0, "")
    );
    std::cout << "If(1==1) -> SPICE: " << gen.expressionToSpice(ifexpr.get()) << std::endl;
    
    return 0;
}
