/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 6WIND S.A.
 */

#ifndef _RTE_MEMPOOL_DRIVER_H_
#define _RTE_MEMPOOL_DRIVER_H_

#include <rte_mempool.h>
#include <rte_dev.h>
#include <rte_bus_vdev.h>

/**
 * Macro to statically register the ops of a mempool handler.
 * Note that the rte_mempool_register_ops fails silently here when
 * more than RTE_MEMPOOL_MAX_OPS_IDX is registered.
 */
#define RTE_MEMPOOL_REGISTER_OPS(name)					\
	static int							\
	mempool_ops_probe_##name(struct rte_vdev_device *vdev)		\
	{								\
		RTE_SET_USED(vdev);					\
		rte_mempool_register_ops(&name);			\
		return 0;						\
	}								\
	static struct rte_vdev_driver drv_##name = {			\
		.probe = mempool_ops_probe_##name,			\
		.remove = NULL,						\
	};								\
	RTE_PMD_REGISTER_VDEV(mempool_##name, drv_##name);		\
	RTE_PMD_AUTOLOAD_VDEV(mempool_##name##0)

#endif /* _RTE_MEMPOOL_DRIVER_H_ */
