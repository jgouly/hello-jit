#include <cstdio>
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Support/IRReader.h"
#include <llvm/DerivedTypes.h>
#include <llvm/GlobalVariable.h>
#include "llvm/Analysis/Passes.h"
#include "llvm/PassManager.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/Verifier.h"

using namespace llvm;

LLVMContext *C;
Module *M;
ExecutionEngine *EE;
FunctionPassManager *TheFPM;

int stack[999];
int *sp;

Module *load_module(std::string file)
{
	std::string error;
  Module* jit;
	SMDiagnostic Err;
	jit = ParseIRFile(file, Err, *C);
	return jit;
}

int i = 1;
extern "C" {
	void push1()
	{
		*sp++ = i++;
	}
}

void initEEGlobals(bool funcs = true)
{
	EE->addGlobalMapping(M->getNamedGlobal("stack"), stack);
	EE->addGlobalMapping(M->getNamedGlobal("sp"), sp);
	if(funcs)
	{
		EE->addGlobalMapping(M->getFunction("push1"), (void*)push1);
	}
}


void setup()
{
	InitializeNativeTarget();
	C = &getGlobalContext();
	M = load_module("jit.ll");

	std::string ErrStr;
	EE = EngineBuilder(M).setErrorStr(&ErrStr).create();
	if (!EE) {
    fprintf(stderr, "Could not create ExecutionEngine: %s\n", ErrStr.c_str());
    exit(1);
  }

	TheFPM = new FunctionPassManager(M);
  // Set up the optimizer pipeline.  Start with registering info about how the
  // target lays out data structures.
  TheFPM->add(new TargetData(*EE->getTargetData()));
  // Provide basic AliasAnalysis support for GVN.
 	TheFPM->add(createBasicAliasAnalysisPass());
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->add(createInstructionCombiningPass());
  // Reassociate expressions.
  TheFPM->add(createReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->add(createCFGSimplificationPass());

  TheFPM->doInitialization();

	initEEGlobals(true);
	sp = stack;
}


void loadInlineJit()
{
	Module *M2 = load_module("inline-jit.ll");
	for(Module::iterator i = M2->begin(), e = M2->end();
		i != e; ++i)
	{
		Function *F = M->getFunction(i->getName());
		ValueToValueMapTy VMap;
		SmallVector<ReturnInst*, 8> Returns; 
		CloneFunctionInto(F, i, VMap, true, Returns);
	}
	EE->clearGlobalMappingsFromModule(M);
	initEEGlobals(false);	
}

template<typename Ret>
struct NativeFunction
{
	Ret (*FP)();
	NativeFunction(void* ptr)
	{
		FP = (Ret (*)()) (intptr_t) ptr;
	}
	Ret operator ()()
	{
		return FP();
	}
};

struct Proc 
{
	bool dirty;
	Proc() : dirty(false) {}
	virtual bool is_compiled()
	{
		return false;
	}

	virtual int exec()
	{
		*sp++ = -10;
		return *sp;
	}
	void is_dirty() { dirty = true; }	
};

struct CompiledProc : public Proc
{
	NativeFunction<int> *func;
	std::string name;
	CompiledProc(std::string n) : name(n) {
		func = new NativeFunction<int>(EE->getPointerToFunction(M->getFunction(n)));
	}
	
	int exec()
	{
		if(dirty) 
			func = new NativeFunction<int>(EE->getPointerToFunction(M->getFunction(name)));
		return (*func)();
	}

	bool is_compiled()
	{
		return true;
	}
};

int main()
{
	setup();
	
	for(Module::iterator i = M->begin(), e = M->end();
		i != e; ++i)
	{
			verifyFunction(*i);
			TheFPM->run(*i);
	}

	Proc *x = new Proc();
	Proc *x1 = new CompiledProc("push1");
	x->exec();
	x1->exec();
	x1->exec();
	printf("TOS = %d\n", *(sp-1));
	
	loadInlineJit();
	x1->is_dirty();
	
	x1->exec();
	printf("TOS = %d\n", *(sp-1));
	
	for(int i = 0; i < 5; i++)
	{
		printf("stack[%d] = %d\n", i, stack[i]);
	}
	M->dump();
}
