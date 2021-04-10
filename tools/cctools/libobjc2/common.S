#if defined(__WIN32__) || defined(__APPLE__)
#define CDECL(symbol) _##symbol
#else
#define CDECL(symbol) symbol
#endif

#if __ELF__
#define TYPE_DIRECTIVE(symbol, symboltype) .type symbol, symboltype
#else
#define TYPE_DIRECTIVE(symbol, symboltype)
#endif

