// SPDX-License-Identifier: MIT
// device tree nodes over xnu's device tree plane, with property data in fdt byte order (pd_linux_of.c)
#ifndef _LINUXKPI_LINUX_OF_H
#define	_LINUXKPI_LINUX_OF_H

#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mod_devicetable.h>

typedef u32 phandle;

struct property {
	char		*name;
	int		length;
	void		*value;
	struct property	*next;
};

struct device_node {
	const char		*name;
	const char		*full_name;
	phandle			phandle;
	struct property		*properties;
	struct device_node	*parent;
	struct kobject		kobj;
	// the IORegistryEntry this node mirrors
	void			*pd_entry;
	struct device_node	*pd_next;
};

#define	MAX_PHANDLE_ARGS	16
struct of_phandle_args {
	struct device_node	*np;
	int			args_count;
	uint32_t		args[MAX_PHANDLE_ARGS];
};

struct device_node *of_node_get(struct device_node *);
void of_node_put(struct device_node *);
struct property *of_find_property(const struct device_node *, const char *, int *);
const void *of_get_property(const struct device_node *, const char *, int *);
bool of_property_read_bool(const struct device_node *, const char *);
#define	of_property_present(np, name)	of_property_read_bool((np), (name))
// the match table pointer, device tree support is always built in here
#define	of_match_ptr(p)	(p)
int of_property_read_u32_index(const struct device_node *, const char *, u32, u32 *);
int of_property_read_u32_array(const struct device_node *, const char *, u32 *, size_t);
int of_property_read_u64(const struct device_node *, const char *, u64 *);
int of_property_read_string(const struct device_node *, const char *, const char **);
int of_property_read_string_index(const struct device_node *, const char *, int, const char **);
int of_property_match_string(const struct device_node *, const char *, const char *);
int of_property_count_elems_of_size(const struct device_node *, const char *, int);
int of_device_is_compatible(const struct device_node *, const char *);
bool of_device_is_available(const struct device_node *);
struct device_node *of_get_parent(const struct device_node *);
struct device_node *of_get_next_child(const struct device_node *, struct device_node *);
struct device_node *of_get_child_by_name(const struct device_node *, const char *);
struct device_node *of_find_node_by_phandle(phandle);
struct device_node *of_find_compatible_node(struct device_node *from, const char *type, const char *compat);
struct device_node *of_parse_phandle(const struct device_node *, const char *, int);
int of_parse_phandle_with_args(const struct device_node *, const char *, const char *, int,
    struct of_phandle_args *);
int of_count_phandle_with_args(const struct device_node *, const char *, const char *);
const struct of_device_id *of_match_node(const struct of_device_id *, const struct device_node *);
int of_n_addr_cells(struct device_node *);
int of_n_size_cells(struct device_node *);

static inline int
of_property_read_u32(const struct device_node *np, const char *name, u32 *out)
{
	return (of_property_read_u32_index(np, name, 0, out));
}

static inline int
of_property_count_u32_elems(const struct device_node *np, const char *name)
{
	return (of_property_count_elems_of_size(np, name, sizeof(u32)));
}

static inline int
of_property_count_strings(const struct device_node *np, const char *name)
{
	int n = 0;
	const char *s;

	while (of_property_read_string_index(np, name, n, &s) == 0)
		n++;
	return (n > 0 ? n : -EINVAL);
}

#define	for_each_child_of_node(parent, child)				\
	for ((child) = of_get_next_child((parent), NULL); (child) != NULL; \
	    (child) = of_get_next_child((parent), (child)))
#define	for_each_available_child_of_node(parent, child)			\
	for_each_child_of_node(parent, child) if (!of_device_is_available(child)) {} else

static inline const char *
of_node_full_name(const struct device_node *np)
{
	return (np != NULL ? np->full_name : "<no-node>");
}

static inline bool
is_of_node(const void *fwnode)
{
	return (false);
}

#endif
