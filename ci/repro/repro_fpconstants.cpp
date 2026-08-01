// Minimal LLVM-only reproducer for the Windows FPConstants teardown crash.
//
// Links ONLY the prebuilt LLVM static libs (no flux code). Mimics the
// validate_examples loop in cli/minimal.cpp: create an LLVMContext, register
// MANY distinct double constants via ConstantFP::get (populating
// LLVMContextImpl's FPConstants DenseMap<APFloat, unique_ptr<ConstantFP>>),
// driving the map through growth/rehash, then destroy the context
// (~LLVMContextImpl -> FPConstants::clear()).
//
// flux.exe injects the full standard library (hundreds of distinct double
// literals) into a fresh context on every compile, which forces FPConstants
// to rehash many times; a plain "10 distinct keys" repro never grew the map
// and passed, so this version mirrors the stdlib's key diversity instead.
//
// If this crashes (0xC0000005 in llvm::APFloat::Storage::~Storage), the bug
// is inside the prebuilt LLVM libs. If it passes, the bug is in flux's usage.
#include <cstdio>

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

static double bitmix(unsigned seed)
{
    uint64_t x = seed;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return static_cast<double>(static_cast<int64_t>(x >> 1)) / 1e17;
}

int main()
{
    // ~600 distinct double literals per context (stdlib-scale diversity),
    // inserted via IRBuilder as in flux's codegen, plus direct ConstantFP::get.
    for (int i = 0; i < 200; ++i)
    {
        llvm::LLVMContext ctx;
        llvm::Module mod("validate_1", ctx);
        llvm::Type* dbl = llvm::Type::getDoubleTy(ctx);
        llvm::IRBuilder<> builder(ctx);
        llvm::FunctionType* fnTy = llvm::FunctionType::get(dbl, {}, false);
        llvm::Function* fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, "main", mod);
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", fn);
        builder.SetInsertPoint(entry);
        for (int j = 0; j < 600; ++j)
        {
            double v = bitmix(1000u * static_cast<unsigned>(i) + static_cast<unsigned>(j));
            llvm::Value* c = llvm::ConstantFP::get(dbl, v);
            (void)c;
            llvm::Value* sum = builder.CreateFAdd(c, c, "acc");
            (void)sum;
        }
        // destroy context -> ~LLVMContextImpl -> FPConstants::clear()
    }
    std::printf("repro OK\n");
    return 0;
}
