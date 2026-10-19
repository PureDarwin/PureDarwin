/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGAffineTransform.h>
#import <CoreFoundation/CFDictionary.h>
#import <CoreFoundation/CFNumber.h>
#import <CoreFoundation/CFString.h>
#include <math.h>
#include <math.h>

const CGAffineTransform CGAffineTransformIdentity={1,0,0,1,0,0};

bool CGAffineTransformIsIdentity(CGAffineTransform xform)
{
    return xform.a == 1 && xform.b == 0 && xform.c == 0 && xform.d == 1 && xform.tx == 0 && xform.ty == 0;
}

CGAffineTransform CGAffineTransformMake(CGFloat a,CGFloat b,CGFloat c,CGFloat d,CGFloat tx,CGFloat ty){
   CGAffineTransform xform={a,b,c,d,tx,ty};
   return xform;
}

CGAffineTransform CGAffineTransformMakeRotation(CGFloat radians){
   CGAffineTransform xform={cos(radians),sin(radians),-sin(radians),cos(radians),0,0};
   return xform;
}

CGAffineTransform CGAffineTransformMakeScale(CGFloat scalex,CGFloat scaley){
   CGAffineTransform xform={scalex,0,0,scaley,0,0};
   return xform;
}

CGAffineTransform CGAffineTransformMakeTranslation(CGFloat tx,CGFloat ty){
   CGAffineTransform xform={1,0,0,1,tx,ty};
   return xform;
}

CGAffineTransform CGAffineTransformConcat(CGAffineTransform xform,CGAffineTransform append){
   CGAffineTransform result;

   result.a=xform.a*append.a+xform.b*append.c;
   result.b=xform.a*append.b+xform.b*append.d;
   result.c=xform.c*append.a+xform.d*append.c;
   result.d=xform.c*append.b+xform.d*append.d;
   result.tx=xform.tx*append.a+xform.ty*append.c+append.tx;
   result.ty=xform.tx*append.b+xform.ty*append.d+append.ty;

   return result;
}

CGAffineTransform CGAffineTransformInvert(CGAffineTransform xform){
   CGAffineTransform result;
   CGFloat determinant;

   determinant=xform.a*xform.d-xform.c*xform.b;
   if(determinant==0){
    return xform;
   }

   result.a=xform.d/determinant;
   result.b=-xform.b/determinant;
   result.c=-xform.c/determinant;
   result.d=xform.a/determinant;
   result.tx=(-xform.d*xform.tx+xform.c*xform.ty)/determinant;
   result.ty=(xform.b*xform.tx-xform.a*xform.ty)/determinant;

   return result;
}

CGAffineTransform CGAffineTransformRotate(CGAffineTransform xform,CGFloat radians){
   CGAffineTransform rotate=CGAffineTransformMakeRotation(radians);
   return CGAffineTransformConcat(rotate,xform);
}

CGAffineTransform CGAffineTransformScale(CGAffineTransform xform,CGFloat scalex,CGFloat scaley){
   CGAffineTransform scale=CGAffineTransformMakeScale(scalex,scaley);
   return CGAffineTransformConcat(scale,xform);
}

CGAffineTransform CGAffineTransformTranslate(CGAffineTransform xform,CGFloat tx,CGFloat ty){
   CGAffineTransform translate=CGAffineTransformMakeTranslation(tx,ty);
   return CGAffineTransformConcat(translate,xform);
}

CGPoint CGPointApplyAffineTransform(CGPoint point,CGAffineTransform xform){
    CGPoint p;

    p.x=xform.a*point.x+xform.c*point.y+xform.tx;
    p.y=xform.b*point.x+xform.d*point.y+xform.ty;

    return p;
}

CGSize CGSizeApplyAffineTransform(CGSize size,CGAffineTransform xform){
    CGSize s;

    s.width=xform.a*size.width+xform.c*size.height;
    s.height=xform.b*size.width+xform.d*size.height;

    return s;
}

// the bounding box of the four transformed corners, a null rect stays null
CGRect CGRectApplyAffineTransform(CGRect rect,CGAffineTransform t){
   if(CGRectIsNull(rect))
    return rect;
   if(rect.size.width<0){
    rect.origin.x+=rect.size.width;
    rect.size.width=-rect.size.width;
   }
   if(rect.size.height<0){
    rect.origin.y+=rect.size.height;
    rect.size.height=-rect.size.height;
   }

   CGFloat xs[4]={ rect.origin.x, rect.origin.x+rect.size.width, rect.origin.x, rect.origin.x+rect.size.width };
   CGFloat ys[4]={ rect.origin.y, rect.origin.y, rect.origin.y+rect.size.height, rect.origin.y+rect.size.height };
   CGFloat minX=INFINITY,minY=INFINITY,maxX=-INFINITY,maxY=-INFINITY;

   for(int i=0;i<4;i++){
    CGFloat x=t.a*xs[i]+t.c*ys[i]+t.tx;
    CGFloat y=t.b*xs[i]+t.d*ys[i]+t.ty;

    minX=fmin(minX,x);
    minY=fmin(minY,y);
    maxX=fmax(maxX,x);
    maxY=fmax(maxY,y);
   }
   return CGRectMake(minX,minY,maxX-minX,maxY-minY);
}

static bool numberForKey(CFDictionaryRef dict,CFStringRef key,CGFloat *value){
   CFTypeRef number=CFDictionaryGetValue(dict,key);

   if(number==NULL || CFGetTypeID(number)!=CFNumberGetTypeID())
    return false;
   return CFNumberGetValue((CFNumberRef)number,kCFNumberCGFloatType,value);
}

// the X, Y, Width and Height keys of Apple's dictionary representation
CFDictionaryRef CGRectCreateDictionaryRepresentation(CGRect rect){
   CFStringRef keys[4]={ CFSTR("X"), CFSTR("Y"), CFSTR("Width"), CFSTR("Height") };
   CFNumberRef values[4]={
    CFNumberCreate(NULL,kCFNumberCGFloatType,&rect.origin.x),
    CFNumberCreate(NULL,kCFNumberCGFloatType,&rect.origin.y),
    CFNumberCreate(NULL,kCFNumberCGFloatType,&rect.size.width),
    CFNumberCreate(NULL,kCFNumberCGFloatType,&rect.size.height),
   };
   CFDictionaryRef dict=CFDictionaryCreate(NULL,(const void **)keys,(const void **)values,4,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);

   for(int i=0;i<4;i++){
    CFRelease(values[i]);
   }
   return dict;
}

bool CGRectMakeWithDictionaryRepresentation(CFDictionaryRef dict,CGRect *rect){
   CGRect r;

   if(dict==NULL || rect==NULL || CFGetTypeID(dict)!=CFDictionaryGetTypeID())
    return false;
   if(!numberForKey(dict,CFSTR("X"),&r.origin.x) || !numberForKey(dict,CFSTR("Y"),&r.origin.y) ||
      !numberForKey(dict,CFSTR("Width"),&r.size.width) || !numberForKey(dict,CFSTR("Height"),&r.size.height))
    return false;
   *rect=r;
   return true;
}

static CFDictionaryRef createNumberDictionary(CFStringRef key0, CGFloat v0, CFStringRef key1, CGFloat v1){
   CFStringRef keys[2]={ key0, key1 };
   CFNumberRef values[2]={ CFNumberCreate(NULL,kCFNumberCGFloatType,&v0), CFNumberCreate(NULL,kCFNumberCGFloatType,&v1) };
   CFDictionaryRef dict=CFDictionaryCreate(NULL,(const void **)keys,(const void **)values,2,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);

   CFRelease(values[0]);
   CFRelease(values[1]);
   return dict;
}

CFDictionaryRef CGPointCreateDictionaryRepresentation(CGPoint point){
   return createNumberDictionary(CFSTR("X"),point.x,CFSTR("Y"),point.y);
}

bool CGPointMakeWithDictionaryRepresentation(CFDictionaryRef dict,CGPoint *point){
   CGPoint p;

   if(dict==NULL || point==NULL || CFGetTypeID(dict)!=CFDictionaryGetTypeID())
    return false;
   if(!numberForKey(dict,CFSTR("X"),&p.x) || !numberForKey(dict,CFSTR("Y"),&p.y))
    return false;
   *point=p;
   return true;
}

CFDictionaryRef CGSizeCreateDictionaryRepresentation(CGSize size){
   return createNumberDictionary(CFSTR("Width"),size.width,CFSTR("Height"),size.height);
}

bool CGSizeMakeWithDictionaryRepresentation(CFDictionaryRef dict,CGSize *size){
   CGSize s;

   if(dict==NULL || size==NULL || CFGetTypeID(dict)!=CFDictionaryGetTypeID())
    return false;
   if(!numberForKey(dict,CFSTR("Width"),&s.width) || !numberForKey(dict,CFSTR("Height"),&s.height))
    return false;
   *size=s;
   return true;
}
