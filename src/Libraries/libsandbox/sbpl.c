/*
 * sbpl.c - compiles the Sandbox Profile Language into a kernel profile.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sandbox_profile.h"
#include "sbpl.h"

#define MAX_DEPTH 64

enum node_type { NODE_LIST, NODE_SYMBOL, NODE_STRING, NODE_REGEX, NODE_BOOL };

struct node {
	enum node_type type;
	int line;
	bool boolean;
	char *text;
	struct node **items;
	size_t count;
	size_t capacity;
};

enum value_kind { VALUE_NONE, VALUE_STRING, VALUE_BOOL };

struct value {
	enum value_kind kind;
	const char *string;
	bool boolean;
};

struct binding {
	const char *name;
	struct value value;
};

struct compiler {
	const char *source;
	size_t pos;
	int line;
	bool failed;
	char *error;
	const char *const *parameters;
	struct binding *bindings;
	size_t binding_count;
	size_t binding_capacity;
	struct sb_rule *rules;
	size_t rule_count;
	size_t rule_capacity;
	char *strings;
	size_t strings_size;
	size_t strings_capacity;
};

struct operation {
	const char *name;
	uint32_t ops;
};

static const struct operation operations[] = {
	{ "default", SB_OP_ALL },
	{ "file*", SB_OP_FILE_ALL },
	{ "file-read*", SB_OP_FILE_READ_ALL },
	{ "file-read-data", SB_OP_FILE_READ_DATA },
	{ "file-read-metadata", SB_OP_FILE_READ_METADATA },
	{ "file-read-xattr", SB_OP_FILE_READ_DATA },
	{ "file-write*", SB_OP_FILE_WRITE_ALL },
	{ "file-write-data", SB_OP_FILE_WRITE_DATA },
	{ "file-write-create", SB_OP_FILE_WRITE_CREATE },
	{ "file-write-unlink", SB_OP_FILE_WRITE_UNLINK },
	{ "file-write-setugid", SB_OP_FILE_WRITE_SETUGID },
	{ "file-write-mode", SB_OP_FILE_WRITE_ATTR },
	{ "file-write-owner", SB_OP_FILE_WRITE_ATTR },
	{ "file-write-flags", SB_OP_FILE_WRITE_ATTR },
	{ "file-write-times", SB_OP_FILE_WRITE_ATTR },
	{ "file-write-xattr", SB_OP_FILE_WRITE_ATTR },
	{ "file-write-acl", SB_OP_FILE_WRITE_ATTR },
	{ "file-link", SB_OP_FILE_WRITE_CREATE },
	{ "file-mknod", SB_OP_FILE_WRITE_CREATE },
	{ "process*", SB_OP_PROCESS_EXEC },
	{ "process-exec", SB_OP_PROCESS_EXEC },
	{ "process-exec*", SB_OP_PROCESS_EXEC },
	{ "process-exec-interpreter", SB_OP_PROCESS_EXEC },
	{ "network*", SB_OP_NETWORK_ALL },
	{ "network-outbound", SB_OP_NETWORK_OUTBOUND },
	{ "network-inbound", SB_OP_NETWORK_INBOUND },
	{ "network-bind", SB_OP_NETWORK_INBOUND },
};

/* Recognised but not enforced by the kernel policy: rules for these are dropped. */
static const char *const unenforced_prefixes[] = {
	"process-fork", "process-info", "process-codesigning", "signal", "sysctl",
	"ipc", "system", "mach", "iokit", "user-preference", "darwin-notification",
	"distributed-notification", "job-creation", "lsopen", "nvram", "pseudo-tty",
	"hid-control", "file-ioctl", "file-revoke", "file-search", "file-clone",
	"file-issue-extension", "file-map-executable", "dynamic-code-generation",
	"appleevent-send", "authorization-right-obtain", "opendirectory", "qtn",
	"socket-", "storage-", "boot-arg", "device-", "fs-", "generic-issue-extension",
};

static void
fail(struct compiler *c, int line, const char *format, ...)
{
	char *message = NULL;
	va_list ap;

	if (c->failed) {
		return;
	}
	c->failed = true;

	va_start(ap, format);
	if (vasprintf(&message, format, ap) < 0) {
		message = NULL;
	}
	va_end(ap);

	if (asprintf(&c->error, "line %d: %s", line, message ? message : "out of memory") < 0) {
		c->error = NULL;
	}
	free(message);
}

static struct node *
node_new(struct compiler *c, enum node_type type)
{
	struct node *n = calloc(1, sizeof(*n));

	if (n == NULL) {
		fail(c, c->line, "out of memory");
		return NULL;
	}
	n->type = type;
	n->line = c->line;
	return n;
}

static void
node_free(struct node *n)
{
	if (n == NULL) {
		return;
	}
	for (size_t i = 0; i < n->count; i++) {
		node_free(n->items[i]);
	}
	free(n->items);
	free(n->text);
	free(n);
}

static bool
node_append(struct compiler *c, struct node *list, struct node *item)
{
	if (list->count == list->capacity) {
		size_t capacity = list->capacity ? list->capacity * 2 : 8;
		struct node **items = realloc(list->items, capacity * sizeof(*items));

		if (items == NULL) {
			fail(c, item->line, "out of memory");
			node_free(item);
			return false;
		}
		list->items = items;
		list->capacity = capacity;
	}
	list->items[list->count++] = item;
	return true;
}

static void
skip_space(struct compiler *c)
{
	for (;;) {
		char ch = c->source[c->pos];

		if (ch == '\n') {
			c->line++;
			c->pos++;
		} else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\f') {
			c->pos++;
		} else if (ch == ';') {
			while (c->source[c->pos] != '\0' && c->source[c->pos] != '\n') {
				c->pos++;
			}
		} else {
			return;
		}
	}
}

static bool
buffer_push(char **buffer, size_t *length, size_t *capacity, char ch)
{
	if (*length == *capacity) {
		size_t grown = *capacity ? *capacity * 2 : 32;
		char *p = realloc(*buffer, grown);

		if (p == NULL) {
			return false;
		}
		*buffer = p;
		*capacity = grown;
	}
	(*buffer)[(*length)++] = ch;
	return true;
}

/* A regex literal (#"...") keeps its backslashes, except the one escaping a quote. */
static char *
parse_quoted(struct compiler *c, bool raw)
{
	int start_line = c->line;
	char *buffer = NULL;
	size_t length = 0, capacity = 0;

	c->pos++;
	for (;;) {
		char ch = c->source[c->pos];

		if (ch == '\0') {
			free(buffer);
			fail(c, start_line, "unterminated string");
			return NULL;
		}
		c->pos++;
		if (ch == '"') {
			break;
		}
		if (ch == '\n') {
			c->line++;
		}
		if (ch == '\\' && c->source[c->pos] != '\0') {
			char next = c->source[c->pos++];

			if (next == '"') {
				ch = '"';
			} else if (raw) {
				if (!buffer_push(&buffer, &length, &capacity, '\\')) {
					goto oom;
				}
				ch = next;
			} else if (next == 'n') {
				ch = '\n';
			} else if (next == 't') {
				ch = '\t';
			} else {
				ch = next;
			}
		}
		if (!buffer_push(&buffer, &length, &capacity, ch)) {
			goto oom;
		}
	}
	if (buffer_push(&buffer, &length, &capacity, '\0')) {
		return buffer;
	}
oom:
	free(buffer);
	fail(c, start_line, "out of memory");
	return NULL;
}

static struct node *
parse_node(struct compiler *c, unsigned depth)
{
	struct node *n;
	char ch;

	if (depth > MAX_DEPTH) {
		fail(c, c->line, "nesting too deep");
		return NULL;
	}
	skip_space(c);
	ch = c->source[c->pos];

	if (ch == '\0') {
		fail(c, c->line, "unexpected end of profile");
		return NULL;
	}
	if (ch == ')') {
		fail(c, c->line, "unexpected ')'");
		return NULL;
	}
	if (ch == '(') {
		struct node *list = node_new(c, NODE_LIST);

		if (list == NULL) {
			return NULL;
		}
		c->pos++;
		for (;;) {
			struct node *item;

			skip_space(c);
			if (c->source[c->pos] == ')') {
				c->pos++;
				return list;
			}
			if (c->source[c->pos] == '\0') {
				fail(c, list->line, "unterminated list");
				node_free(list);
				return NULL;
			}
			item = parse_node(c, depth + 1);
			if (item == NULL || !node_append(c, list, item)) {
				node_free(list);
				return NULL;
			}
		}
	}
	if (ch == '"' || (ch == '#' && c->source[c->pos + 1] == '"')) {
		bool raw = (ch == '#');

		n = node_new(c, raw ? NODE_REGEX : NODE_STRING);
		if (n == NULL) {
			return NULL;
		}
		if (raw) {
			c->pos++;
		}
		n->text = parse_quoted(c, raw);
		if (n->text == NULL) {
			node_free(n);
			return NULL;
		}
		return n;
	}

	size_t start = c->pos;
	while ((ch = c->source[c->pos]) != '\0' && strchr(" \t\r\n\f();\"", ch) == NULL) {
		c->pos++;
	}
	n = node_new(c, NODE_SYMBOL);
	if (n == NULL) {
		return NULL;
	}
	n->text = strndup(c->source + start, c->pos - start);
	if (n->text == NULL) {
		fail(c, n->line, "out of memory");
		node_free(n);
		return NULL;
	}
	if (strcmp(n->text, "#t") == 0 || strcmp(n->text, "#f") == 0) {
		n->type = NODE_BOOL;
		n->boolean = (n->text[1] == 't');
	}
	return n;
}

static struct value
lookup_param(struct compiler *c, const char *name)
{
	struct value v = { VALUE_NONE, NULL, false };

	if (c->parameters == NULL) {
		return v;
	}
	for (size_t i = 0; c->parameters[i] != NULL && c->parameters[i + 1] != NULL; i += 2) {
		if (strcmp(c->parameters[i], name) == 0) {
			v.kind = VALUE_STRING;
			v.string = c->parameters[i + 1];
		}
	}
	return v;
}

static struct value
eval(struct compiler *c, const struct node *n)
{
	struct value v = { VALUE_NONE, NULL, false };

	switch (n->type) {
	case NODE_STRING:
	case NODE_REGEX:
		v.kind = VALUE_STRING;
		v.string = n->text;
		return v;
	case NODE_BOOL:
		v.kind = VALUE_BOOL;
		v.boolean = n->boolean;
		return v;
	case NODE_SYMBOL:
		for (size_t i = c->binding_count; i-- > 0;) {
			if (strcmp(c->bindings[i].name, n->text) == 0) {
				return c->bindings[i].value;
			}
		}
		fail(c, n->line, "unbound variable '%s'", n->text);
		return v;
	case NODE_LIST:
		if (n->count == 2 && n->items[0]->type == NODE_SYMBOL &&
		    strcmp(n->items[0]->text, "param") == 0) {
			struct value key = eval(c, n->items[1]);

			if (key.kind != VALUE_STRING) {
				fail(c, n->line, "param expects a string");
				return v;
			}
			return lookup_param(c, key.string);
		}
		fail(c, n->line, "unsupported expression");
		return v;
	}
	return v;
}

static bool
truthy(struct value v)
{
	return v.kind == VALUE_STRING || (v.kind == VALUE_BOOL && v.boolean);
}

static void
emit_rule(struct compiler *c, int line, uint32_t ops, uint8_t action,
    uint8_t filter, const char *string)
{
	struct sb_rule rule = { .ops = ops, .action = action, .filter = filter };

	if (string != NULL) {
		size_t length = strlen(string);

		if (c->strings_size + length + 1 > SB_PROFILE_MAX_SIZE) {
			fail(c, line, "profile too large");
			return;
		}
		if (c->strings_size + length + 1 > c->strings_capacity) {
			size_t capacity = c->strings_capacity ? c->strings_capacity : 1024;
			char *p;

			while (capacity < c->strings_size + length + 1) {
				capacity *= 2;
			}
			p = realloc(c->strings, capacity);
			if (p == NULL) {
				fail(c, line, "out of memory");
				return;
			}
			c->strings = p;
			c->strings_capacity = capacity;
		}
		memcpy(c->strings + c->strings_size, string, length + 1);
		rule.str_offset = (uint32_t)c->strings_size;
		rule.str_length = (uint32_t)length;
		c->strings_size += length + 1;
	}

	if (c->rule_count == c->rule_capacity) {
		size_t capacity = c->rule_capacity ? c->rule_capacity * 2 : 64;
		struct sb_rule *rules = realloc(c->rules, capacity * sizeof(*rules));

		if (rules == NULL) {
			fail(c, line, "out of memory");
			return;
		}
		c->rules = rules;
		c->rule_capacity = capacity;
	}
	c->rules[c->rule_count++] = rule;
}

/* The kernel matches this subset: no groups, alternation or counted repetition. */
static bool
regex_supported(const char *re)
{
	for (const char *p = re; *p != '\0'; p++) {
		switch (*p) {
		case '\\':
			if (p[1] == '\0') {
				return false;
			}
			p++;
			break;
		case '(': case ')': case '|': case '{': case '}':
			return false;
		case '[':
			p++;
			if (*p == '^') {
				p++;
			}
			if (*p == ']') {
				p++;
			}
			while (*p != '\0' && *p != ']') {
				if (*p == '\\' && p[1] != '\0') {
					p++;
				}
				p++;
			}
			if (*p != ']') {
				return false;
			}
			break;
		default:
			break;
		}
	}
	return true;
}

static void
compile_address(struct compiler *c, const struct node *f, uint32_t ops, uint8_t action)
{
	const char *kind;

	if (f->count < 2 || f->items[1]->type != NODE_SYMBOL) {
		fail(c, f->line, "%s expects an address type", f->items[0]->text);
		return;
	}
	if (ops == 0) {
		return;
	}
	kind = f->items[1]->text;

	if (strcmp(kind, "ip") == 0 || strcmp(kind, "tcp") == 0 || strcmp(kind, "udp") == 0) {
		/* Hosts and ports other than localhost are not told apart. */
		uint8_t filter = SB_FILTER_IP;

		if (f->count >= 3) {
			struct value v = eval(c, f->items[2]);

			if (c->failed) {
				return;
			}
			if (v.kind == VALUE_STRING && strncmp(v.string, "localhost:", 10) == 0) {
				filter = SB_FILTER_IP_LOCALHOST;
			}
		}
		emit_rule(c, f->line, ops, action, filter, NULL);
		return;
	}
	if (strcmp(kind, "unix-socket") == 0) {
		for (size_t i = 2; i < f->count; i++) {
			const struct node *p = f->items[i];
			struct value v;

			if (p->type != NODE_LIST || p->count != 2 || p->items[0]->type != NODE_SYMBOL ||
			    strcmp(p->items[0]->text, "path-literal") != 0) {
				fail(c, p->line, "unix-socket expects (path-literal \"...\")");
				return;
			}
			v = eval(c, p->items[1]);
			if (c->failed) {
				return;
			}
			if (v.kind == VALUE_STRING) {
				emit_rule(c, p->line, ops, action, SB_FILTER_UNIX_LITERAL, v.string);
			}
		}
		return;
	}
	fail(c, f->line, "unsupported address type '%s'", kind);
}

static void
compile_filter(struct compiler *c, const struct node *f, uint32_t ops, uint8_t action, unsigned depth)
{
	const char *name;
	uint8_t filter;

	if (depth > MAX_DEPTH) {
		fail(c, f->line, "nesting too deep");
		return;
	}
	if (f->type != NODE_LIST || f->count == 0 || f->items[0]->type != NODE_SYMBOL) {
		fail(c, f->line, "expected a filter");
		return;
	}
	name = f->items[0]->text;

	if (strcmp(name, "require-any") == 0) {
		for (size_t i = 1; i < f->count && !c->failed; i++) {
			compile_filter(c, f->items[i], ops, action, depth + 1);
		}
		return;
	}
	if (strcmp(name, "require-all") == 0) {
		if (f->count == 2) {
			compile_filter(c, f->items[1], ops, action, depth + 1);
		} else {
			fail(c, f->line, "require-all with several filters is not supported");
		}
		return;
	}
	if (strcmp(name, "remote") == 0 || strcmp(name, "local") == 0) {
		compile_address(c, f, ops & SB_OP_NETWORK_ALL, action);
		return;
	}
	if (strcmp(name, "literal") == 0 || strcmp(name, "path-literal") == 0) {
		filter = SB_FILTER_LITERAL;
	} else if (strcmp(name, "subpath") == 0) {
		filter = SB_FILTER_SUBPATH;
	} else if (strcmp(name, "regex") == 0 || strcmp(name, "path-regex") == 0) {
		filter = SB_FILTER_REGEX;
	} else {
		fail(c, f->line, "unsupported filter '%s'", name);
		return;
	}

	for (size_t i = 1; i < f->count; i++) {
		struct value v = eval(c, f->items[i]);

		if (c->failed) {
			return;
		}
		if (v.kind == VALUE_NONE) {
			/* An unset parameter matches nothing. */
			continue;
		}
		if (v.kind != VALUE_STRING) {
			fail(c, f->items[i]->line, "%s expects a string", name);
			return;
		}
		if (filter == SB_FILTER_REGEX && !regex_supported(v.string)) {
			fail(c, f->items[i]->line, "unsupported regular expression \"%s\"", v.string);
			return;
		}
		if (filter == SB_FILTER_SUBPATH) {
			char *path = strdup(v.string);
			size_t length;

			if (path == NULL) {
				fail(c, f->line, "out of memory");
				return;
			}
			length = strlen(path);
			while (length > 1 && path[length - 1] == '/') {
				path[--length] = '\0';
			}
			emit_rule(c, f->line, ops, action, filter, path);
			free(path);
		} else {
			emit_rule(c, f->line, ops, action, filter, v.string);
		}
	}
}

static bool
lookup_operation(const char *name, uint32_t *ops)
{
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		if (strcmp(operations[i].name, name) == 0) {
			*ops = operations[i].ops;
			return true;
		}
	}
	for (size_t i = 0; i < sizeof(unenforced_prefixes) / sizeof(unenforced_prefixes[0]); i++) {
		if (strncmp(name, unenforced_prefixes[i], strlen(unenforced_prefixes[i])) == 0) {
			*ops = 0;
			return true;
		}
	}
	return false;
}

static void
compile_rule(struct compiler *c, const struct node *form, uint8_t action)
{
	uint32_t ops = 0;
	bool had_operation = false, had_filter = false;
	size_t i = 1;

	for (; i < form->count && form->items[i]->type == NODE_SYMBOL; i++) {
		uint32_t bits;

		if (!lookup_operation(form->items[i]->text, &bits)) {
			fail(c, form->items[i]->line, "unknown operation '%s'", form->items[i]->text);
			return;
		}
		ops |= bits;
		had_operation = true;
	}
	if (!had_operation) {
		fail(c, form->line, "%s needs an operation", form->items[0]->text);
		return;
	}
	if (ops == 0) {
		return;
	}

	for (; i < form->count && !c->failed; i++) {
		const struct node *f = form->items[i];

		if (f->type == NODE_LIST && f->count > 0 && f->items[0]->type == NODE_SYMBOL &&
		    strcmp(f->items[0]->text, "with") == 0) {
			continue;
		}
		had_filter = true;
		compile_filter(c, f, ops, action, 0);
	}
	if (!had_filter) {
		emit_rule(c, form->line, ops, action, SB_FILTER_ANY, NULL);
	}
}

static void
compile_form(struct compiler *c, const struct node *form, unsigned depth)
{
	const char *head;

	if (c->failed) {
		return;
	}
	if (depth > MAX_DEPTH) {
		fail(c, form->line, "nesting too deep");
		return;
	}
	if (form->type != NODE_LIST || form->count == 0 || form->items[0]->type != NODE_SYMBOL) {
		fail(c, form->line, "expected a form");
		return;
	}
	head = form->items[0]->text;

	if (strcmp(head, "version") == 0 || strcmp(head, "debug") == 0 || strcmp(head, "trace") == 0) {
		return;
	}
	if (strcmp(head, "allow") == 0) {
		compile_rule(c, form, SB_ACTION_ALLOW);
		return;
	}
	if (strcmp(head, "deny") == 0) {
		compile_rule(c, form, SB_ACTION_DENY);
		return;
	}
	if (strcmp(head, "define") == 0) {
		struct value v;

		if (form->count != 3 || form->items[1]->type != NODE_SYMBOL) {
			fail(c, form->line, "define expects a name and a value");
			return;
		}
		v = eval(c, form->items[2]);
		if (c->failed) {
			return;
		}
		if (c->binding_count == c->binding_capacity) {
			size_t capacity = c->binding_capacity ? c->binding_capacity * 2 : 8;
			struct binding *bindings = realloc(c->bindings, capacity * sizeof(*bindings));

			if (bindings == NULL) {
				fail(c, form->line, "out of memory");
				return;
			}
			c->bindings = bindings;
			c->binding_capacity = capacity;
		}
		c->bindings[c->binding_count].name = form->items[1]->text;
		c->bindings[c->binding_count].value = v;
		c->binding_count++;
		return;
	}
	if (strcmp(head, "if") == 0) {
		struct value v;

		if (form->count != 3 && form->count != 4) {
			fail(c, form->line, "if expects a condition and one or two branches");
			return;
		}
		v = eval(c, form->items[1]);
		if (c->failed) {
			return;
		}
		if (truthy(v)) {
			compile_form(c, form->items[2], depth + 1);
		} else if (form->count == 4) {
			compile_form(c, form->items[3], depth + 1);
		}
		return;
	}
	if (strcmp(head, "begin") == 0) {
		for (size_t i = 1; i < form->count; i++) {
			compile_form(c, form->items[i], depth + 1);
		}
		return;
	}
	fail(c, form->line, "unsupported form '%s'", head);
}

int
sbpl_compile(const char *source, const char *const parameters[],
    void **profile, size_t *size, char **error)
{
	struct compiler c = { .source = source, .line = 1, .parameters = parameters };
	struct node *root = node_new(&c, NODE_LIST);
	uint8_t *blob = NULL;
	size_t total = 0;

	*profile = NULL;
	*size = 0;
	*error = NULL;

	if (root != NULL) {
		for (;;) {
			struct node *n;

			skip_space(&c);
			if (source[c.pos] == '\0') {
				break;
			}
			n = parse_node(&c, 0);
			if (n == NULL || !node_append(&c, root, n)) {
				break;
			}
		}
		for (size_t i = 0; i < root->count && !c.failed; i++) {
			compile_form(&c, root->items[i], 0);
		}
	}

	if (!c.failed) {
		size_t rules_size = c.rule_count * sizeof(struct sb_rule);
		struct sb_profile_header header = {
			.magic = SB_PROFILE_MAGIC,
			.version = SB_PROFILE_VERSION,
			.default_action = SB_ACTION_ALLOW,
			.rule_count = (uint32_t)c.rule_count,
			.strings_size = (uint32_t)c.strings_size,
		};

		total = sizeof(header) + rules_size + c.strings_size;
		if (total > SB_PROFILE_MAX_SIZE) {
			fail(&c, c.line, "profile too large");
		} else if ((blob = malloc(total)) == NULL) {
			fail(&c, c.line, "out of memory");
		} else {
			memcpy(blob, &header, sizeof(header));
			if (rules_size != 0) {
				memcpy(blob + sizeof(header), c.rules, rules_size);
			}
			if (c.strings_size != 0) {
				memcpy(blob + sizeof(header) + rules_size, c.strings, c.strings_size);
			}
		}
	}

	node_free(root);
	free(c.bindings);
	free(c.rules);
	free(c.strings);

	if (c.failed) {
		free(blob);
		*error = c.error;
		return -1;
	}
	*profile = blob;
	*size = total;
	return 0;
}
