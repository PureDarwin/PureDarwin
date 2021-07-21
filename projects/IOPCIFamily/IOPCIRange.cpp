/*
 * Copyright (c) 2010-2010 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifdef KERNEL

#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/pci/IOPCIConfigurator.h>

#else

/*
cc IOPCIRange.cpp -o /tmp/pcirange -Wall -framework IOKit -framework CoreFoundation -arch i386 -g -lstdc++
*/

#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

#include "IOKit/pci/IOPCIConfigurator.h"
#define panic(x)               do { printf("panic: %s\n", x); assert(0); } while(0)
#define kprintf(fmt, args...)  printf(fmt, ## args)

#endif

IOPCIScalar IOPCIScalarAlign(IOPCIScalar num, IOPCIScalar alignment)
{
    return (num + (alignment - 1) & ~(alignment - 1));
}

IOPCIScalar IOPCIScalarTrunc(IOPCIScalar num, IOPCIScalar alignment)
{
    return (num & ~(alignment - 1));
}

IOPCIRange * IOPCIRangeAlloc(void)
{
#ifdef KERNEL
    return (IONew(IOPCIRange, 1));
#else
    return ((IOPCIRange *) malloc(sizeof(IOPCIRange)));
#endif
}

void IOPCIRangeFree(IOPCIRange * range)
{
//  memset(range, 0xBB, sizeof(*range));
#ifdef KERNEL
    IODelete(range, IOPCIRange, 1);
#else
    free(range);
#endif
}

void IOPCIRangeInit(IOPCIRange * range, uint32_t type,
                    IOPCIScalar start, IOPCIScalar size, IOPCIScalar alignment)
{
    bzero(range, sizeof(*range));
    range->type         = type;
    range->start        = start;
//    range->size         = 0;
    range->proposedSize = size;
    range->totalSize    = size;
    range->end          = start;
//    range->zero         = 0;
    range->alignment    = alignment ? alignment : size;
//    range->minAddress   = 0;
    range->maxAddress   = 0xFFFFFFFF;
    range->allocations  = (IOPCIRange *) &range->end;
}

void IOPCIRangeInitAlloc(IOPCIRange * range, uint32_t type,
                         IOPCIScalar start, IOPCIScalar size, IOPCIScalar alignment)
{
    IOPCIRangeInit(range, type, start, size, alignment);
    range->size = range->proposedSize;
    range->end  = range->start + range->size;
}

bool IOPCIRangeListAddRange(IOPCIRange ** rangeList,
                            uint32_t type,
                            IOPCIScalar start,
                            IOPCIScalar size,
                            IOPCIScalar alignment)
{
    IOPCIRange *  range;
    IOPCIRange *  nextRange;
    IOPCIRange ** prev;
    IOPCIScalar   end;
    bool          result = true;
    bool          alloc  = true;

    end = start + size;
    for (prev = rangeList; (range = *prev); prev = &range->next)
    {
        if (((start >= range->start) && (start < range->end))
         || ((end > range->start) && (end <= range->end))
         || ((start < range->start) && (end > range->end)))
        {
            range = NULL;
            result = false;
            break;
        }
        if (end == range->start)
        {
            range->start        = start;
            range->size         = range->end - range->start;
            range->proposedSize = range->size;
            alloc = false;
            break;
        }
        if (start == range->end)
        {
            if ((nextRange = range->next) && (nextRange->start == end))
            {
                if (nextRange->allocations != (IOPCIRange *) &nextRange->end)
                    assert(false);
                end = nextRange->end;
                range->next = nextRange->next;
                IOPCIRangeFree(nextRange);
            }
            range->end          = end;
            range->size         = end - range->start;
            range->proposedSize = range->size;
            alloc = false;
            break;
        }
        if (range->start > end)
        {
            alloc = true;
            break;
        }
    }

    if (result && alloc)
    {
        nextRange = IOPCIRangeAlloc();
        IOPCIRangeInitAlloc(nextRange, type, start, size, alignment);
        nextRange->next = range;
        *prev = nextRange;
    }

    return (result);
}

IOPCIScalar IOPCIRangeListCollapse(IOPCIRange * headRange, IOPCIScalar alignment)
{
    IOPCIScalar total = 0;

    while (headRange)
    {
        total += IOPCIRangeCollapse(headRange, alignment);
        headRange = headRange->next;
    }

    return (total);
}

IOPCIScalar IOPCIRangeCollapse(IOPCIRange * headRange, IOPCIScalar alignment)
{
    IOPCIScalar   start, end, saving;
    IOPCIRange *  range;

    start  = 0;
    end    = 0;
    saving = headRange->size;
    range  = headRange->allocations;
    do
    {
        // keep walking down the list
        if (!range->size)
            break;
        if (!start)
            start = range->start;
        end = range->end;
        range = range->nextSubRange;
    }
    while(true); 

    start = IOPCIScalarTrunc(start, alignment);
    end   = IOPCIScalarAlign(end,   alignment);

	if (!start) headRange->proposedSize = 0;
	else
	{
		headRange->start        = start;
		headRange->end          = end;
		headRange->size         = 
		headRange->proposedSize = end - start;
		if (headRange->size > headRange->totalSize) 
		{
			headRange->extendSize = headRange->size - headRange->totalSize;
		}
		else
        {
            headRange->extendSize = 0;
        }
	}
	if (saving < headRange->proposedSize) panic("IOPCIRangeCollapse");
    else if (headRange->proposedSize <= headRange->totalSize) saving = 0;
    else                                                      saving -= headRange->proposedSize;

    return (saving);
}

IOPCIScalar IOPCIRangeListLastFree(IOPCIRange * headRange, IOPCIScalar align)
{
	IOPCIRange * next;
	next = headRange;
    while (next)
    {
		headRange = next;
        next = next->next;
    }
	return (IOPCIRangeLastFree(headRange, align));
}

IOPCIScalar IOPCIRangeLastFree(IOPCIRange * headRange, IOPCIScalar align)
{
    IOPCIScalar   last;
    IOPCIRange *  range;

    range = headRange->allocations;
    last  = headRange->start;
    do
    {
        // keep walking down the list
        if (!range->size) break;
        last = range->end;
        range = range->nextSubRange;
    }
    while(true);

	last = IOPCIScalarAlign(last, align);
	if (headRange->end > last)
		last = headRange->end - last;
	else
		last = 0;

    return (last);
}

void IOPCIRangeOptimize(IOPCIRange * headRange)
{
    IOPCIScalar   free;
    IOPCIScalar	  count;
	IOPCIScalar   chunk, bump, tail;
    IOPCIScalar	  next, end;
    IOPCIRange *  range;
    IOPCIRange *  prev;

    range = headRange->allocations;
    prev  = NULL;
    free  = 0;
    count = 0;
    do
	{
		if (prev)
		{
			assert(range->start >= prev->end);
			free += (range->start - prev->end);
		}
        // eol
        if (!range->size) break;

		range->nextToAllocate = prev;
		if ((kIOPCIRangeFlagMaximizeSize | kIOPCIRangeFlagMaximizeRoot | kIOPCIRangeFlagSplay)
			 & range->flags) count++;
		prev = range;
        range = range->nextSubRange;
    }
    while(true);

	if (!free) return;

	end = headRange->end;
	for (range = prev; free && range; end = range->start, range = range->nextToAllocate)
	{
		if (!(kIOPCIRangeFlagRelocatable & range->flags)) continue;

        chunk = 0;
		if ((kIOPCIRangeFlagMaximizeSize | kIOPCIRangeFlagSplay) & range->flags)
        {
            chunk = free / count;
            chunk = IOPCIScalarTrunc(chunk, range->alignment);
            free -= chunk;
            count--;
        }

		next = range->end;
		if (end < next) panic("end");
        tail = IOPCIScalarTrunc(end - next, range->alignment);
        if (chunk > tail) chunk = tail;

		if (kIOPCIRangeFlagSplay & range->flags)
		{
            bump = tail - chunk;
			range->end   += bump;
			range->start += bump;
		}
        else
		{
            range->size        += chunk;
            range->proposedSize = range->size;
            range->start        = IOPCIScalarTrunc(end - range->size, range->alignment);
            range->end          = range->start + range->size;
		}

        if (range->start & (range->alignment - 1)) panic("sA");
        if (range->start > headRange->end)         panic("s>");
        if (range->start < headRange->start)       panic("s<");
        if (range->end > headRange->end)           panic("e>");
        if (range->end < headRange->start)         panic("e<");
    }
}

void IOPCIRangeListOptimize(IOPCIRange * headRange)
{
	IOPCIRange * next;
	next = headRange;
    while (next)
    {
		IOPCIRangeOptimize(next);
        next = next->next;
    }
}

bool IOPCIRangeListAllocateSubRange(IOPCIRange * headRange,
                                    IOPCIRange * newRange,
                                    IOPCIScalar  newStart)
{
    IOPCIScalar   minSize, maxSize;
    IOPCIScalar   len, bestFit, waste;
	IOPCIScalar   pos, endPos;
    IOPCIRange ** where = NULL;
    IOPCIRange *  whereNext = NULL;
	IOPCIRange *  range = NULL;
	IOPCIRange ** prev;

	minSize = newRange->size;
	if (!minSize)  minSize = newRange->proposedSize;
	if (!minSize)  panic("!minSize");
	if (!newStart) newStart = newRange->start;

	bestFit = UINT64_MAX;
    for (; headRange; headRange = headRange->next)
    {
        if (!headRange->size) continue;

		maxSize = newRange->proposedSize;
		if (!maxSize) panic("!maxSize");
		if (maxSize < minSize) minSize = maxSize;

        pos  = headRange->start;
        prev = &headRange->allocations;
        range = NULL;
        do
        {
			if (range)
			{
				// onto next element
				if (!range->size)
				{
					// end of list
					range = NULL;
					break;
				}
				pos = range->end;
				prev = &range->nextSubRange;
			}
            range = *prev;
            if (range == newRange)
            {
				// reallocate in place - treat as free
                range = range->nextSubRange;
            }
			endPos = range->start;

			// [pos,endPos] is free
			waste = endPos - pos;
			if (pos > newRange->maxAddress)    pos = newRange->maxAddress;
			if (endPos > newRange->maxAddress) endPos = newRange->maxAddress;
			if (newStart)
			{
				if (newStart < pos)     continue;
				if (newStart >= endPos) continue;
				pos = newStart;
			}
			else pos = IOPCIScalarAlign(pos, newRange->alignment);
			if (kIOPCIRangeFlagMaximizeRoot & newRange->flags)
				endPos = IOPCIScalarTrunc(endPos, newRange->alignment);

			if (endPos < pos) continue;

			len = endPos - pos;
			if (len < minSize) continue;
			if (len > maxSize) len = maxSize;

			if (newStart
				|| (where && (newRange->start < newRange->minAddress) && (pos >= newRange->minAddress))
				|| (len < maxSize)
				|| (kIOPCIRangeFlagMaximizeRoot & newRange->flags))
			{
			}
			else
			{
				if ((kIOPCIRangeFlagSplay | kIOPCIRangeFlagMaximizeSize) & newRange->flags)
				{
					// in biggest free area position
					waste = UINT64_MAX - waste;
				}
				else
				{
					// least waste position
					waste -= len;
				}
				if (where && (bestFit < waste)) continue;
				bestFit = waste;
			}
			// best candidate, will queue prev->newRange->range
			// new size to look for
			minSize         = len;
			where           = prev;
			whereNext       = range;
			newRange->start = pos;
			newRange->size  = len;
			newRange->end   = pos + len;

			if (newStart || !waste)
			{
				// use this if placed or zero waste
				break;
			}
        }
        while(true); 
    }

    if (where)
    {
		if (kIOPCIRangeFlagMaximizeRoot & newRange->flags) newRange->proposedSize = newRange->size;
        newRange->nextSubRange = whereNext;
        *where = newRange;
    }

    return (where != NULL);
}

bool IOPCIRangeListDeallocateSubRange(IOPCIRange * headRange,
                                	  IOPCIRange * oldRange)
{
    IOPCIRange *  range = NULL;
    IOPCIRange ** prev = NULL;

    do
    {
		range = oldRange->allocations;
        if (!range->size)
            break;
        IOPCIRangeListDeallocateSubRange(oldRange, range);
    }
    while(true); 

    for (range = NULL;
         headRange && !range; 
         headRange = headRange->next)
    {
        prev = &headRange->allocations;
        do
        {
            range = *prev;
            if (range == oldRange)
                break;
            // keep walking down the list
            if (!range->size)
            {
                range = NULL;
                break;
            }
        }
        while (prev = &range->nextSubRange, true);
    }

    if (range)
    {
        *prev = range->nextSubRange;
		oldRange->nextSubRange  = NULL;
		oldRange->end           = 0;
//		oldRange->proposedSize  = oldRange->size;
//		oldRange->size          = 0;
//		oldRange->extendSize    = 0;
		oldRange->start         = 0;
	}

    return (range != 0);
}

IOPCIScalar IOPCIRangeListSize(IOPCIRange * first)
{
    IOPCIRange * allocs;
    IOPCIRange * range;
    IOPCIRange * prev;
    IOPCIRange * newAlloc;
    IOPCIRange * newAllocNext;
    IOPCIScalar  size, prevAddr, nextAddr;

    allocs = NULL;
    for (newAlloc = first; newAlloc; newAlloc = newAllocNext)
    {
		newAllocNext = newAlloc->nextToAllocate;
		if (!allocs)
		{
			allocs = newAlloc;
			newAlloc->nextToAllocate = NULL;
			continue;
		}
		size     = (newAlloc->totalSize + newAlloc->extendSize);
	    prev     = NULL;
	    prevAddr = nextAddr = 0;
		for (range = allocs; ; range = range->nextToAllocate)
		{
			if (range)
			{
				nextAddr = IOPCIScalarAlign(prevAddr, range->alignment);
				assert(nextAddr >= prevAddr);
			}
			if (!range || (size <= (nextAddr - prevAddr)))
			{
				newAlloc->nextToAllocate = prev->nextToAllocate;
				prev->nextToAllocate = newAlloc;
				break;
			}
			prevAddr = nextAddr + range->totalSize + range->extendSize;
			prev = range;
		}
    }

	prevAddr = 0;
	for (range = allocs; range; range = range->nextToAllocate)
	{
		nextAddr = IOPCIScalarAlign(prevAddr, range->alignment);
		size = range->totalSize + range->extendSize;
		prevAddr = nextAddr + size;
	}

	return (prevAddr);
}

void IOPCIRangeDump(IOPCIRange * head)
{
#if !DEVELOPMENT && !defined(__x86_64__) && defined(KERNEL)
#else
    IOPCIRange * range;
    uint32_t idx;
    
    do
    {
        kprintf("head.start     0x%llx\n", head->start);
        kprintf("head.size      0x%llx\n", head->size);
        kprintf("head.end       0x%llx\n", head->end);
        kprintf("head.alignment 0x%llx\n", head->alignment);
    
        kprintf("allocs:\n");
        range = head->allocations;
        idx = 0;
        while (true)
        {
            if (range == (IOPCIRange *) &head->end)
            {
                kprintf("[end]\n");
                break;
            }
            kprintf("[%d].start     0x%llx\n", idx, range->start);
            kprintf("[%d].size      0x%llx\n", idx, range->size);
            kprintf("[%d].end       0x%llx\n", idx, range->end);
            kprintf("[%d].alignment 0x%llx\n", idx, range->alignment);
            idx++;
            range = range->nextSubRange;
        }
        kprintf("------\n");
    }
    while ((head = head->next));

    kprintf("------------------------------------\n");
#endif
}

#ifndef KERNEL

int main(int argc, char **argv)
{
    IOPCIRange * head = NULL;
    IOPCIRange * range;
    IOPCIRange * requests = NULL;
    IOPCIRange * elems[24];
    IOPCIScalar  shrink;
    size_t       idx;
    bool         ok;

    for (idx = 0; idx < sizeof(elems) / sizeof(elems[0]); idx++)
    {
        elems[idx] = IOPCIRangeAlloc();
        elems[idx]->maxAddress = 0xFFFFFFFFFFFFFFFFULL;
    }

#if 1
#if 0
  MEM: 0xd1c00000:0xd00000,0xd00000-0xd00000,0x0:0x400000 (at [i1e]188:0:0(0x8086:0x156d)) ARsmbv  ok allocated
//  MEM: 0xd1c00000:0x500000,0x500000-0x500000,0x0:0x400000 (at [i1f]189:3:0(0x8086:0x156d)) ARsmbv  ok allocated
//  MEM: 0xd2400000:0x500000,0x500000-0x500000,0x0:0x400000 (at [i21]189:5:0(0x8086:0x156d)) ARSmbv  ok allocated
#endif

    IOPCIRangeListAddRange(&head, 0, 0xd1c00000, 0xd00000, 0x400000);

    idx = 0;
	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0, 0x500000, 0x400000);
	range->flags = kIOPCIRangeFlagRelocatable;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0, 0x500000, 0x400000);
	range->flags = kIOPCIRangeFlagRelocatable | kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

    IOPCIRangeDump(head);
    IOPCIRangeListOptimize(head);
    IOPCIRangeDump(head);
    exit(0);

#elif 1

#if 0
  MEM: 0xb1000000:0x500000,0x500000-0x500000,0x0:0x400000 (at [ib]0:29:3(0x8086:0x9d1b)) Arsmbv  ok allocated
  MEM: 0xb1500000:0x100000,0x100000-0x100000,0x0:0x100000 (at [i8]0:28:0(0x8086:0x9d10)) ARsmbv  ok allocated
  MEM: 0xb1624000:0x4000,0x4000-0x4000,0x0:0x4000 (at [i11]0:31:2(0x8086:0x9d21)) Arsmbv  ok allocated
  MEM: 0xba900000:0x200000,0x200000-0x200000,0x0:0x100000 (at [ia]0:29:0(0x8086:0x9d18)) ARsMbv  ok allocated
  MEM: 0xb1700000:0x200000,0x200000-0x200000,0x0:0x100000 (at [i9]0:28:4(0x8086:0x9d14)) ARsMbv  ok allocated
  MEM: 0x7f81000000:0x1000000,0x1000000-0x1000000,0x0:0x1000000 (at [i4]0:2:0(0x8086:0x1927)) Arsmbv  ok allocated
  MEM: 0x7f80200000:0x10000,0x10000-0x10000,0x0:0x10000 (at [i12]0:31:3(0x8086:0x9d70)) Arsmbv  ok allocated
  MEM: 0x7f80210000:0x10000,0x10000-0x10000,0x0:0x10000 (at [i5]0:20:0(0x8086:0x9d2f)) Arsmbv  ok allocated
  MEM: 0x7f80220000:0x4000,0x4000-0x4000,0x0:0x4000 (at [i12]0:31:3(0x8086:0x9d70)) Arsmbv  ok allocated
  MEM: 0x7f80224000:0x1000,0x1000-0x1000,0x0:0x1000 (at [if]0:30:3(0x8086:0x9d2a)) Arsmbv  ok allocated
  MEM: 0x7f80225000:0x1000,0x1000-0x1000,0x0:0x1000 (at [ie]0:30:2(0x8086:0x9d29)) Arsmbv  ok allocated
  MEM: 0x7f80226000:0x1000,0x1000-0x1000,0x0:0x1000 (at [id]0:30:1(0x8086:0x9d28)) Arsmbv  ok allocated
  MEM: 0x7f80227000:0x1000,0x1000-0x1000,0x0:0x1000 (at [ic]0:30:0(0x8086:0x9d27)) Arsmbv  ok allocated
  MEM: 0x7f80228000:0x1000,0x1000-0x1000,0x0:0x1000 (at [i7]0:25:0(0x8086:0x9d66)) Arsmbv  ok allocated
  MEM: 0x7f80229000:0x1000,0x1000-0x1000,0x0:0x1000 (at [i6]0:22:0(0x8086:0x9d3a)) Arsmbv  ok allocated
  MEM: 0x7f8022a000:0x100,0x100-0x100,0x0:0x100 (at [i13]0:31:4(0x8086:0x9d23)) Arsmbv  ok allocated
#endif

    IOPCIRangeListAddRange(&head, 0, 0x0000000080000000, 0x0000000070000000, 0x1000);
    IOPCIRangeListAddRange(&head, 0, 0x0000007f80000000, 0x0000000080000000, 0x1000);

    idx = 0;
	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0xb1000000, 0x500000, 0x400000);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0xb1500000, 0x100000, 0x100000);
	range->flags = kIOPCIRangeFlagRelocatable;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0xb1624000, 0x4000, 0x4000);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0xba900000, 0x200000, 0x100000);
	range->flags = kIOPCIRangeFlagMaximizeSize;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0xb1700000, 0x200000, 0x100000);
	range->flags = kIOPCIRangeFlagMaximizeSize;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f81000000, 0x1000000, 0x1000000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80200000, 0x10000, 0x10000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80210000, 0x10000, 0x10000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80220000, 0x4000, 0x4000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80224000, 0x1000, 0x1000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80225000, 0x1000, 0x1000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80226000, 0x1000, 0x1000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80227000, 0x1000, 0x1000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80228000, 0x1000, 0x1000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f80229000, 0x1000, 0x1000);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0x7f8022a000, 0x100, 0x100);
	range->maxAddress = 0xffffffffffffffff;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

    IOPCIRangeDump(head);
    IOPCIRangeListOptimize(head);
    IOPCIRangeDump(head);
    exit(0);

#elif 0
    idx = 0;
	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0, 0x11000000, 0x08000000);
	range->nextToAllocate = elems[idx];

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0, 0x11000000, 0x08000000);
	range->nextToAllocate = elems[idx];

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0, 0x08000000, 0x01000000);
	range->nextToAllocate = elems[idx];

	range = elems[idx++];
	IOPCIRangeInit(range, 0, 0, 0x2000, 0x2000);
	range->nextToAllocate = NULL;


	shrink = IOPCIRangeListSize(elems[0]);
	printf("size 0x%08qx\n", shrink);
    exit(0);

#elif 0

    IOPCIRangeListAddRange(&head, 0, 0x00000000b5f00000, 0x0000000001100000, 0x0000000000400000);

	range = elems[0];
	IOPCIRangeInit(range, 0, 0, 0x0000000000100000, 0x0000000000100000);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[1];
	IOPCIRangeInit(range, 0, 0, 0x0000000000f00000, 0x400000);
    range->flags |= kIOPCIRangeFlagRelocatable | kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

    IOPCIRangeDump(head);

    IOPCIRangeListOptimize(head);

    IOPCIRangeDump(head);
    
    exit(0);

#elif 0

	uint32_t flags;

    IOPCIRangeListAddRange(&head, 0, 0x25, 0x7, 1);
    IOPCIRangeDump(head);

	range = elems[0];
	IOPCIRangeInit(range, 0, 0x25, 1, 1);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[1];
	IOPCIRangeInit(range, 0, 0, 6, 1);
	range->size = 1;
	range->flags |= kIOPCIRangeFlagMaximizeSize;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);
	assert(range->proposedSize == range->size);
    IOPCIRangeDump(head);
	exit(0);

	range = elems[2];
	IOPCIRangeInit(range, 0, 0x7, 1, 1);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[3];
	IOPCIRangeInit(range, 0, 0x6, 0x1, 1);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[4];
	IOPCIRangeInit(range, 0, 0x8, 1, 1);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);



	range = elems[5];
	IOPCIRangeInit(range, 0, 0, 0xa, 1);
	range->size = 9;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(0xa == range->size);
	assert(ok);
exit(0);
	flags = kIOPCIRangeFlagRelocatable | kIOPCIRangeFlagSplay;
//	flags = kIOPCIRangeFlagRelocatable | kIOPCIRangeFlagMaximizeSize;

	range = elems[2];
	IOPCIRangeInit(range, 0, 0, 0xe, 1);
	range->flags |= flags;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[3];
	IOPCIRangeInit(range, 0, 0, 0x7, 1);
	range->flags |= flags;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[4];
	IOPCIRangeInit(range, 0, 0, 0x7, 1);
	range->flags |= flags;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	IOPCIRangeOptimize(head);

    exit(0);
#endif



#if 0
    IOPCIRangeListAddRange(&head, 0, 0,0, 1024*1024);
    IOPCIRangeDump(head);
    shrink = IOPCIRangeListLastFree(head, 1024*1024);
    printf("IOPCIRangeListLastFree 0x%llx\n", shrink);
exit(0);
#endif

#if 0
    IOPCIRangeListAddRange(&head, 0, 0xa0800000, 0x500000, 0x100000);

	range = elems[0];
	IOPCIRangeInit(range, 0, 0xa0800000, 0x40000, 0x40000);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[1];
	IOPCIRangeInit(range, 0, 0xa0a00000, 0x400000, 0x100000);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range->proposedSize = 0x300000;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);
//	ok = IOPCIRangeListDeallocateSubRange(head, range);
//	assert(ok);
    IOPCIRangeDump(head);

    exit(0);
#endif

    IOPCIRangeListAddRange(&head, 0, 0x6, 0xfa, 1);
    IOPCIRangeDump(head);

	range = elems[4];
	range->start = 0x06;
	range->proposedSize = 0x01;
	range->alignment = 1;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[3];
	range->start = 0x07;
	range->proposedSize = 0x01;
	range->alignment = 1;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[1];
	range->start = 1*0x87;
	range->size = 3;
	range->proposedSize = 3;
	range->alignment = 1;
	range->flags = kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[2];
	range->start = 0;
	range->proposedSize = 2;
	range->alignment = 1;
	range->flags = kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);


	range = elems[5];
	range->start = 0;
	range->proposedSize = 1;
	range->alignment = 1;
	range->flags = kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[6];
	range->start = 0;
	range->proposedSize = 1;
	range->alignment = 1;
	range->flags = kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);


    IOPCIRangeDump(head);
	exit(0);

    shrink = IOPCIRangeListLastFree(head, 1024);
    printf("IOPCIRangeListLastFree 0x%llx\n", shrink);
    exit(0);

    idx = 0;
    IOPCIRangeInit(elems[idx++], 0, 0, 1024*1024);

    range = elems[0];
    ok = IOPCIRangeListAllocateSubRange(head, range);
    printf("alloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
    IOPCIRangeDump(head);

    shrink = IOPCIRangeListCollapse(head, 1024*1024);
    printf("Collapsed by 0x%llx\n", shrink);
    IOPCIRangeDump(head);
    exit(0);


    IOPCIRangeListAddRange(&head, 0, 0xA0000000, 0x10000000);
    IOPCIRangeListAddRange(&head, 0, 0x98000000, 0x08000000);
    IOPCIRangeListAddRange(&head, 0, 0x90000000, 0x08000000);
    IOPCIRangeListAddRange(&head, 0, 0xB0000000, 0x10000000);

//exit(0);


    idx = 0;
    IOPCIRangeInit(elems[idx++], 0, 0x80001000, 0x1000);
    IOPCIRangeInit(elems[idx++], 0, 0, 0x1000000);
    IOPCIRangeInit(elems[idx++], 0, 0, 0x20000000);
    IOPCIRangeInit(elems[idx++], 0, 0, 0x20000000);
    IOPCIRangeInit(elems[idx++], 0, 0x80002000, 0x800);
    IOPCIRangeInit(elems[idx++], 0, 0, 0x10000, 1);

    for (idx = 0; idx < sizeof(elems) / sizeof(elems[0]); idx++)
    {
        if (elems[idx]->size)
            IOPCIRangeAppendSubRange(&requests, elems[idx]);
    }

    printf("reqs:\n");
    range = requests;
    idx = 0;
    while (range)
    {
        printf("[%ld].start     0x%llx\n", idx, range->start);
        printf("[%ld].size      0x%llx\n", idx, range->size);
        printf("[%ld].end       0x%llx\n", idx, range->end);
        printf("[%ld].alignment 0x%llx\n", idx, range->alignment);
        idx++;
        range = range->nextSubRange;
    }

    while ((range = requests))
    {
        requests = range->nextSubRange;
        ok = IOPCIRangeListAllocateSubRange(head, range);
        printf("alloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
    }

    IOPCIRangeDump(head);
    shrink = IOPCIRangeListCollapse(head, 1024*1024);
    printf("Collapsed by 0x%llx\n", shrink);
    IOPCIRangeDump(head);
exit(0);

    for (idx = 0; idx < sizeof(elems) / sizeof(elems[0]); idx++)
    {
        range = elems[idx];
        if (range->size && range->start)
        {
            ok = IOPCIRangeListDeallocateSubRange(head, range);
            printf("dealloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
            ok = IOPCIRangeListAllocateSubRange(head, range);
            printf("alloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
        }
    }

    // extend
    range = elems[5];
    range->proposedSize = 2 * range->size;
    ok = IOPCIRangeListAllocateSubRange(head, range);
    printf("extalloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
    range = elems[0];
    range->proposedSize = 2 * range->size;
    ok = IOPCIRangeListAllocateSubRange(head, range);
    printf("extalloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
    range->proposedSize = 2 * range->size;
    ok = IOPCIRangeListAllocateSubRange(head, range, range->start - range->size);
    printf("extalloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);

    IOPCIRangeDump(head);

    exit(0);    
}

#endif
