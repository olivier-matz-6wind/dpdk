/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation.
 * Copyright(c) 2016 6WIND S.A.
 */

#include <stdio.h>
#include <string.h>

#include <rte_string_fns.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <rte_dev.h>
#include <rte_bus_vdev.h>

#include "rte_mempool_trace.h"

#define RTE_MEMPOOL_OPS_MZNAME "rte_mempool_ops"

/* indirect jump table to support external memory pools. */
struct rte_mempool_ops_table rte_mempool_ops_table = {
	.sl =  RTE_SPINLOCK_INITIALIZER,
	.num_ops = 0
};

/* shared mempool ops table, for multiprocess support */
struct rte_mempool_shared_ops_table {
	size_t count;
	struct {
		char name[RTE_MEMPOOL_OPS_NAMESIZE];
	} ops[RTE_MEMPOOL_MAX_OPS_IDX];
};
static struct rte_mempool_shared_ops_table *shared_ops_table;

/* add a new ops struct in rte_mempool_ops_table, return its index. */
int
rte_mempool_register_ops(const struct rte_mempool_ops *h)
{
	struct rte_mempool_ops *ops;
	int16_t ops_index;
	unsigned int count, i;

	printf("register %s\n", h->name);
	rte_spinlock_lock(&rte_mempool_ops_table.sl);

	if (rte_mempool_ops_table.num_ops >=
			RTE_MEMPOOL_MAX_OPS_IDX) {
		rte_spinlock_unlock(&rte_mempool_ops_table.sl);
		RTE_LOG(ERR, MEMPOOL,
			"Maximum number of mempool ops structs exceeded\n");
		return -ENOSPC;
	}

	if (h->alloc == NULL || h->enqueue == NULL ||
			h->dequeue == NULL || h->get_count == NULL) {
		rte_spinlock_unlock(&rte_mempool_ops_table.sl);
		RTE_LOG(ERR, MEMPOOL,
			"Missing callback while registering mempool ops\n");
		return -EINVAL;
	}

	if (strlen(h->name) >= sizeof(ops->name) - 1) {
		rte_spinlock_unlock(&rte_mempool_ops_table.sl);
		RTE_LOG(DEBUG, MEMPOOL, "%s(): mempool_ops <%s>: name too long\n",
				__func__, h->name);
		rte_errno = ENAMETOOLONG;
		return -ENAMETOOLONG;
	}

	for (i = 0; i < rte_mempool_ops_table.num_ops; i++) {
		if (!strcmp(h->name, rte_mempool_ops_table.ops[i].name)) {
			rte_spinlock_unlock(&rte_mempool_ops_table.sl);
			RTE_LOG(DEBUG, MEMPOOL,
				"%s(): mempool_ops <%s>: already registered\n",
				__func__, h->name);
			rte_errno = EEXIST;
			return -EEXIST;
		}
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY ||
			shared_ops_table == NULL) {
		ops_index = rte_mempool_ops_table.num_ops;
	} else {
		/* lookup in shared memory to get the same index */
		count = shared_ops_table->count;
		rte_rmb();
		for (i = 0; i < count; i++) {
			if (!strcmp(h->name, shared_ops_table->ops[i].name))
				break;
		}
		if (i == count) {
			rte_spinlock_unlock(&rte_mempool_ops_table.sl);
			RTE_LOG(DEBUG, MEMPOOL,
				"%s(): mempool_ops <%s>: not registered in primary process\n",
				__func__, h->name);
			rte_errno = EEXIST;
			return -EEXIST;
		}
		ops_index = i;
	}

	rte_mempool_ops_table.num_ops = ops_index + 1;
	ops = &rte_mempool_ops_table.ops[ops_index];
	strlcpy(ops->name, h->name, sizeof(ops->name));
	ops->alloc = h->alloc;
	ops->free = h->free;
	ops->enqueue = h->enqueue;
	ops->dequeue = h->dequeue;
	ops->get_count = h->get_count;
	ops->calc_mem_size = h->calc_mem_size;
	ops->populate = h->populate;
	ops->get_info = h->get_info;
	ops->dequeue_contig_blocks = h->dequeue_contig_blocks;

	/* update shared memory */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY &&
			shared_ops_table != NULL) {
		strlcpy(shared_ops_table->ops[ops_index].name, h->name,
			sizeof(ops->name));
		rte_wmb();
		shared_ops_table->count++;
	}

	rte_spinlock_unlock(&rte_mempool_ops_table.sl);

	return ops_index;
}

/* wrapper to allocate an external mempool's private (pool) data. */
int
rte_mempool_ops_alloc(struct rte_mempool *mp)
{
	struct rte_mempool_ops *ops;

	rte_mempool_trace_ops_alloc(mp);
	ops = rte_mempool_get_ops(mp->ops_index);
	return ops->alloc(mp);
}

/* wrapper to free an external pool ops. */
void
rte_mempool_ops_free(struct rte_mempool *mp)
{
	struct rte_mempool_ops *ops;

	rte_mempool_trace_ops_free(mp);
	ops = rte_mempool_get_ops(mp->ops_index);
	if (ops->free == NULL)
		return;
	ops->free(mp);
}

/* wrapper to get available objects in an external mempool. */
unsigned int
rte_mempool_ops_get_count(const struct rte_mempool *mp)
{
	struct rte_mempool_ops *ops;

	ops = rte_mempool_get_ops(mp->ops_index);
	return ops->get_count(mp);
}

/* wrapper to calculate the memory size required to store given number
 * of objects
 */
ssize_t
rte_mempool_ops_calc_mem_size(const struct rte_mempool *mp,
				uint32_t obj_num, uint32_t pg_shift,
				size_t *min_chunk_size, size_t *align)
{
	struct rte_mempool_ops *ops;

	ops = rte_mempool_get_ops(mp->ops_index);

	if (ops->calc_mem_size == NULL)
		return rte_mempool_op_calc_mem_size_default(mp, obj_num,
				pg_shift, min_chunk_size, align);

	return ops->calc_mem_size(mp, obj_num, pg_shift, min_chunk_size, align);
}

/* wrapper to populate memory pool objects using provided memory chunk */
int
rte_mempool_ops_populate(struct rte_mempool *mp, unsigned int max_objs,
				void *vaddr, rte_iova_t iova, size_t len,
				rte_mempool_populate_obj_cb_t *obj_cb,
				void *obj_cb_arg)
{
	struct rte_mempool_ops *ops;

	ops = rte_mempool_get_ops(mp->ops_index);

	rte_mempool_trace_ops_populate(mp, max_objs, vaddr, iova, len, obj_cb,
		obj_cb_arg);
	if (ops->populate == NULL)
		return rte_mempool_op_populate_default(mp, max_objs, vaddr,
						       iova, len, obj_cb,
						       obj_cb_arg);

	return ops->populate(mp, max_objs, vaddr, iova, len, obj_cb,
			     obj_cb_arg);
}

/* wrapper to get additional mempool info */
int
rte_mempool_ops_get_info(const struct rte_mempool *mp,
			 struct rte_mempool_info *info)
{
	struct rte_mempool_ops *ops;

	ops = rte_mempool_get_ops(mp->ops_index);

	RTE_FUNC_PTR_OR_ERR_RET(ops->get_info, -ENOTSUP);
	return ops->get_info(mp, info);
}


/* sets mempool ops previously registered by rte_mempool_register_ops. */
int
rte_mempool_set_ops_byname(struct rte_mempool *mp, const char *name,
	void *pool_config)
{
	struct rte_mempool_ops *ops = NULL;
	unsigned i;

	/* too late, the mempool is already populated. */
	if (mp->flags & MEMPOOL_F_POOL_CREATED)
		return -EEXIST;

	for (i = 0; i < rte_mempool_ops_table.num_ops; i++) {
		if (!strcmp(name,
				rte_mempool_ops_table.ops[i].name)) {
			ops = &rte_mempool_ops_table.ops[i];
			break;
		}
	}

	if (ops == NULL)
		return -EINVAL;

	mp->ops_index = i;
	mp->pool_config = pool_config;
	rte_mempool_trace_set_ops_byname(mp, name, pool_config);
	return 0;
}

/* Allocate and initialize the shared memory. */
static void
rte_mempool_ops_shm_init(void)
{
	const struct rte_memzone *mz;

	printf("rte_mempool_ops_shm_init\n");

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(RTE_MEMPOOL_OPS_MZNAME,
					sizeof(*shared_ops_table),
					SOCKET_ID_ANY, 0);
	} else {
		mz = rte_memzone_lookup(RTE_MEMPOOL_OPS_MZNAME);
	}
	if (mz == NULL) {
		RTE_LOG(ERR, MEMPOOL,
			"Failed to register shared memzone for mempool ops\n");
		return;
	}

	shared_ops_table = mz->addr;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* init free_space, keep it sync'd with
		 * rte_mbuf_dynfield_copy().
		 */
		memset(shared_ops_table, 0, sizeof(*shared_ops_table));
	}
}

static void
rte_mempool_ops_bus_scan(void *arg)
{
	static struct rte_devargs devargs = { 0 };
	struct rte_devargs *pdevargs = &devargs;

	(void)arg;
	printf("bus scan\n");
	devargs.bus = rte_bus_find_by_name("vdev");
	devargs.type = RTE_DEVTYPE_VIRTUAL;
	devargs.policy = RTE_DEV_WHITELISTED;
	snprintf(devargs.name, sizeof(devargs.name), "%s", "mempool_ops_0");

	rte_devargs_insert(&pdevargs);
}

static int
mempool_ops_probe(struct rte_vdev_device *vdev)
{
	(void)vdev;
	printf("probe\n");
	rte_mempool_ops_shm_init();
	return 0;
}

static struct rte_vdev_driver mempool_ops_drv = {
	.probe = mempool_ops_probe,
	.remove = NULL,
};

RTE_PMD_REGISTER_VDEV(mempool_ops, mempool_ops_drv);

void
rte_mempool_ops_init(void)
{
	static int initialized = 0;

	if (initialized)
		return;

	printf("rte_mempool_ops_init\n");
	initialized = 1;
	/* register a callback that will be invoked once memory subsystem is
	 * initialized.
	 */
	rte_vdev_add_custom_scan(rte_mempool_ops_bus_scan, NULL);
}
