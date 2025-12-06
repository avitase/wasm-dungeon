#include "engine.c"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

[[nodiscard]] static uint32_t *create_world(void)
{
    // clang-format off
    enum Tile map[] = {
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 3,
        3, 1, 1, 1, 2, 1, 5, 1, 2, 1, 1, 3,
        3, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 3,
        3, 3, 3, 3, 7, 3, 3, 3, 3, 3, 3, 3
    };
    // clang-format on
    const uint32_t n_rows = 5U;
    const uint32_t n_cols = 12U;
    const uint32_t map_size = sizeof(map) / sizeof(map[0]);
    assert(map_size == n_rows * n_cols * sizeof(enum Tile));

    uint32_t *world = (uint32_t *)malloc(7 * sizeof(uint32_t) + map_size);
    world[0] = 2U;
    world[1] = 28U;
    world[2] = 32U;
    world[3] = ORIENTATION_RIGHT;
    world[4] = ORIENTATION_UP;
    world[5] = n_rows;
    world[6] = n_cols;
    memcpy(world + 7, map, map_size);

    return world;
}

void test_load_world(void)
{
    uint32_t *world_state = create_world();
    TEST_ASSERT_NOT_NULL(world_state);

    {
        const uint32_t seed = 0U;
        const struct World world = load_world(world_state, seed);
        TEST_ASSERT_TRUE(world.rng_state > 0U);
    }

    {
        const uint32_t seed = 42U;
        const struct World world = load_world(world_state, seed);

        TEST_ASSERT_EQUAL_UINT32(seed, world.rng_state);

        const struct Agents agents = world.agents;
        TEST_ASSERT_EQUAL_UINT32(28U, agents.positions[0]);
        TEST_ASSERT_EQUAL_UINT32(32U, agents.positions[1]);
        TEST_ASSERT_EQUAL_UINT32(ORIENTATION_RIGHT, agents.orientations[0]);
        TEST_ASSERT_EQUAL_UINT32(ORIENTATION_UP, agents.orientations[1]);

        const struct Map map = world.map;
        TEST_ASSERT_EQUAL_UINT32(5, map.n_rows);
        TEST_ASSERT_EQUAL_UINT32(12, map.n_cols);
        TEST_ASSERT_EQUAL_UINT8(TILE_WALL, map.tiles[0]);
        TEST_ASSERT_EQUAL_UINT8(TILE_WALL, map.tiles[59]);
        TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, map.tiles[28]);
        TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, map.tiles[32]);
    }

    free(world_state);
}

void test_try_move_moves_agent_into_free_tile(void)
{
    uint32_t *world_state = create_world();
    TEST_ASSERT_NOT_NULL(world_state);

    struct World world = load_world(world_state, 42U);
    struct Map map = world.map;

    TEST_ASSERT_EQUAL_UINT32(2U, world.agents.n_agents);
    TEST_ASSERT_EQUAL_UINT32(28U, world.agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, map.tiles[28]);
    TEST_ASSERT_EQUAL_UINT8(TILE_FREE, map.tiles[29]);

    try_move(&world, ACTION_MOVE_RIGHT, 0U);

    TEST_ASSERT_EQUAL_UINT32(29U, world.agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_FREE, map.tiles[28]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, map.tiles[29]);

    TEST_ASSERT_EQUAL_UINT32(32U, world.agents.positions[1]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, map.tiles[32]);

    free(world_state);
}

void test_try_move_does_not_move_into_wall(void)
{
    uint32_t *world_state = create_world();
    TEST_ASSERT_NOT_NULL(world_state);

    struct World world = load_world(world_state, 42U);
    struct Map map = world.map;

    world.agents.positions[0] = 13U;
    map.tiles[28] = TILE_FREE;
    map.tiles[13] = TILE_OCCUPIED;

    TEST_ASSERT_EQUAL_UINT8(TILE_WALL, map.tiles[1]);

    try_move(&world, ACTION_MOVE_UP, 0U);

    TEST_ASSERT_EQUAL_UINT32(13U, world.agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, map.tiles[13]);
    TEST_ASSERT_EQUAL_UINT8(TILE_WALL, map.tiles[1]);

    free(world_state);
}

void test_try_move_leaving_open_door_keeps_door_open(void)
{
    uint32_t *world_state = create_world();
    TEST_ASSERT_NOT_NULL(world_state);

    struct World world = load_world(world_state, 42U);
    struct Map map = world.map;

    world.agents.positions[0] = 30U;
    map.tiles[28] = TILE_FREE;
    map.tiles[30] = TILE_OPEN_DOOR_OCCUPIED;

    TEST_ASSERT_EQUAL_UINT8(TILE_FREE, map.tiles[31]);

    try_move(&world, ACTION_MOVE_RIGHT, 0U);

    TEST_ASSERT_EQUAL_UINT32(31U, world.agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OPEN_DOOR_FREE, map.tiles[30]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, map.tiles[31]);

    free(world_state);
}

void test_try_move_into_open_door_marks_it_occupied(void)
{
    uint32_t *world_state = create_world();
    TEST_ASSERT_NOT_NULL(world_state);

    struct World world = load_world(world_state, 42U);
    struct Map map = world.map;

    world.agents.positions[0] = 29U;
    map.tiles[28] = TILE_FREE;
    map.tiles[29] = TILE_OCCUPIED;
    map.tiles[30] = TILE_OPEN_DOOR_FREE;

    try_move(&world, ACTION_MOVE_RIGHT, 0U);

    TEST_ASSERT_EQUAL_UINT32(30U, world.agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_FREE, map.tiles[29]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OPEN_DOOR_OCCUPIED, map.tiles[30]);

    free(world_state);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_load_world);
    RUN_TEST(test_try_move_moves_agent_into_free_tile);
    RUN_TEST(test_try_move_does_not_move_into_wall);
    RUN_TEST(test_try_move_leaving_open_door_keeps_door_open);
    RUN_TEST(test_try_move_into_open_door_marks_it_occupied);

    return UNITY_END();
}
