// pdcuda.c - a CUDA driver API check under elftrans: devices, a context, PTX JIT, memory and a vector add on the GPU
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
// NVIDIA's libcuda.so.1 is loaded at run time, the few entry points used are declared here
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int CUresult;
typedef int CUdevice;
typedef void *CUcontext;
typedef void *CUmodule;
typedef void *CUfunction;
typedef unsigned long long CUdeviceptr;

// c[i] = a[i] + b[i] for i < n, one thread an element
static const char ptx[] =
	".version 4.0\n"
	".target sm_50\n"
	".address_size 64\n"
	".visible .entry vadd(.param .u64 pa, .param .u64 pb, .param .u64 pc, .param .u32 pn)\n"
	"{\n"
	"  .reg .pred %p<2>;\n"
	"  .reg .b32 %r<6>;\n"
	"  .reg .f32 %f<4>;\n"
	"  .reg .b64 %rd<11>;\n"
	"  ld.param.u64 %rd1, [pa];\n"
	"  ld.param.u64 %rd2, [pb];\n"
	"  ld.param.u64 %rd3, [pc];\n"
	"  ld.param.u32 %r2, [pn];\n"
	"  mov.u32 %r3, %ctaid.x;\n"
	"  mov.u32 %r4, %ntid.x;\n"
	"  mov.u32 %r5, %tid.x;\n"
	"  mad.lo.s32 %r1, %r3, %r4, %r5;\n"
	"  setp.ge.u32 %p1, %r1, %r2;\n"
	"  @%p1 bra DONE;\n"
	"  cvta.to.global.u64 %rd4, %rd1;\n"
	"  cvta.to.global.u64 %rd5, %rd2;\n"
	"  cvta.to.global.u64 %rd6, %rd3;\n"
	"  mul.wide.u32 %rd7, %r1, 4;\n"
	"  add.s64 %rd8, %rd4, %rd7;\n"
	"  add.s64 %rd9, %rd5, %rd7;\n"
	"  add.s64 %rd10, %rd6, %rd7;\n"
	"  ld.global.f32 %f1, [%rd8];\n"
	"  ld.global.f32 %f2, [%rd9];\n"
	"  add.f32 %f3, %f1, %f2;\n"
	"  st.global.f32 [%rd10], %f3;\n"
	"DONE:\n"
	"  ret;\n"
	"}\n";

static void *lib;
static CUresult (*getErrorName)(CUresult, const char **);

static void *
sym(const char *name)
{
	void *p = dlsym(lib, name);

	if (p == NULL) {
		fprintf(stderr, "pdcuda: libcuda has no %s\n", name);
		exit(2);
	}
	return p;
}

// each step prints its result, so a run shows the first thing that fails
static int
check(const char *what, CUresult r)
{
	const char *name = "?";

	if (getErrorName != NULL)
		getErrorName(r, &name);
	printf("%-24s %d %s\n", what, r, r == 0 ? "ok" : name);
	return r == 0;
}

int
main(void)
{
	enum { N = 1 << 20 };
	lib = dlopen("libcuda.so.1", RTLD_NOW);
	if (lib == NULL) {
		fprintf(stderr, "pdcuda: %s\n", dlerror());
		return 2;
	}
	getErrorName = sym("cuGetErrorName");
	CUresult (*init)(unsigned) = sym("cuInit");
	CUresult (*getCount)(int *) = sym("cuDeviceGetCount");
	CUresult (*deviceGet)(CUdevice *, int) = sym("cuDeviceGet");
	CUresult (*deviceName)(char *, int, CUdevice) = sym("cuDeviceGetName");
	CUresult (*totalMem)(size_t *, CUdevice) = sym("cuDeviceTotalMem_v2");
	CUresult (*ctxCreate)(CUcontext *, unsigned, CUdevice) = sym("cuCtxCreate_v2");
	CUresult (*moduleLoad)(CUmodule *, const void *) = sym("cuModuleLoadData");
	CUresult (*getFunction)(CUfunction *, CUmodule, const char *) = sym("cuModuleGetFunction");
	CUresult (*memAlloc)(CUdeviceptr *, size_t) = sym("cuMemAlloc_v2");
	CUresult (*toDevice)(CUdeviceptr, const void *, size_t) = sym("cuMemcpyHtoD_v2");
	CUresult (*toHost)(void *, CUdeviceptr, size_t) = sym("cuMemcpyDtoH_v2");
	CUresult (*launch)(CUfunction, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, void *,
	    void **, void **) = sym("cuLaunchKernel");
	CUresult (*sync)(void) = sym("cuCtxSynchronize");

	int count = 0;
	char name[128] = "";
	size_t mem = 0;
	CUdevice dev;
	CUcontext ctx;
	CUmodule mod;
	CUfunction fn;
	CUdeviceptr da, db, dc;

	if (!check("cuInit", init(0)) || !check("cuDeviceGetCount", getCount(&count)) || count == 0)
		return 1;
	if (!check("cuDeviceGet", deviceGet(&dev, 0)) || !check("cuDeviceGetName", deviceName(name, sizeof(name), dev)))
		return 1;
	check("cuDeviceTotalMem", totalMem(&mem, dev));
	printf("device 0: %s, %zu MB\n", name, mem >> 20);
	if (!check("cuCtxCreate", ctxCreate(&ctx, 0, dev)) || !check("cuModuleLoadData (PTX JIT)", moduleLoad(&mod, ptx)) ||
	    !check("cuModuleGetFunction", getFunction(&fn, mod, "vadd")))
		return 1;

	float *a = malloc(N * sizeof(float)), *b = malloc(N * sizeof(float)), *c = malloc(N * sizeof(float));
	for (int i = 0; i < N; i++) {
		a[i] = (float)i;
		b[i] = (float)(2 * i);
		c[i] = -1.0f;
	}
	if (!check("cuMemAlloc", memAlloc(&da, N * sizeof(float))) || !check("cuMemAlloc", memAlloc(&db, N * sizeof(float))) ||
	    !check("cuMemAlloc", memAlloc(&dc, N * sizeof(float))) ||
	    !check("cuMemcpyHtoD", toDevice(da, a, N * sizeof(float))) ||
	    !check("cuMemcpyHtoD", toDevice(db, b, N * sizeof(float))))
		return 1;
	unsigned n = N;
	void *args[] = { &da, &db, &dc, &n };
	if (!check("cuLaunchKernel", launch(fn, N / 256, 1, 1, 256, 1, 1, 0, NULL, args, NULL)) ||
	    !check("cuCtxSynchronize", sync()) || !check("cuMemcpyDtoH", toHost(c, dc, N * sizeof(float))))
		return 1;

	int wrong = 0;
	for (int i = 0; i < N; i++)
		if (c[i] != 3.0f * (float)i)
			wrong++;
	printf("vector add of %d floats on %s: %d wrong\n", N, name, wrong);
	return wrong != 0;
}
