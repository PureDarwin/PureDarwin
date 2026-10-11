/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGFunction.h>
#import <Onyx2D/O2Function.h>
#import "CGConversions.h"
#include <stdlib.h>

CGFunctionRef CGFunctionRetain(CGFunctionRef self) {
   return O2FunctionRetain(self);
}

void CGFunctionRelease(CGFunctionRef self) {
   O2FunctionRelease(self);
}

// CG callbacks and arrays are CGFloat, Onyx2D's are float, so convert through a trampoline
typedef struct {
   void *info;
   size_t inCount, outCount;
   CGFunctionCallbacks callbacks;
} CGFunctionBridge;

static void bridgeEvaluate(void *raw,const float *in,float *out) {
   CGFunctionBridge *bridge=raw;
   CGFloat din[bridge->inCount], dout[bridge->outCount];

   for(size_t i=0;i<bridge->inCount;i++){
    din[i]=in[i];
   }
   if(bridge->callbacks.evaluate!=NULL) bridge->callbacks.evaluate(bridge->info,din,dout);
   for(size_t i=0;i<bridge->outCount;i++){
    out[i]=dout[i];
   }
}

static void bridgeRelease(void *raw) {
   CGFunctionBridge *bridge=raw;

   if(bridge->callbacks.releaseInfo!=NULL) bridge->callbacks.releaseInfo(bridge->info);
   free(bridge);
}

CGFunctionRef CGFunctionCreate(void *info,size_t domainDimension,const CGFloat *domain,size_t rangeDimension,const CGFloat *range,const CGFunctionCallbacks *callbacks) {
   O2Float fdomain[domainDimension*2+1], frange[rangeDimension*2+1];
   O2FunctionCallbacks o2callbacks={0,bridgeEvaluate,bridgeRelease};
   CGFunctionBridge *bridge;

   if(callbacks==NULL) return NULL;

   for(size_t i=0;domain!=NULL && i<domainDimension*2;i++){
    fdomain[i]=domain[i];
   }
   for(size_t i=0;range!=NULL && i<rangeDimension*2;i++){
    frange[i]=range[i];
   }

   bridge=calloc(1,sizeof(*bridge));
   bridge->info=info;
   bridge->inCount=domainDimension;
   bridge->outCount=rangeDimension;
   bridge->callbacks=*callbacks;
   return (CGFunctionRef)O2FunctionCreate(bridge,domainDimension,domain?fdomain:NULL,rangeDimension,range?frange:NULL,&o2callbacks);
}
