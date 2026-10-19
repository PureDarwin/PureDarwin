// SPDX-License-Identifier: MIT
// device tree nodes for linux drivers, mirrored from xnu's device tree plane with fdt byte order restored

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include "pd_fbsd_os.h"

static struct device_node *sNodes;
// set up on first use, which is single threaded (module init or the first attach)
static struct mutex pd_of_nodes_lock;
static int pd_of_nodes_lock_ready;

static struct mutex *
pd_of_nodes_lock_get(void)
{
	if (!pd_of_nodes_lock_ready) {
		mutex_init(&pd_of_nodes_lock);
		pd_of_nodes_lock_ready = 1;
	}
	return (&pd_of_nodes_lock);
}

static uint32_t
rd32(const uint8_t *p)
{
	uint32_t v;

	memcpy(&v, p, 4);
	return (v);
}

static void
wr_be32(uint8_t *p, uint32_t v)
{
	p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static uint32_t
be32_at(const void *p, int i)
{
	const uint8_t *b = (const uint8_t *)p + 4 * i;

	return ((uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3]);
}

static bool
starts(const char *s, const char *p)
{
	return (strncmp(s, p, strlen(p)) == 0);
}

static bool
ends(const char *s, const char *p)
{
	size_t a = strlen(s), b = strlen(p);

	return (a >= b && strcmp(s + a - b, p) == 0);
}

// the loader's rule for which properties it keeps as strings
static bool
looks_like_strings(const uint8_t *v, uint32_t len)
{
	uint32_t start = 0;

	if (len == 0 || v[len - 1] != 0)
		return (false);
	for (uint32_t i = 0; i < len; i++) {
		if (v[i] == 0) {
			if (i == start)
				return (false);
			start = i + 1;
		} else if (v[i] < 0x20 || v[i] > 0x7e)
			return (false);
	}
	return (true);
}

static uint32_t
entry_u32(void *entry, const char *name, uint32_t def)
{
	const void *d;
	uint32_t len;

	if (entry == NULL || pdos_dt_prop(entry, name, &d, &len) != 0 || len < 4)
		return (def);
	return (rd32(d));
}

// one field of n cells as the loader stored it (n-2 u32 cells then one native u64), back to big endian
static uint32_t
field_to_be(uint8_t *out, const uint8_t *in, uint32_t n)
{
	uint32_t o = 0;

	if (n == 1) {
		wr_be32(out, rd32(in));
		return (4);
	}
	for (uint32_t i = 0; n >= 2 && i < n - 2; i++, o += 4)
		wr_be32(out + o, rd32(in + o));
	if (n >= 2) {
		uint64_t v;
		memcpy(&v, in + o, 8);
		wr_be32(out + o, v >> 32);
		wr_be32(out + o + 4, (uint32_t)v);
		o += 8;
	}
	return (o);
}

static bool
tuples_to_be(uint8_t *out, const uint8_t *in, uint32_t len, const uint32_t *f, int nf)
{
	uint32_t stride = 0, at = 0;

	for (int i = 0; i < nf; i++)
		stride += f[i];
	if (stride == 0 || len % (4 * stride) != 0)
		return (false);
	while (at < len) {
		for (int i = 0; i < nf; i++)
			at += field_to_be(out + at, in + at, f[i]);
	}
	return (true);
}

// the inverse of the loader's import, so drivers see the property exactly as the fdt had it
static void
to_fdt(void *entry, const char *name, const uint8_t *in, uint32_t len, uint8_t *out)
{
	bool numeric = strcmp(name, "reg") == 0 || strcmp(name, "ranges") == 0 ||
	    strcmp(name, "dma-ranges") == 0 || strcmp(name, "phandle") == 0 ||
	    strcmp(name, "linux,phandle") == 0 || strcmp(name, "interrupt-parent") == 0 ||
	    strcmp(name, "interrupts") == 0 || strcmp(name, "interrupts-extended") == 0 ||
	    name[0] == '#';
	void *parent = pdos_dt_parent(entry);

	if ((!numeric && (looks_like_strings(in, len) || ends(name, "-names"))) || len % 4 != 0) {
		memcpy(out, in, len);
		return;
	}
	if (strcmp(name, "reg") == 0) {
		uint32_t f[2] = { entry_u32(parent, "#address-cells", 2), entry_u32(parent, "#size-cells", 1) };
		if (tuples_to_be(out, in, len, f, 2))
			return;
	} else if (strcmp(name, "ranges") == 0 || strcmp(name, "dma-ranges") == 0) {
		uint32_t f[3] = { entry_u32(entry, "#address-cells", 2),
		    entry_u32(parent, "#address-cells", 2), entry_u32(entry, "#size-cells", 1) };
		if (tuples_to_be(out, in, len, f, 3))
			return;
	} else if (len == 8 && (ends(name, "-frequency") || starts(name, "linux,initrd-"))) {
		field_to_be(out, in, 2);
		return;
	}
	for (uint32_t i = 0; i + 4 <= len; i += 4)
		wr_be32(out + i, rd32(in + i));
}

static void
nodes_lock(void)
{
	mutex_lock(pd_of_nodes_lock_get());
}

// the device_node mirroring an IORegistryEntry, made on first use and kept for the kext's life
struct device_node *
pd_of_node_for_entry(void *entry)
{
	struct device_node *np;
	char name[128];

	if (entry == NULL)
		return (NULL);
	nodes_lock();
	for (np = sNodes; np != NULL; np = np->pd_next) {
		if (np->pd_entry == entry) {
			mutex_unlock(pd_of_nodes_lock_get());
			return (np);
		}
	}
	np = kzalloc(sizeof(*np), GFP_KERNEL);
	pdos_dt_name(entry, name, sizeof(name));
	np->full_name = kstrdup(name, GFP_KERNEL);
	np->name = kstrdup(name, GFP_KERNEL);
	if (strchr(np->name, '@') != NULL)
		*strchr((char *)np->name, '@') = 0;
	np->phandle = entry_u32(entry, "phandle", 0);
	np->pd_entry = entry;
	np->pd_next = sNodes;
	sNodes = np;
	mutex_unlock(pd_of_nodes_lock_get());
	np->parent = pd_of_node_for_entry(pdos_dt_parent(entry));
	return (np);
}

struct device_node *
of_node_get(struct device_node *np)
{
	return (np);
}

void
of_node_put(struct device_node *np)
{
}

struct property *
of_find_property(const struct device_node *cnp, const char *name, int *lenp)
{
	struct device_node *np = (struct device_node *)cnp;
	struct property *pp;
	const void *data;
	uint32_t len;

	if (np == NULL)
		return (NULL);
	nodes_lock();
	for (pp = np->properties; pp != NULL; pp = pp->next) {
		if (strcmp(pp->name, name) == 0)
			break;
	}
	mutex_unlock(pd_of_nodes_lock_get());
	if (pp == NULL) {
		if (pdos_dt_prop(np->pd_entry, name, &data, &len) != 0)
			return (NULL);
		pp = kzalloc(sizeof(*pp) + len + 1, GFP_KERNEL);
		pp->name = kstrdup(name, GFP_KERNEL);
		pp->length = len;
		pp->value = (uint8_t *)(pp + 1);
		to_fdt(np->pd_entry, name, data, len, pp->value);
		nodes_lock();
		pp->next = np->properties;
		np->properties = pp;
		mutex_unlock(pd_of_nodes_lock_get());
	}
	if (lenp != NULL)
		*lenp = pp->length;
	return (pp);
}

const void *
of_get_property(const struct device_node *np, const char *name, int *lenp)
{
	struct property *pp = of_find_property(np, name, lenp);

	return (pp != NULL ? pp->value : NULL);
}

bool
of_property_read_bool(const struct device_node *np, const char *name)
{
	return (of_find_property(np, name, NULL) != NULL);
}

int
of_property_read_u32_index(const struct device_node *np, const char *name, u32 index, u32 *out)
{
	int len;
	const void *v = of_get_property(np, name, &len);

	if (v == NULL)
		return (-EINVAL);
	if ((index + 1) * 4 > (u32)len)
		return (-EOVERFLOW);
	*out = be32_at(v, index);
	return (0);
}

int
of_property_read_u32_array(const struct device_node *np, const char *name, u32 *out, size_t n)
{
	int len;
	const void *v = of_get_property(np, name, &len);

	if (v == NULL)
		return (-EINVAL);
	if (n * 4 > (size_t)len)
		return (-EOVERFLOW);
	for (size_t i = 0; i < n; i++)
		out[i] = be32_at(v, i);
	return (0);
}

int
of_property_read_u64(const struct device_node *np, const char *name, u64 *out)
{
	int len;
	const void *v = of_get_property(np, name, &len);

	if (v == NULL)
		return (-EINVAL);
	if (len < 8)
		return (-EOVERFLOW);
	*out = (u64)be32_at(v, 0) << 32 | be32_at(v, 1);
	return (0);
}

int
of_property_read_string_index(const struct device_node *np, const char *name, int index,
    const char **out)
{
	int len;
	const char *v = of_get_property(np, name, &len), *p;

	if (v == NULL)
		return (-EINVAL);
	for (p = v; p < v + len; p += strlen(p) + 1) {
		if (index-- == 0) {
			*out = p;
			return (0);
		}
	}
	return (-ENODATA);
}

int
of_property_read_string(const struct device_node *np, const char *name, const char **out)
{
	return (of_property_read_string_index(np, name, 0, out));
}

int
of_property_match_string(const struct device_node *np, const char *name, const char *str)
{
	const char *s;

	for (int i = 0; of_property_read_string_index(np, name, i, &s) == 0; i++) {
		if (strcmp(s, str) == 0)
			return (i);
	}
	return (-ENODATA);
}

int
of_property_count_elems_of_size(const struct device_node *np, const char *name, int size)
{
	int len;

	if (of_get_property(np, name, &len) == NULL)
		return (-EINVAL);
	return (size > 0 && len % size == 0 ? len / size : -EINVAL);
}

// higher for a match earlier in the node's compatible list, 0 for none
int
of_device_is_compatible(const struct device_node *np, const char *compat)
{
	const char *s;

	for (int i = 0; of_property_read_string_index(np, "compatible", i, &s) == 0; i++) {
		if (strcasecmp(s, compat) == 0)
			return (INT_MAX - i);
	}
	return (0);
}

bool
of_device_is_available(const struct device_node *np)
{
	const char *status;

	if (np == NULL)
		return (false);
	if (of_property_read_string(np, "status", &status) != 0)
		return (true);
	return (strcmp(status, "okay") == 0 || strcmp(status, "ok") == 0);
}

struct device_node *
of_get_parent(const struct device_node *np)
{
	return (np != NULL ? np->parent : NULL);
}

struct device_node *
of_get_next_child(const struct device_node *np, struct device_node *prev)
{
	void *e;
	int i = 0;

	if (np == NULL)
		return (NULL);
	if (prev != NULL) {
		while ((e = pdos_dt_child(np->pd_entry, i++)) != NULL && e != prev->pd_entry)
			;
		if (e == NULL)
			return (NULL);
	}
	return (pd_of_node_for_entry(pdos_dt_child(np->pd_entry, i)));
}

struct device_node *
of_get_child_by_name(const struct device_node *np, const char *name)
{
	struct device_node *child;

	for_each_child_of_node(np, child) {
		if (strcmp(child->name, name) == 0)
			return (child);
	}
	return (NULL);
}

struct device_node *
of_find_node_by_phandle(phandle ph)
{
	return (ph != 0 ? pd_of_node_for_entry(pdos_dt_find_phandle(ph)) : NULL);
}

int
of_parse_phandle_with_args(const struct device_node *np, const char *list, const char *cells,
    int index, struct of_phandle_args *out)
{
	int len, at = 0;
	const void *v = of_get_property(np, list, &len);

	if (v == NULL)
		return (-ENOENT);
	for (int n = 0; at < len / 4; n++) {
		struct device_node *target = of_find_node_by_phandle(be32_at(v, at));
		u32 count = 0;

		if (target == NULL)
			return (-EINVAL);
		if (cells != NULL && of_property_read_u32(target, cells, &count) != 0)
			return (-EINVAL);
		if (count > MAX_PHANDLE_ARGS || at + 1 + (int)count > len / 4)
			return (-EINVAL);
		if (n == index) {
			if (out != NULL) {
				out->np = target;
				out->args_count = count;
				for (u32 i = 0; i < count; i++)
					out->args[i] = be32_at(v, at + 1 + i);
			}
			return (0);
		}
		at += 1 + count;
	}
	return (-ENOENT);
}

int
of_count_phandle_with_args(const struct device_node *np, const char *list, const char *cells)
{
	int n = 0;

	while (of_parse_phandle_with_args(np, list, cells, n, NULL) == 0)
		n++;
	return (n > 0 ? n : -ENOENT);
}

struct device_node *
of_parse_phandle(const struct device_node *np, const char *list, int index)
{
	struct of_phandle_args args;

	if (of_parse_phandle_with_args(np, list, NULL, index, &args) != 0)
		return (NULL);
	return (args.np);
}

// the best entry for the node: the one matching its earliest compatible string
const struct of_device_id *
of_match_node(const struct of_device_id *matches, const struct device_node *np)
{
	const struct of_device_id *best = NULL;
	int score = 0;

	if (matches == NULL || np == NULL)
		return (NULL);
	for (; matches->name[0] || matches->type[0] || matches->compatible[0]; matches++) {
		int s = matches->compatible[0] ? of_device_is_compatible(np, matches->compatible) : 0;
		if (s > score) {
			score = s;
			best = matches;
		}
	}
	return (best);
}

int
of_n_addr_cells(struct device_node *np)
{
	u32 v;

	for (np = np != NULL ? np->parent : NULL; np != NULL; np = np->parent) {
		if (of_property_read_u32(np, "#address-cells", &v) == 0)
			return (v);
	}
	return (2);
}

int
of_n_size_cells(struct device_node *np)
{
	u32 v;

	for (np = np != NULL ? np->parent : NULL; np != NULL; np = np->parent) {
		if (of_property_read_u32(np, "#size-cells", &v) == 0)
			return (v);
	}
	return (1);
}

static u64
read_cells(const void *v, int at, int n)
{
	u64 r = 0;

	for (int i = 0; i < n; i++)
		r = r << 32 | be32_at(v, at + i);
	return (r);
}

// through each bus's ranges up to the root, false when a window does not cover the address
static bool
translate(struct device_node *bus, u64 *addr)
{
	for (; bus != NULL && bus->parent != NULL; bus = bus->parent) {
		int len;
		const void *r = of_get_property(bus, "ranges", &len);
		u32 ca = 2, cs = 1, pa = 2;

		if (r == NULL)
			return (true);
		if (len == 0)
			continue;
		of_property_read_u32(bus, "#address-cells", &ca);
		of_property_read_u32(bus, "#size-cells", &cs);
		of_property_read_u32(bus->parent, "#address-cells", &pa);
		int stride = ca + pa + cs;
		bool hit = false;
		for (int at = 0; stride > 0 && (at + stride) * 4 <= len; at += stride) {
			u64 child = read_cells(r, at, ca), parent = read_cells(r, at + ca, pa);
			u64 size = read_cells(r, at + ca + pa, cs);
			if (*addr >= child && *addr - child < size) {
				*addr = *addr - child + parent;
				hit = true;
				break;
			}
		}
		if (!hit)
			return (false);
	}
	return (true);
}

int
of_address_to_resource(struct device_node *np, int index, struct resource *res)
{
	int len, ac = of_n_addr_cells(np), sc = of_n_size_cells(np);
	const void *reg = of_get_property(np, "reg", &len);
	const char *name = NULL;
	u64 addr, size;

	if (reg == NULL || (index + 1) * (ac + sc) * 4 > len)
		return (-EINVAL);
	addr = read_cells(reg, index * (ac + sc), ac);
	size = read_cells(reg, index * (ac + sc) + ac, sc);
	if (!translate(np->parent, &addr))
		return (-EINVAL);
	of_property_read_string_index(np, "reg-names", index, &name);
	memset(res, 0, sizeof(*res));
	res->start = addr;
	res->end = addr + size - 1;
	res->flags = IORESOURCE_MEM;
	res->name = name != NULL ? name : np->full_name;
	return (0);
}

void __iomem *
of_iomap(struct device_node *np, int index)
{
	struct resource res;

	if (of_address_to_resource(np, index, &res) != 0)
		return (NULL);
	return (ioremap(res.start, resource_size(&res)));
}

const struct of_device_id *
of_match_device(const struct of_device_id *matches, const struct device *dev)
{
	return (dev != NULL ? of_match_node(matches, dev->of_node) : NULL);
}

const void *
of_device_get_match_data(const struct device *dev)
{
	const struct of_device_id *m;

	if (dev == NULL || dev->driver == NULL)
		return (NULL);
	m = of_match_node(dev->driver->of_match_table, dev->of_node);
	return (m != NULL ? m->data : NULL);
}
