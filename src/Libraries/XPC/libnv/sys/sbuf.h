/*
 * Real FreeBSD ships these declarations in a real <sys/sbuf.h>; our
 * libsbuf.c only ever shipped the implementation, not a matching header, so
 * every caller (xpc_misc.c) hit implicit-function-declaration errors under a
 * modern compiler. struct sbuf's layout matches libsbuf.c's own definition
 * exactly (it must - callers there dereference these fields nowhere, only
 * pass struct sbuf* opaquely, so this is just a forward-compatible mirror).
 */
#ifndef _PD_SYS_SBUF_H_
#define _PD_SYS_SBUF_H_

#include <stdarg.h>
#include <stddef.h>

struct sbuf {
    char    *s_buf;
    void    *s_unused;
    int      s_size;
    int      s_len;
    int      s_flags;
};

struct sbuf *sbuf_new_auto(void);
void         sbuf_clear(struct sbuf *sb);
int          sbuf_setpos(struct sbuf *sb, int pos);
int          sbuf_bcat(struct sbuf *sb, const void *ptr, size_t len);
int          sbuf_bcpy(struct sbuf *sb, const void *ptr, size_t len);
int          sbuf_cat(struct sbuf *sb, const char *str);
int          sbuf_cpy(struct sbuf *sb, const char *str);
int          sbuf_vprintf(struct sbuf *sb, const char *fmt, va_list ap);
int          sbuf_printf(struct sbuf *sb, const char *fmt, ...);
int          sbuf_putc(struct sbuf *sb, int c);
int          sbuf_trim(struct sbuf *sb);
int          sbuf_overflowed(struct sbuf *sb);
void         sbuf_finish(struct sbuf *sb);
char        *sbuf_data(struct sbuf *sb);
int          sbuf_len(struct sbuf *sb);
int          sbuf_done(struct sbuf *sb);
void         sbuf_delete(struct sbuf *sb);

#endif /* _PD_SYS_SBUF_H_ */
