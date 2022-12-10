/*
 * sym.c
 * lockstat
 *
 * Created by Samuel Gosselin on 10/1/14.
 * Copyright 2014 Apple Inc. All rights reserved.
 *
 */

#include <CoreSymbolication/CoreSymbolication.h>
#include <CoreSymbolication/CoreSymbolicationPrivate.h>

static CSSymbolicatorRef 	g_symbolicator;

int
symtab_init(void)
{
	uint32_t symflags = 0x0;
	symflags |= kCSSymbolicatorDefaultCreateFlags;
	symflags |= kCSSymbolicatorUseSlidKernelAddresses;

	/* retrieve the kernel symbolicator */
	g_symbolicator = CSSymbolicatorCreateWithMachKernelFlagsAndNotification(symflags, NULL);
	if (CSIsNull(g_symbolicator)) {
		fprintf(stderr, "could not retrieve the kernel symbolicator\n");
		return -1;
	}

	return 0;
}

char const*
addr_to_sym(uintptr_t addr, uintptr_t *offset, size_t *sizep)
{
	CSSymbolRef symbol;
	CSRange	range;

	assert(offset);
	assert(sizep);

	symbol = CSSymbolicatorGetSymbolWithAddressAtTime(g_symbolicator, addr, kCSNow);
	if (!CSIsNull(symbol)) {
		range = CSSymbolGetRange(symbol);
		*offset = addr - range.location;
		*sizep = range.length;
		return CSSymbolGetName(symbol);
	}

	return NULL;
}

uintptr_t
sym_to_addr(char *name)
{
	CSSymbolRef symbol;

	symbol = CSSymbolicatorGetSymbolWithNameAtTime(g_symbolicator, name, kCSNow);
	if (!CSIsNull(symbol))
		return CSSymbolGetRange(symbol).location;

	return NULL;
}

size_t
sym_size(char *name)
{
	CSSymbolRef symbol;

	symbol = CSSymbolicatorGetSymbolWithNameAtTime(g_symbolicator, name, kCSNow);
	if (!CSIsNull(symbol))
		return CSSymbolGetRange(symbol).length;

	return NULL;
}

