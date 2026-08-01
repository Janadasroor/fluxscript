// Minimal LLVM-only reproducer for the Windows FPConstants teardown crash.
//
// Links ONLY the prebuilt LLVM static libs (no flux code). Mimics what
// flux.exe does for the FIRST validate example:
//   - inject ~150 extern function decls + prototypes (stdlib)
//   - create MANY distinct double constants via ConstantFP::get
//   - build functions with real bodies (string globals, calls, vector ops)
//   - run verifyModule
//   - destroy context -> ~LLVMContextImpl -> FPConstants::clear()
//
// If this crashes (0xC0000005 in llvm::APFloat::Storage::~Storage), the bug
// is inside the prebuilt LLVM libs. If it passes, the bug is in flux's usage.
#include <cstdio>
#include <string>
#include <vector>

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

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

static void buildStdlibLikeModule(llvm::Module& mod, llvm::LLVMContext& ctx, int iter)
{
    llvm::Type* dbl = llvm::Type::getDoubleTy(ctx);
    llvm::IRBuilder<> builder(ctx);

    // ~150 extern decls like injectStandardLibrary
    for (int i = 0; i < 150; ++i) {
        std::string name = "flux_extern_" + std::to_string(i);
        llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
        llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, mod);
    }

    // prototype-style decls (sin, cos, ...)
    for (int i = 0; i < 30; ++i) {
        std::string name = "stdlib_fn_" + std::to_string(i);
        llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl}, false);
        llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, mod);
    }

    // A handful of real functions with bodies: globals, calls, vector ops
    for (int f = 0; f < 20; ++f) {
        llvm::FunctionType* fnTy = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
        llvm::Function* fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage,
                                                    "fn_" + std::to_string(f) + "_" + std::to_string(iter), mod);
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", fn);
        builder.SetInsertPoint(entry);

        llvm::Value* a = fn->getArg(0);
        llvm::Value* b = fn->getArg(1);

        // string global + bitcast (println pattern)
        llvm::Value* strp = builder.CreateGlobalString("flux test string", "str");
        llvm::Value* asInt = builder.CreatePtrToInt(strp, llvm::Type::getInt64Ty(ctx));
        llvm::Value* asDbl = builder.CreateBitCast(asInt, dbl, "strptr_double");

        // call an extern
        llvm::Function* callee = mod.getFunction("stdlib_fn_0");
        builder.CreateCall(callee, {a, asDbl});

        // vector ops (complex pattern)
        llvm::Value* cv = llvm::PoisonValue::get(llvm::VectorType::get(dbl, 2, false));
        llvm::Value* zi = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);
        llvm::Value* oi = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 1);
        cv = builder.CreateInsertElement(cv, a, zi, "real_ins");
        cv = builder.CreateInsertElement(cv, b, oi, "imag_ins");

        // many distinct double constants + arithmetic
        llvm::Value* acc = llvm::ConstantFP::get(dbl, 0.0);
        for (int j = 0; j < 300; ++j) {
            double v = bitmix(100000u * static_cast<unsigned>(iter) + static_cast<unsigned>(f) * 1000u +
                              static_cast<unsigned>(j));
            llvm::Value* c = llvm::ConstantFP::get(dbl, v);
            acc = builder.CreateFAdd(acc, c, "acc");
        }
        builder.CreateRet(acc);
    }

    // verifyModule like compileToIR does
    std::string verifyError;
    llvm::raw_string_ostream verifyOS(verifyError);
    (void)llvm::verifyModule(mod, &verifyOS);
}

int main()
{
    for (int i = 0; i < 200; ++i)
    {
        llvm::LLVMContext ctx;
        llvm::Module mod("validate_1", ctx);
        buildStdlibLikeModule(mod, ctx, i);
        // destroy context -> ~LLVMContextImpl -> FPConstants::clear()
    }
    std::printf("repro OK\n");
    return 0;
}
