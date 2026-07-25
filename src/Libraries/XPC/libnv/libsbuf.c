#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

struct sbuf {
	char	*s_buf;		/* storage buffer */
	void	*s_unused;	/* binary compatibility, unused */
	int		 s_size;	/* size of storage buffer */
	int		 s_len;		/* current length of string */
	int		 s_flags;	/* flags, unused */
};

struct sbuf *sbuf_new_auto(void) {
	struct sbuf *sb = calloc(1, sizeof(*sb));
	sb->s_buf = malloc(1024);
	sb->s_size = 1024;
	sb->s_len = 0;
	return sb;
}

void sbuf_clear(struct sbuf *sb) {
	free(sb->s_buf);
	sb->s_buf = malloc(1024);
	sb->s_size = 1024;
	sb->s_len = 0;
}

int	sbuf_setpos(struct sbuf *sb, int pos) {
	// Unimplemented.
	return sb->s_len;
}

int	sbuf_bcat(struct sbuf *sb, const void *ptr, size_t len) {
	if ((sb->s_len + len) > sb->s_size) {
		sb->s_size *= 2;
		sb->s_buf = realloc(sb->s_buf, sb->s_size);
	}

	memcpy(sb->s_buf + sb->s_len, ptr, len);
	sb->s_len += len;
	return sb->s_len;
}

int sbuf_bcpy(struct sbuf *sb, const void *ptr, size_t len) {
	if (len > sb->s_size) {
		sb->s_size *= 2;
		sb->s_buf = realloc(sb->s_buf, sb->s_size);
	}

	memcpy(sb->s_buf, ptr, len);
	sb->s_len = (int)len;
	return sb->s_len;
}

int sbuf_cat(struct sbuf *sb, const char *str) {
	return sbuf_bcat(sb, str, (int)strlen(str) * sizeof(char));
}

int sbuf_cpy(struct sbuf *sb, const char *str) {
	return sbuf_bcpy(sb, str, (int)strlen(str) * sizeof(char));
}

int sbuf_vprintf(struct sbuf *sb, const char *fmt, va_list ap) {
	char *str; vasprintf(&str, fmt, ap);
	int ret = sbuf_cat(sb, str);
	free(str);
	return ret;
}

int sbuf_printf(struct sbuf *sb, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = sbuf_vprintf(sb, fmt, ap);
	va_end(ap);
	return ret;
}

int sbuf_putc(struct sbuf *sb, int c) {
	return sbuf_bcat(sb, &c, sizeof(int));
}

int sbuf_trim(struct sbuf *sb) {
	// Don't know what this does.
	return 0;
}

int	sbuf_overflowed(struct sbuf *sb) {
	// Don't know what this does.
	return 0;
}

void sbuf_finish(struct sbuf *sb) {
	// Don't know what this does.
}

char *sbuf_data(struct sbuf *sb) {
	bzero(sb->s_buf + sb->s_len, sb->s_size - sb->s_len);
	return sb->s_buf;
}

int	sbuf_len(struct sbuf *sb) {
	return sb->s_len;
}

int sbuf_done(struct sbuf *sb) {
	// Don't know what this does.
	return 0;
}

void sbuf_delete(struct sbuf *sb) {
	free(sb->s_buf);
	free(sb);
}
