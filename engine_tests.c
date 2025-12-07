#include "engine.c"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

static uint32_t *g_world_state = NULL;
static struct World g_world;
static struct Agents g_agents;
static struct Map g_map;

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

void setUp(void)
{
    g_world_state = create_world();
    TEST_ASSERT_NOT_NULL(g_world_state);

    g_world = load_world(g_world_state, 42U);
    g_agents = g_world.agents;
    g_map = g_world.map;
}

void tearDown(void)
{
    free(g_world_state);
    g_world_state = NULL;
}

void test_load_world_with_zero_seed(void)
{
    const uint32_t seed = 0U;
    const struct World world = load_world(g_world_state, seed);

    TEST_ASSERT_NOT_EQUAL(0U, world.rng_state);
}

void test_load_world_initializes_world(void)
{
    TEST_ASSERT_NOT_EQUAL(0U, g_world.rng_state);

    TEST_ASSERT_EQUAL_UINT32(2U, g_agents.n_agents);
    TEST_ASSERT_EQUAL_UINT32(28U, g_agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT32(32U, g_agents.positions[1]);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_RIGHT, g_agents.orientations[0]);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_UP, g_agents.orientations[1]);

    TEST_ASSERT_EQUAL_UINT32(5U, g_map.n_rows);
    TEST_ASSERT_EQUAL_UINT32(12U, g_map.n_cols);

    TEST_ASSERT_EQUAL_UINT8(TILE_WALL, g_map.tiles[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_WALL, g_map.tiles[59]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[28]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[32]);
}

void test_try_move_moves_agent_into_free_tile(void)
{
    /* Move agent 1 into a free tile and ensure agent 0 is unaffected. */
    TEST_ASSERT_EQUAL_UINT32(2U, g_agents.n_agents);

    /* Place agent 0 at 28 and agent 1 at 29, with 30 free. */
    g_agents.positions[0] = 28U;
    g_agents.positions[1] = 29U;
    g_map.tiles[28] = TILE_OCCUPIED;
    g_map.tiles[29] = TILE_OCCUPIED;
    g_map.tiles[30] = TILE_FREE;

    try_move(&g_world, ACTION_MOVE_RIGHT, g_agents.positions + 1U);

    /* Agent 1 moved into the free tile. */
    TEST_ASSERT_EQUAL_UINT32(30U, g_agents.positions[1]);
    TEST_ASSERT_EQUAL_UINT8(TILE_FREE, g_map.tiles[29]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[30]);

    /* Agent 0 is unaffected. */
    TEST_ASSERT_EQUAL_UINT32(28U, g_agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[28]);
}

void test_try_move_does_not_move_into_wall(void)
{
    /* Place agent 0 on its default tile and agent 1 below a wall. */
    g_agents.positions[0] = 28U;
    g_agents.positions[1] = 13U;
    g_map.tiles[28] = TILE_OCCUPIED;
    g_map.tiles[13] = TILE_OCCUPIED;

    TEST_ASSERT_EQUAL_UINT8(TILE_WALL, g_map.tiles[1]);

    try_move(&g_world, ACTION_MOVE_UP, g_agents.positions + 1U);

    /* Agent 1 must not move into the wall. */
    TEST_ASSERT_EQUAL_UINT32(13U, g_agents.positions[1]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[13]);
    TEST_ASSERT_EQUAL_UINT8(TILE_WALL, g_map.tiles[1]);

    /* Agent 0 is unaffected. */
    TEST_ASSERT_EQUAL_UINT32(28U, g_agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[28]);
}

void test_try_move_leaving_open_door_keeps_door_open(void)
{
    /* Agent 0 stays on its tile; agent 1 starts on an open door at 30. */
    g_agents.positions[0] = 28U;
    g_agents.positions[1] = 30U;

    g_map.tiles[28] = TILE_OCCUPIED;
    g_map.tiles[32] = TILE_FREE; /* clear old agent-1 tile */
    g_map.tiles[30] = TILE_OPEN_DOOR_OCCUPIED;

    TEST_ASSERT_EQUAL_UINT8(TILE_FREE, g_map.tiles[31]);

    try_move(&g_world, ACTION_MOVE_RIGHT, g_agents.positions + 1U);

    /* Agent 1 moved off the door to the right. */
    TEST_ASSERT_EQUAL_UINT32(31U, g_agents.positions[1]);
    /* Old door stays open but becomes unoccupied. */
    TEST_ASSERT_EQUAL_UINT8(TILE_OPEN_DOOR_FREE, g_map.tiles[30]);
    /* New tile becomes normally occupied. */
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[31]);

    /* Agent 0 is unaffected. */
    TEST_ASSERT_EQUAL_UINT32(28U, g_agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[28]);
}

void test_try_move_into_open_door_marks_it_occupied(void)
{
    /* Agent 0 stays on its tile; agent 1 moves into an open door. */
    g_agents.positions[0] = 28U;
    g_agents.positions[1] = 29U;

    g_map.tiles[28] = TILE_OCCUPIED;
    g_map.tiles[32] = TILE_FREE; /* clear old agent-1 tile */
    g_map.tiles[29] = TILE_OCCUPIED;
    g_map.tiles[30] = TILE_OPEN_DOOR_FREE;

    try_move(&g_world, ACTION_MOVE_RIGHT, g_agents.positions + 1U);

    /* Agent 1 should have moved onto the door tile. */
    TEST_ASSERT_EQUAL_UINT32(30U, g_agents.positions[1]);
    TEST_ASSERT_EQUAL_UINT8(TILE_FREE, g_map.tiles[29]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OPEN_DOOR_OCCUPIED, g_map.tiles[30]);

    /* Agent 0 is unaffected. */
    TEST_ASSERT_EQUAL_UINT32(28U, g_agents.positions[0]);
    TEST_ASSERT_EQUAL_UINT8(TILE_OCCUPIED, g_map.tiles[28]);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_load_world_with_zero_seed);
    RUN_TEST(test_load_world_initializes_world);

    RUN_TEST(test_try_move_moves_agent_into_free_tile);
    RUN_TEST(test_try_move_does_not_move_into_wall);
    RUN_TEST(test_try_move_leaving_open_door_keeps_door_open);
    RUN_TEST(test_try_move_into_open_door_marks_it_occupied);

    return UNITY_END();
}
