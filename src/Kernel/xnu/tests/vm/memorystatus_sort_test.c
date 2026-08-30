#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/sysctl.h>

#include <darwintest.h>
#include <dispatch/dispatch.h>
#include <mach-o/dyld.h>

/* internal */
#include <spawn_private.h>
#include <sys/coalition.h>
#include <sys/kern_memorystatus.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vm"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM"));

#define NUM_PER_ROLE 3 /* Number of procs per role in coalition (besides leader) */
#define NUM_PROCS_IN_COALITION (NUM_PER_ROLE * (COALITION_NUM_TASKROLES - 1) + 1)
#define NUM_COALITIONS 3

#define COAL_ORDER_NUM_PIDS (NUM_PROCS_IN_COALITION + COALITION_NUM_TASKROLES - 1)
typedef struct {
	pid_t pids[NUM_PROCS_IN_COALITION]; // An array of pids in this coalition. Owned by this struct.
	pid_t expected_order[COAL_ORDER_NUM_PIDS]; // An array of pids in this coalition in proper sorted order.
	uint64_t ids[COALITION_NUM_TYPES];
	size_t leader_footprint;
} coalition_info_t;

/*
 * Children pids spawned by this test that need to be cleaned up.
 * Has to be a global because the T_ATEND API doesn't take any arguments.
 */
#define kMaxChildrenProcs NUM_PROCS_IN_COALITION * NUM_COALITIONS + 1
static pid_t children_pids[kMaxChildrenProcs];
static size_t num_children = 0;

/*
 * Sets up a new coalition.
 */
static void init_coalition(coalition_info_t*, size_t leader_fp);

/*
 * Places all procs in the coalition in the given band.
 */
static void place_coalition_in_band(const coalition_info_t *, int band);

/*
 * Place the given proc in the given band.
 */
static void place_proc_in_band(pid_t pid, int band);

/*
 * Cleans up any children processes.
 */
static void cleanup_children(void);

/*
 * Check if we're on a kernel where we can test coalitions.
 */
static bool has_unrestrict_coalitions(void);

/*
 * Unrestrict coalition syscalls.
 */
static void unrestrict_coalitions(void);

/*
 * Restrict coalition syscalls
 */
static void restrict_coalitions(void);

/*
 * Allocate the requested number of pages and fault them in.
 * Used to achieve a desired footprint.
 */
static void *allocate_pages(int);

/*
 * Get the vm page size.
 */
static int get_vmpage_size(void);

/*
 * Launch a proc with a role in a coalition.
 * If coalition_ids is NULL, skip adding the proc to the coalition.
 */
static pid_t
launch_proc_in_coalition(uint64_t *coalition_ids, int role, int num_pages);

static void
bufprint(char **buf, size_t *size, const char *fmt, ...)
{
	va_list list;
	int n_written;

	va_start(list, fmt);
	n_written = vsnprintf(*buf, *size, fmt, list);
	va_end(list);

	if (n_written > 0) {
		*buf += n_written;
		*size -= n_written;
	}
}

static char *
pids_str(pid_t *pids, int n_pids)
{
	int i;
	size_t buf_len = n_pids * 8 + 2; /* For good measure */
	char *buf = malloc(buf_len);
	char *obuf = buf;

	bufprint(&buf, &buf_len, "(");

	for (i = 0; (i < n_pids) && (buf_len > 0); i++) {
		if (pids[i] == -1) {
			bufprint(&buf, &buf_len, "), (");
		} else {
			bool is_last = (i == (n_pids - 1)) || (pids[i + 1] == -1);
			bufprint(&buf, &buf_len, "%d%s", pids[i], is_last ? "" : ", ");
		}
	}

	bufprint(&buf, &buf_len, ")");

	return obuf;
}

/*
 * Sorts the given jetsam band with the desired order and verifies that the
 * sort was done correctly.
 * `expected_order` is an array of groups of PIDs separated by `-1`, where PIDs
 * in each group are re-orderable. For instance, for the expected order:
 * [1, 2, -1, 3, -1, 4]
 * the orderings of
 * 1, 2, 3, 4 and 2, 1, 3, 4 are both valid since 1 and 2 are in the same group.
 */
static void
sort_and_verify(
	unsigned int prio,
	memorystatus_jetsam_sort_order_t order,
	pid_t *expected_order,
	size_t expected_order_len)
{
	size_t i, j, n_pids, group_idx;
	bool in_order;
	pid_t *actual_order;
	pid_t *original_expected_order;

	/* Bigger than we need it, but that's fine */
	actual_order = malloc(sizeof(pid_t) * expected_order_len);

	/* Make a copy of expected_order since we'll be overwriting it */
	original_expected_order = malloc(sizeof(pid_t) * expected_order_len);
	memcpy(original_expected_order, expected_order, sizeof(pid_t) * expected_order_len);

	/*
	 * Add only the actual pids from expected_order in to tell memorystatus which
	 * PIDs we care about
	 */
	n_pids = 0;
	for (i = 0; i < expected_order_len; i++) {
		if (expected_order[i] != -1) {
			actual_order[n_pids] = expected_order[i];
			n_pids++;
		}
	}

	int ret = memorystatus_control(MEMORYSTATUS_CMD_TEST_JETSAM_SORT, prio, order,
	    actual_order, n_pids * sizeof(pid_t));
	T_QUIET; T_EXPECT_POSIX_SUCCESS(ret, "Band sorted and order copied out");

	/* Check that the order we got was what we expected */
	group_idx = 0; /* idx of pid that starts current reorderable group */
	for (i = 0; i < n_pids; i++) {
		/*
		 * Check if the current pid in actual_order is in the current group.
		 * If not, advance to the next group until we find it. This is essentially
		 * a ratcheting mechanism - we can move our search group forwards, but not
		 * backwards.
		 */
		for (j = group_idx; j < expected_order_len; j++) {
			if (expected_order[j] == -1) {
				/* Made it to the end of a group w/o finding the pid */
				group_idx = j + 1;
				continue;
			} else if (expected_order[j] == actual_order[i]) {
				/* Found our pid. Mark it found */
				expected_order[j] = 0;
				break;
			}
		}
	}


	/* Check that all pids were actually found */
	in_order = true;
	for (i = 0; i < expected_order_len; i++) {
		if ((expected_order[i] != -1) && (expected_order[i] != 0)) {
			in_order = false;
			break;
		}
	}

	T_EXPECT_TRUE(in_order, "Band in correct order when sorted in order (%d)", order);

	if (!in_order) {
		char *exp_str = pids_str(original_expected_order, expected_order_len);
		char *actual_str = pids_str(actual_order, n_pids);
		T_LOG("Out of order! Expected:\n%s\nbut got\n%s\n", exp_str, actual_str);
		free(exp_str);
		free(actual_str);
	}

	free(actual_order);
	free(original_expected_order);
}

/*
 * Background process that will munch some memory, signal its parent, and
 * then sit in a loop.
 */
T_HELPER_DECL(coalition_member, "Mock coalition member") {
	int num_pages = 0;
	if (argc == 1) {
		num_pages = atoi(argv[0]);
	}
	allocate_pages(num_pages);
	if (num_pages) {
		printf("%d has %d\n", getpid(), num_pages);
	}
	// Signal to the parent that we've touched all of our pages.
	if (kill(getppid(), SIGUSR1) != 0) {
		T_LOG("Unable to signal to parent process!");
		exit(1);
	}
	while (true) {
		sleep(100);
	}
}

static void
random_order(int *arr, int size)
{
	int i, a, b, s;
	for (i = 0; i < size; i++) {
		arr[i] = i;
	}
	for (i = 0; i < size; i++) {
		a = rand() % size;
		b = rand() % size;
		s = arr[a];
		arr[a] = arr[b];
		arr[b] = s;
	}
}

static void
add_coalition_to_order(pid_t *order, coalition_info_t *coal, int coal_idx)
{
	int order_idx = coal_idx * (COAL_ORDER_NUM_PIDS + 1);
	memcpy(&order[order_idx], &coal->expected_order, sizeof(coal->expected_order));
	if (coal_idx != 0) {
		order[order_idx - 1] = -1;
	}
}

/*
 * Test that sorting the fg bucket in coalition order works properly.
 * Spawns children in the same coalition in the fg band. Each child
 * has a different coalition role. Verifies that the coalition
 * is sorted properly by role.
 */
#define COALS_EXPECTED_ORDER_LEN ((COAL_ORDER_NUM_PIDS * NUM_COALITIONS) + (NUM_COALITIONS - 1))
T_DECL(memorystatus_sort_coalitions_footprint, "Sort coalitions by leader footprint",
    T_META_ASROOT(true),
    T_META_TAG_VM_PREFERRED)
{
	int i;
	coalition_info_t *coalitions;
	int coalition_order[NUM_COALITIONS];
	pid_t *expected_order; /* Expected order of all pids in all coalitions */

	if (!has_unrestrict_coalitions()) {
		T_SKIP("Unable to test coalitions on this kernel.");
	}
	unrestrict_coalitions();

	T_ATEND(cleanup_children);
	T_ATEND(restrict_coalitions);

	/* Initialize our coalitions */
	coalitions = malloc(sizeof(coalition_info_t) * NUM_COALITIONS);
	expected_order = malloc(sizeof(pid_t) * COALS_EXPECTED_ORDER_LEN);

	/* Spawn the coalitions in random order */
	random_order(coalition_order, NUM_COALITIONS);

	/* Spawn coalitions, each with a different leader footprint */
	for (i = 0; i < NUM_COALITIONS; i++) {
		int coal = coalition_order[i];
		init_coalition(&coalitions[coal], (NUM_COALITIONS - coal) * 50);
		add_coalition_to_order(expected_order, &coalitions[coal], coal);
		place_coalition_in_band(&coalitions[coal], JETSAM_PRIORITY_FOREGROUND);
	}

	/* Sort by leader footprint and verify coalitions are sorted by leader footprint */
	sort_and_verify(JETSAM_PRIORITY_FOREGROUND, JETSAM_SORT_FOOTPRINT, expected_order, COALS_EXPECTED_ORDER_LEN);

	free(coalitions);
	free(expected_order);
}

T_DECL(memorystatus_sort_coalitions_lru, "Sort coalitions by leader LRU",
    T_META_ASROOT(true),
    T_META_TAG_VM_PREFERRED)
{
	int i;
	coalition_info_t *coalitions;
	int coalition_order[NUM_COALITIONS];
	pid_t *expected_order; /* Expected order of all pids in all coalitions */

	if (!has_unrestrict_coalitions()) {
		T_SKIP("Unable to test coalitions on this kernel.");
	}
	unrestrict_coalitions();

	T_ATEND(cleanup_children);
	T_ATEND(restrict_coalitions);

	/* Initialize our coalitions */
	coalitions = malloc(sizeof(coalition_info_t) * NUM_COALITIONS);
	expected_order = malloc(sizeof(pid_t) * COALS_EXPECTED_ORDER_LEN);

	/* Spawn coalitions */
	for (i = 0; i < NUM_COALITIONS; i++) {
		init_coalition(&coalitions[i], 0);
	}

	/* Add coalitions to foreground in random order*/
	random_order(coalition_order, NUM_COALITIONS);
	for (i = 0; i < NUM_COALITIONS; i++) {
		int coal = coalition_order[i];
		place_coalition_in_band(&coalitions[coal], JETSAM_PRIORITY_FOREGROUND);
		add_coalition_to_order(expected_order, &coalitions[coal], i);
	}


	/* Sort by leader LRU and verify coalitions are sorted by leader LRU */
	sort_and_verify(JETSAM_PRIORITY_FOREGROUND, JETSAM_SORT_LRU, expected_order, COALS_EXPECTED_ORDER_LEN);

	free(coalitions);
	free(expected_order);
}


/*
 * Test that sorting the idle bucket in footprint order works properly.
 *
 * Spawns some children with very different footprints in the idle band,
 * and then ensures that they get sorted properly.
 */
T_DECL(memorystatus_sort_footprint, "Footprint sort order",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED) {
#define kNumChildren 3
	static const int kChildrenFootprints[kNumChildren] = {500, 0, 2500};
	/*
	 * The expected sort order of the children in the order that they were launched.
	 * Used to construct the expected_order pid array.
	 * Note that procs should be sorted in descending footprint order.
	 */
	static const int kExpectedOrder[kNumChildren] = {2, 0, 1};
	static const int kJetsamBand = JETSAM_PRIORITY_BACKGROUND;
	__block pid_t pid;
	sig_t res;
	dispatch_source_t ds_allocated;
	T_ATEND(cleanup_children);

	// After we spawn the children, they'll signal that they've touched their pages.
	res = signal(SIGUSR1, SIG_IGN);
	T_WITH_ERRNO; T_ASSERT_NE(res, SIG_ERR, "SIG_IGN SIGUSR1");
	ds_allocated = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
	T_QUIET; T_ASSERT_NOTNULL(ds_allocated, "dispatch_source_create (ds_allocated)");

	dispatch_source_set_event_handler(ds_allocated, ^{
		if (num_children < kNumChildren) {
			pid = launch_proc_in_coalition(NULL, 0, kChildrenFootprints[num_children]);
			place_proc_in_band(pid, kJetsamBand);
		} else {
			pid_t expected_order[kNumChildren] = {0};
			for (int i = 0; i < kNumChildren; i++) {
				expected_order[i] = children_pids[kExpectedOrder[i]];
			}
			sort_and_verify(kJetsamBand, JETSAM_SORT_FOOTPRINT_NOCOAL, expected_order, kNumChildren);
			T_END;
		}
	});
	dispatch_activate(ds_allocated);

	pid = launch_proc_in_coalition(NULL, 0, kChildrenFootprints[num_children]);
	place_proc_in_band(pid, kJetsamBand);

	dispatch_main();

#undef kNumChildren
}

static pid_t
launch_proc_in_coalition(uint64_t *coalition_ids, int role, int num_pages)
{
	int ret;
	posix_spawnattr_t attr;
	pid_t pid;
	char testpath[PATH_MAX];
	uint32_t testpath_buf_size = PATH_MAX;
	char num_pages_str[32] = {0};
	char *argv[5] = {testpath, "-n", "coalition_member", num_pages_str, NULL};
	extern char **environ;
	T_QUIET; T_ASSERT_LT(num_children + 1, (size_t) kMaxChildrenProcs, "Don't create too many children.");
	ret = posix_spawnattr_init(&attr);
	T_QUIET; T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_init");
	if (coalition_ids != NULL) {
		for (int i = 0; i < COALITION_NUM_TYPES; i++) {
			ret = posix_spawnattr_setcoalition_np(&attr, coalition_ids[i], i, role);
			T_QUIET; T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_setcoalition_np");
		}
	}

	ret = snprintf(num_pages_str, sizeof(num_pages_str), "%d", num_pages);
	T_QUIET; T_ASSERT_LE((size_t) ret, sizeof(num_pages_str), "Don't allocate too many pages.");
	ret = _NSGetExecutablePath(testpath, &testpath_buf_size);
	T_QUIET; T_ASSERT_EQ(ret, 0, "_NSGetExecutablePath");
	ret = posix_spawn(&pid, argv[0], NULL, &attr, argv, environ);
	T_QUIET; T_ASSERT_POSIX_ZERO(ret, "posix_spawn");
	ret = posix_spawnattr_destroy(&attr);
	T_QUIET; T_ASSERT_POSIX_ZERO(ret, "posix_spawnattr_destroy");
	children_pids[num_children++] = pid;
	return pid;
}

static void
init_coalition(coalition_info_t *coalition, size_t leader_fp)
{
	/* This code will need updating if we add a role */
	static_assert(COALITION_NUM_TASKROLES == 4);

	sigset_t set;
	int ret, i, sig;
	uint32_t flags = 0;
	memset(coalition, 0, sizeof(coalition_info_t));
	for (int i = 0; i < COALITION_NUM_TYPES; i++) {
		COALITION_CREATE_FLAGS_SET_TYPE(flags, i);
		ret = coalition_create(&coalition->ids[i], flags);
		T_QUIET; T_ASSERT_POSIX_ZERO(ret, "coalition_create");
	}

	sigemptyset(&set);
	ret = sigaddset(&set, SIGUSR1);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sigaddset(SIGUSR1)");

	coalition->leader_footprint = leader_fp;

	/*
	 * Spawn procs for each coalition role, and construct the expected
	 * sorted order.
	 */
	int n_roles[COALITION_NUM_TASKROLES] = {0};
	int role_order_idx[COALITION_NUM_TASKROLES] = {
		/* COALITION_TASKROLE_UNDEF  */ 0,
		/* COALITION_TASKROLE_LEADER */ (NUM_PER_ROLE + 1) * 3,
		/* COALITION_TASKROLE_XPC    */ (NUM_PER_ROLE + 1) * 2,
		/* COALITION_TASKROLE_EXT    */ NUM_PER_ROLE + 1
	};
	for (i = 1; i < COALITION_NUM_TASKROLES; i++) {
		coalition->expected_order[role_order_idx[i] - 1] = -1;
	}
	for (size_t i = 0; i < NUM_PROCS_IN_COALITION; i++) {
		int role;
		size_t pages = 0;

		while (true) {
			role = rand() % COALITION_NUM_TASKROLES;
			if ((role == COALITION_TASKROLE_LEADER) && n_roles[role]) {
				continue; /* Already have a leader */
			} else if (n_roles[role] == NUM_PER_ROLE) {
				continue; /* Already have all of this role */
			}
			n_roles[role]++;
			break;
		}

		if (role == COALITION_TASKROLE_LEADER) {
			pages = leader_fp;
		}

		pid_t pid = launch_proc_in_coalition(coalition->ids, role, pages);
		ret = sigwait(&set, &sig);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sigwait");
		T_QUIET; T_ASSERT_EQ(sig, SIGUSR1, "sigwait == SIGUSR1");
		coalition->pids[i] = pid;
		coalition->expected_order[role_order_idx[role]] = pid;
		role_order_idx[role]++;
	}
}

static void
place_proc_in_band(pid_t pid, int band)
{
	memorystatus_priority_properties_t props = {0};
	int ret;
	props.priority = band;
	props.user_data = 0;
	ret = memorystatus_control(MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES, pid, 0, &props, sizeof(props));
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "move proc to band");
}


static void
place_coalition_in_band(const coalition_info_t *coalition, int band)
{
	for (size_t i = 0; i < NUM_PROCS_IN_COALITION; i++) {
		pid_t curr = coalition->pids[i];
		place_proc_in_band(curr, band);
	}
}

static void
cleanup_children(void)
{
	int ret, status;
	for (size_t i = 0; i < num_children; i++) {
		pid_t exited_pid = 0;
		pid_t curr = children_pids[i];
		ret = kill(curr, SIGKILL);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kill");
		while (exited_pid == 0) {
			exited_pid = waitpid(curr, &status, 0);
		}
		T_QUIET; T_ASSERT_POSIX_SUCCESS(exited_pid, "waitpid");
		T_QUIET; T_ASSERT_TRUE(WIFSIGNALED(status), "proc was signaled.");
		T_QUIET; T_ASSERT_EQ(WTERMSIG(status), SIGKILL, "proc was killed");
	}
}

static bool
has_unrestrict_coalitions()
{
	int ret, val;
	size_t val_sz;

	val = 0;
	val_sz = sizeof(val);
	ret = sysctlbyname("kern.unrestrict_coalitions", &val, &val_sz, NULL, 0);
	return ret >= 0;
}

static void
unrestrict_coalitions()
{
	int ret, val = 1;
	size_t val_sz;
	val_sz = sizeof(val);
	ret = sysctlbyname("kern.unrestrict_coalitions", NULL, 0, &val, val_sz);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kern.unrestrict_coalitions <- 1");
}

static void
restrict_coalitions()
{
	int ret, val = 0;
	size_t val_sz;
	val_sz = sizeof(val);
	ret = sysctlbyname("kern.unrestrict_coalitions", NULL, 0, &val, val_sz);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kern.unrestrict_coalitions <- 0");
}

static void *
allocate_pages(int num_pages)
{
	int page_size, i;
	unsigned char *buf;

	page_size = get_vmpage_size();
	buf = malloc((unsigned long)(num_pages * page_size));
	for (i = 0; i < num_pages; i++) {
		((volatile unsigned char *)buf)[i * page_size] = 1;
	}
	return buf;
}

static int
get_vmpage_size()
{
	int vmpage_size;
	size_t size = sizeof(vmpage_size);
	int ret = sysctlbyname("vm.pagesize", &vmpage_size, &size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "failed to query vm.pagesize");
	T_QUIET; T_ASSERT_GT(vmpage_size, 0, "vm.pagesize is not > 0");
	return vmpage_size;
}
