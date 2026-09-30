#include <fcntl.h>
#include <mach/mach.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <llvm-c/BitReader.h>
#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/Target.h>
typedef void *ref;

void *dlopen(const char *path, int mode) {
  (void)path;
  (void)mode;
  return (void *)1;
}
char *dlerror(void) {
  return "static LLVM shim";
}
#define MAP(s)                                                                                                         \
  if (!strcmp(name, #s))                                                                                               \
  return (void *)(uintptr_t)&s
void *dlsym(void *handle, const char *name) {
  (void)handle;
  MAP(LLVMInitializeAArch64TargetInfo);
  MAP(LLVMInitializeAArch64Target);
  MAP(LLVMInitializeAArch64TargetMC);
  MAP(LLVMInitializeAArch64AsmPrinter);
  MAP(LLVMCreateMemoryBufferWithMemoryRangeCopy);
  MAP(LLVMContextCreate);
  MAP(LLVMParseBitcodeInContext2);
  MAP(LLVMOrcCreateLLJIT);
  MAP(LLVMOrcLLJITGetTripleString);
  MAP(LLVMOrcLLJITGetDataLayoutStr);
  MAP(LLVMSetTarget);
  MAP(LLVMSetDataLayout);
  MAP(LLVMOrcCreateNewThreadSafeContextFromLLVMContext);
  MAP(LLVMOrcCreateNewThreadSafeModule);
  MAP(LLVMOrcLLJITGetMainJITDylib);
  MAP(LLVMOrcLLJITAddLLVMIRModule);
  MAP(LLVMOrcLLJITLookup);
  MAP(LLVMGetErrorMessage);
  return 0;
}
