// SPDX-License-Identifier: MIT
// dma-fence-unwrap, yields the plain fences of a dma_fence_chain and its arrays in order
// written from the callers in drm_syncobj.c, drm_atomic_uapi.c and amdgpu_cs.c
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-fence-chain.h>
#include <linux/dma-fence-unwrap.h>
#include <linux/slab.h>

static struct dma_fence *
pd_unwrap_array(struct dma_fence_unwrap *cursor)
{
	cursor->array = dma_fence_chain_contained(cursor->chain);
	cursor->index = 0;
	return dma_fence_array_first(cursor->array);
}

struct dma_fence *
dma_fence_unwrap_first(struct dma_fence *head, struct dma_fence_unwrap *cursor)
{
	cursor->chain = dma_fence_get(head);
	return pd_unwrap_array(cursor);
}

struct dma_fence *
dma_fence_unwrap_next(struct dma_fence_unwrap *cursor)
{
	struct dma_fence *tmp;

	++cursor->index;
	tmp = dma_fence_array_next(cursor->array, cursor->index);
	if (tmp != NULL)
		return tmp;
	cursor->chain = dma_fence_chain_walk(cursor->chain);
	return pd_unwrap_array(cursor);
}

struct dma_fence *
__dma_fence_unwrap_merge(unsigned int num_fences, struct dma_fence **fences,
    struct dma_fence_unwrap *cursors)
{
	struct dma_fence **array, *tmp;
	struct dma_fence_array *result;
	unsigned int i, j, count = 0;

	// count the unsignaled plain fences under every argument
	for (i = 0; i < num_fences; i++) {
		dma_fence_unwrap_for_each(tmp, &cursors[i], fences[i]) {
			if (!dma_fence_is_signaled(tmp))
				count++;
		}
	}
	if (count == 0)
		return dma_fence_get_stub();

	array = kmalloc_array(count, sizeof(*array), GFP_KERNEL);
	if (array == NULL)
		return NULL;

	// keep one fence per context, the later one wins
	count = 0;
	for (i = 0; i < num_fences; i++) {
		dma_fence_unwrap_for_each(tmp, &cursors[i], fences[i]) {
			if (dma_fence_is_signaled(tmp))
				continue;
			for (j = 0; j < count; j++) {
				if (array[j]->context != tmp->context)
					continue;
				if (dma_fence_is_later(tmp, array[j])) {
					dma_fence_put(array[j]);
					array[j] = dma_fence_get(tmp);
				}
				break;
			}
			if (j == count)
				array[count++] = dma_fence_get(tmp);
		}
	}

	if (count == 1) {
		tmp = array[0];
		kfree(array);
		return tmp;
	}
	result = dma_fence_array_create(count, array,
	    dma_fence_context_alloc(1), 1, false);
	if (result == NULL) {
		for (j = 0; j < count; j++)
			dma_fence_put(array[j]);
		kfree(array);
		return NULL;
	}
	return &result->base;
}
