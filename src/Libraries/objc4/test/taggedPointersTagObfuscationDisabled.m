// TEST_ENV OBJC_DISABLE_TAG_OBFUSCATION=YES

#include "test.h"
#include <objc/objc-internal.h>

#if !OBJC_HAVE_TAGGED_POINTERS

int main()
{
    succeed(__FILE__);
}

#else

int main()
{
#if OBJC_SPLIT_TAGGED_POINTERS
    void *obj = (void *)0;
#else
    void *obj = (void *)1;
#endif

    testassert(_objc_getTaggedPointerTag(obj) == 0);
    succeed(__FILE__);
}

#endif
