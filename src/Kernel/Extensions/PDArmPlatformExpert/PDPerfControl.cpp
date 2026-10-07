// edge scheduler policy for an E and a P cluster (the A733's A55s and A76s). without Apple's performance
// controller every thread group prefers the boot cluster and the edges stay closed, so the P cores sit idle

#include <IOKit/IOLib.h>
#include <kern/kern_types.h>
#include <arm/machine_routines.h>
#include "PDPerfControl.h"

static uint32_t sPCluster, sECluster;

// interactive and user-initiated work (realtime and above-UI with it) prefers the P cluster, default,
// utility and background work the E cluster. the edges still spill either way under load
static void
pd_perfcontrol_tg_init(thread_group_data_t data)
{
	uint32_t overrides[PERFCONTROL_CLASS_MAX];

	for (int i = 0; i < PERFCONTROL_CLASS_MAX; i++) {
		overrides[i] = SCHED_PERFCONTROL_PREFERRED_CLUSTER_OVERRIDE_NONE;
	}
	overrides[PERFCONTROL_CLASS_REALTIME] = sPCluster;
	overrides[PERFCONTROL_CLASS_UI] = sPCluster;
	overrides[PERFCONTROL_CLASS_ABOVEUI] = sPCluster;
	overrides[PERFCONTROL_CLASS_USER_INITIATED] = sPCluster;
	sched_perfcontrol_thread_group_preferred_clusters_set(data->thread_group_data, sECluster, overrides, 0);
}

static void
pd_perfcontrol_tg_deinit(thread_group_data_t)
{
}

void
pd_perfcontrol_start(void)
{
	const ml_topology_info_t *topo = ml_get_topology_info();
	bool haveP = false, haveE = false;

	if (topo == NULL || topo->num_clusters != 2)
		return;
	for (unsigned i = 0; i < topo->num_clusters; i++) {
		const ml_topology_cluster_t *c = &topo->clusters[i];

		if (c->cluster_type == CLUSTER_TYPE_P) {
			sPCluster = c->cluster_id;
			haveP = true;
		} else if (c->cluster_type == CLUSTER_TYPE_E) {
			sECluster = c->cluster_id;
			haveE = true;
		}
	}
	if (!haveP || !haveE)
		return;

	// work moves and is stolen both ways: up to P whenever P is lighter, down to E only past a margin.
	// the matrix is by pset id: xnu makes one pset per cluster in topology order, so pset ids are cluster ids
	sched_clutch_edge edges[2 * 2];
	bool changed[2 * 2];

	bzero(edges, sizeof(edges));
	bzero(changed, sizeof(changed));
	for (uint32_t src = 0; src < 2; src++) {
		for (uint32_t dst = 0; dst < 2; dst++) {
			if (src == dst)
				continue;
			sched_clutch_edge *e = &edges[src * 2 + dst];

			e->sce_migration_allowed = 1;
			e->sce_steal_allowed = 1;
			e->sce_migration_weight = dst == sPCluster ? 0 : 2;
			changed[src * 2 + dst] = true;
		}
	}
	sched_perfcontrol_edge_matrix_set(edges, changed, 0, 2);

	static struct sched_perfcontrol_callbacks callbacks;

	callbacks.version = SCHED_PERFCONTROL_CALLBACKS_VERSION_3;
	callbacks.thread_group_init = pd_perfcontrol_tg_init;
	callbacks.thread_group_deinit = pd_perfcontrol_tg_deinit;
	// registering replays thread_group_init for every group that already exists
	sched_perfcontrol_register_callbacks(&callbacks, 0);
	IOLog("PDPerfControl: E cluster %u, P cluster %u: interactive and initiated work prefers P, the rest E\n",
	    sECluster, sPCluster);
}
