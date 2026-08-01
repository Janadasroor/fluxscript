// Minimal LLVM-only reproducer for the Windows FPConstants teardown crash.
//
// Links ONLY the prebuilt LLVM static libs (no flux code). Mimics the
// validate_examples loop in cli/minimal.cpp: create an LLVMContext, register
// double constants via ConstantFP::get (populating LLVMContextImpl's
// FPConstants DenseMap<APFloat, unique_ptr<ConstantFP>>), then destroy the
// context (~LLVMContextImpl -> FPConstants::clear()).
//
// If this crashes (0xC0000005 in llvm::APFloat::Storage::~Storage), the bug
// is inside the prebuilt LLVM libs. If it passes, the bug is in flux's usage.
#include <cstdio>

#include <llvm/IR/ConstantFP.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

int main()
{
    const double vals[] = {0.0, 1.0, 0.5, 2.5, 3.14159, 2.71828, 1e-9, 1e9, -0.0, 12345.678};
    for (int i = 0; i < 200; ++i)
    {
        llvm::LLVMContext ctx;
        llvm::Module mod("repro", ctx);
        llvm::Type* dbl = llvm::Type::getDoubleTy(ctx);
        for (int j = 0; j < 50; ++j)
            llvm::ConstantFP::get(dbl, vals[(i + j) % 10]);
        // destroy context -> ~LLVMContextImpl -> FPConstants::clear()
    }
    std::printf("repro OK\n");
    return 0;
}
