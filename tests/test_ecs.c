#include "ecs.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void assert_id(uint32_t index, uint32_t gen, EntityId actual)
{
    TEST_ASSERT_EQUAL_UINT32(index, actual.index);
    TEST_ASSERT_EQUAL_UINT32(gen, actual.gen);
}

static void assert_rejected(EcsWorld *w, EntityId id)
{
    const uint32_t count = ecs_alive_count(w);
    TEST_ASSERT_FALSE(ecs_is_alive(w, id));
    TEST_ASSERT_FALSE(ecs_add_mask(w, id, 1));
    TEST_ASSERT_FALSE(ecs_remove_mask(w, id, 2));
    TEST_ASSERT_FALSE(ecs_has_mask(w, id, 0));
    ecs_destroy(w, id);
    TEST_ASSERT_EQUAL_UINT32(count, ecs_alive_count(w));
}

static void test_initial_state_and_invalid_ids(void)
{
    EcsWorld w;
    ecs_init(&w);
    TEST_ASSERT_EQUAL_UINT32(0, ecs_alive_count(&w));
    TEST_ASSERT_EQUAL_UINT32(0, w.high_water);
    TEST_ASSERT_EQUAL_UINT32(512, w.free_count);
    assert_id(ECS_INVALID_INDEX, 0, ecs_entity_at(&w, 0));
    assert_id(ECS_INVALID_INDEX, 0, ecs_entity_at(&w, UINT32_MAX));
    assert_rejected(&w, (EntityId){0, 0});
    assert_rejected(&w, (EntityId){ECS_INVALID_INDEX, 0});

    assert_id(0, 1, ecs_spawn(&w));
    assert_rejected(&w, (EntityId){0, 0});
    assert_id(0, 1, ecs_entity_at(&w, 0));
}

static void test_null_world(void)
{
    ecs_init(NULL);
    assert_id(ECS_INVALID_INDEX, 0, ecs_spawn(NULL));
    assert_rejected(NULL, (EntityId){0, 1});
    ecs_flush_destroy(NULL);
    TEST_ASSERT_EQUAL_UINT32(0, ecs_alive_count(NULL));
    assert_id(ECS_INVALID_INDEX, 0, ecs_entity_at(NULL, 0));
}

static void test_reused_slot_has_new_generation_and_empty_mask(void)
{
    EcsWorld w;
    ecs_init(&w);
    const EntityId old = ecs_spawn(&w);
    assert_id(0, 1, old);
    TEST_ASSERT_TRUE(ecs_add_mask(&w, old, UINT64_MAX));
    ecs_destroy(&w, old);
    ecs_flush_destroy(&w);

    const EntityId next = ecs_spawn(&w);
    assert_id(0, 2, next);
    assert_id(0, 2, ecs_entity_at(&w, 0));
    TEST_ASSERT_EQUAL_HEX64(0, w.comp_mask[0]);
    TEST_ASSERT_TRUE(ecs_add_mask(&w, next, 2));
    assert_rejected(&w, old);
    TEST_ASSERT_TRUE(ecs_is_alive(&w, next));
    TEST_ASSERT_EQUAL_HEX64(2, w.comp_mask[0]);
}

static void test_generation_skips_zero_on_wrap(void)
{
    EcsWorld w;
    ecs_init(&w);
    const EntityId id = ecs_spawn(&w);
    w.gen[id.index] = UINT32_MAX;
    const EntityId last = ecs_entity_at(&w, id.index);
    assert_id(id.index, UINT32_MAX, last);
    ecs_destroy(&w, last);
    ecs_flush_destroy(&w);
    assert_id(id.index, 1, ecs_spawn(&w));
}

static void test_full_world_waits_for_flush_to_spawn(void)
{
    EcsWorld w;
    ecs_init(&w);
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; ++i) {
        assert_id(i, 1, ecs_spawn(&w));
    }
    assert_id(ECS_INVALID_INDEX, 0, ecs_spawn(&w));
    TEST_ASSERT_EQUAL_UINT32(512, ecs_alive_count(&w));
    TEST_ASSERT_EQUAL_UINT32(512, w.high_water);
    TEST_ASSERT_EQUAL_UINT32(0, w.free_count);

    const EntityId removed = ecs_entity_at(&w, 123);
    ecs_destroy(&w, removed);
    assert_id(ECS_INVALID_INDEX, 0, ecs_spawn(&w));
    TEST_ASSERT_EQUAL_UINT32(511, ecs_alive_count(&w));
    TEST_ASSERT_EQUAL_UINT32(0, w.free_count);
    ecs_flush_destroy(&w);
    TEST_ASSERT_EQUAL_UINT32(1, w.free_count);
    assert_id(123, 2, ecs_spawn(&w));
    TEST_ASSERT_EQUAL_UINT32(512, ecs_alive_count(&w));
    TEST_ASSERT_EQUAL_UINT32(0, w.free_count);
}

static void test_pending_destroy_hides_slot_immediately(void)
{
    EcsWorld w;
    ecs_init(&w);
    const EntityId id = ecs_spawn(&w);
    TEST_ASSERT_TRUE(ecs_add_mask(&w, id, 2));
    ecs_destroy(&w, id);
    TEST_ASSERT_EQUAL_UINT32(0, ecs_alive_count(&w));
    assert_id(ECS_INVALID_INDEX, 0, ecs_entity_at(&w, id.index));
    assert_rejected(&w, id);
    TEST_ASSERT_EQUAL_UINT32(511, w.free_count);
    TEST_ASSERT_EQUAL_HEX64(2, w.comp_mask[id.index]);
    ecs_flush_destroy(&w);
    TEST_ASSERT_EQUAL_UINT32(512, w.free_count);
    TEST_ASSERT_EQUAL_HEX64(0, w.comp_mask[id.index]);
}

static void test_mask_operations_require_all_requested_bits(void)
{
    EcsWorld w;
    ecs_init(&w);
    const EntityId id = ecs_spawn(&w);
    const uint64_t high = UINT64_C(1) << 63;
    TEST_ASSERT_TRUE(ecs_has_mask(&w, id, 0));
    TEST_ASSERT_TRUE(ecs_add_mask(&w, id, 3));
    TEST_ASSERT_TRUE(ecs_add_mask(&w, id, high));
    TEST_ASSERT_TRUE(ecs_add_mask(&w, id, 0));
    TEST_ASSERT_TRUE(ecs_remove_mask(&w, id, 0));
    TEST_ASSERT_TRUE(ecs_has_mask(&w, id, high | 3));
    TEST_ASSERT_FALSE(ecs_has_mask(&w, id, 7));
    TEST_ASSERT_TRUE(ecs_remove_mask(&w, id, 1));
    TEST_ASSERT_FALSE(ecs_has_mask(&w, id, 3));
    TEST_ASSERT_TRUE(ecs_has_mask(&w, id, high | 2));
    TEST_ASSERT_TRUE(ecs_remove_mask(&w, id, high));
    TEST_ASSERT_FALSE(ecs_has_mask(&w, id, high));
}

static void test_iteration_and_reuse_order(void)
{
    EcsWorld w;
    ecs_init(&w);
    for (uint32_t i = 0; i < 10; ++i) {
        assert_id(i, 1, ecs_spawn(&w));
    }
    ecs_destroy(&w, ecs_entity_at(&w, 3));
    ecs_destroy(&w, ecs_entity_at(&w, 7));
    ecs_flush_destroy(&w);

    const uint32_t expected[] = {0, 1, 2, 4, 5, 6, 8, 9};
    uint32_t visited = 0;
    for (uint32_t i = 0; i < w.high_water; ++i) {
        const EntityId id = ecs_entity_at(&w, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        TEST_ASSERT_TRUE(visited < sizeof expected / sizeof expected[0]);
        assert_id(expected[visited], 1, id);
        ++visited;
    }
    TEST_ASSERT_EQUAL_UINT32(8, visited);
    assert_id(3, 2, ecs_spawn(&w));
    assert_id(7, 2, ecs_spawn(&w));
    assert_id(10, 1, ecs_spawn(&w));
    TEST_ASSERT_EQUAL_UINT32(11, w.high_water);

    ecs_destroy(&w, ecs_entity_at(&w, 9));
    ecs_destroy(&w, ecs_entity_at(&w, 10));
    ecs_flush_destroy(&w);
    TEST_ASSERT_EQUAL_UINT32(9, w.high_water);
    for (uint32_t i = 0; i < w.high_water; ++i) {
        ecs_destroy(&w, ecs_entity_at(&w, i));
    }
    ecs_flush_destroy(&w);
    TEST_ASSERT_EQUAL_UINT32(0, w.high_water);
    TEST_ASSERT_EQUAL_UINT32(0, ecs_alive_count(&w));
    TEST_ASSERT_EQUAL_UINT32(512, w.free_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_and_invalid_ids);
    RUN_TEST(test_null_world);
    RUN_TEST(test_reused_slot_has_new_generation_and_empty_mask);
    RUN_TEST(test_generation_skips_zero_on_wrap);
    RUN_TEST(test_full_world_waits_for_flush_to_spawn);
    RUN_TEST(test_pending_destroy_hides_slot_immediately);
    RUN_TEST(test_mask_operations_require_all_requested_bits);
    RUN_TEST(test_iteration_and_reuse_order);
    return UNITY_END();
}
