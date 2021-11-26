//
//  malloc_zone_unregister_test.c
//  libmalloc
//
//  Tests for malloc_zone_unregister().
//

#include <darwintest.h>

#include <malloc_private.h>
#include <malloc/malloc.h>
#include <stdlib.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

extern int32_t malloc_num_zones;
extern malloc_zone_t **malloc_zones;

T_DECL(malloc_zone_unregister_establish_custom_default_zone,
		"Unregister all initial zones and register a custom default zone",
		T_META_ENVVAR("MallocNanoZone=1"))
{
  void *ptr = malloc(7);
  T_EXPECT_NOTNULL(malloc_zone_from_ptr(ptr), "can find zone for allocation");
  T_EXPECT_TRUE(malloc_claimed_address(ptr), "ptr is claimed");

  T_ASSERT_LE(malloc_num_zones, 10, "at most 10 initial zones");
  malloc_zone_t *initial_zones[10];
  uint32_t initial_zone_count = malloc_num_zones;

  // Unregister initial zones
  for (uint32_t i = 0; i < initial_zone_count; i++) {
    initial_zones[i] = malloc_zones[0];
    malloc_zone_unregister(malloc_zones[0]);
  }
  T_EXPECT_EQ(malloc_num_zones, 0, "unregistered initial zones");

  // No zones, no results, no crash
  T_EXPECT_NULL(malloc_zone_from_ptr(ptr), "cannot find zone");
  T_EXPECT_FALSE(malloc_claimed_address(ptr), "ptr not claimed");

  // Create and register custom default zone
  malloc_zone_t *custom_zone = malloc_create_zone(0, 0);

  // Custom default zone only, no results, no crash
  T_EXPECT_NULL(malloc_zone_from_ptr(ptr), "cannot find zone");
  T_EXPECT_FALSE(malloc_claimed_address(ptr), "ptr not claimed");

  // Re-register initial zones
  for (uint32_t i = 0; i < initial_zone_count; i++) {
    malloc_zone_register(initial_zones[i]);
  }
  T_EXPECT_EQ(malloc_num_zones, initial_zone_count + 1, "re-registered initial zones");

  // Custom default zone plus initial zones
  T_EXPECT_NOTNULL(malloc_zone_from_ptr(ptr), "can find zone for allocation");
  T_EXPECT_TRUE(malloc_claimed_address(ptr), "ptr is claimed");

  // Check that the custom zone is the default zone
  void *ptr2 = malloc(7);
  T_EXPECT_EQ(malloc_zone_from_ptr(ptr2), custom_zone, "can find custom zone for allocation");
  T_EXPECT_TRUE(malloc_claimed_address(ptr2), "ptr from custom zone is claimed");

  free(ptr2);
  free(ptr);
}
