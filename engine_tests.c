#include "engine.c"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#define ASSERT_TILE(idx, expected)                                             \
    TEST_ASSERT_EQUAL_UINT8((expected), g_map.tiles[(idx)])

#define ASSERT_AGENT_POSITION(idx, expected)                                   \
    TEST_ASSERT_EQUAL_UINT32((expected), g_agents.positions[(idx)])

static uint32_t *g_world_state = NULL;
static struct World g_world;
static struct Agents g_agents;
static struct Map g_map;

[[nodiscard]] static uint32_t *create_world(void)
{
    const enum Tile o = TILE_FLOOR;
    const enum Tile x = TILE_FLOOR_OCCUPIED;

    // clang-format off
    enum Tile map[] = {
        x, x, o, o, o, o, o,
        o, o, o, o, o, o, o,
        o, o, o, o, o, o, o,
        o, o, o, o, o, o, o,
        o, o, o, o, o, o, o,
        o, o, o, o, o, o, o,
    };
    // clang-format on

    const uint32_t n_rows = 6U;
    const uint32_t n_cols = 7U;
    const uint32_t map_size = sizeof(map) / sizeof(map[0]);
    assert(map_size == n_rows * n_cols * sizeof(enum Tile));

    uint32_t *world = (uint32_t *)malloc(7 * sizeof(uint32_t) + map_size);
    world[0] = 2U;
    world[1] = 0U;
    world[2] = 1U;
    world[3] = ORIENTATION_UP;
    world[4] = ORIENTATION_UP;
    world[5] = n_rows;
    world[6] = n_cols;
    memcpy(world + 7, map, map_size);

    return world;
}

static void move_agent(const uint32_t agent_id, const uint32_t tile_id)
{
    uint32_t *pos = g_agents.positions + agent_id;
    const uint32_t old_pos = *pos;

    *pos = tile_id;

    g_map.tiles[*pos] = block_tile(g_map.tiles[*pos]);

    if (*pos != old_pos)
    {
        g_map.tiles[old_pos] = unblock_tile(g_map.tiles[old_pos]);
    }
}

static void move_agent0(const uint32_t tile_id)
{
    move_agent(0U, tile_id);
}

static void move_agent1(const uint32_t tile_id)
{
    move_agent(1U, tile_id);
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
    ASSERT_AGENT_POSITION(0, 0U);
    ASSERT_AGENT_POSITION(1, 1U);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_UP, g_agents.orientations[0]);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_UP, g_agents.orientations[1]);

    TEST_ASSERT_EQUAL_UINT32(6U, g_map.n_rows);
    TEST_ASSERT_EQUAL_UINT32(7U, g_map.n_cols);

    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
    ASSERT_TILE(1, TILE_FLOOR_OCCUPIED);
    ASSERT_TILE(41, TILE_FLOOR);
}

void test_try_move_moves_agent_up_into_free_tile(void)
{
    move_agent1(8);
    ASSERT_TILE(1, TILE_FLOOR);

    try_move(&g_world, ACTION_MOVE_UP, g_agents.positions + 1U);

    // agent 1 moved into the free tile
    ASSERT_AGENT_POSITION(1, 1U);
    ASSERT_TILE(1, TILE_FLOOR_OCCUPIED);
    ASSERT_TILE(8, TILE_FLOOR);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0, 0U);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_try_move_moves_agent_right_into_free_tile(void)
{
    move_agent1(1);
    ASSERT_TILE(2, TILE_FLOOR);

    try_move(&g_world, ACTION_MOVE_RIGHT, g_agents.positions + 1U);

    // agent 1 moved into the free tile
    ASSERT_AGENT_POSITION(1, 2U);
    ASSERT_TILE(2, TILE_FLOOR_OCCUPIED);
    ASSERT_TILE(1, TILE_FLOOR);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0U, 0);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_try_move_moves_agent_down_into_free_tile(void)
{
    move_agent1(1);
    ASSERT_TILE(8, TILE_FLOOR);

    try_move(&g_world, ACTION_MOVE_DOWN, g_agents.positions + 1U);

    // agent 1 moved into the free tile
    ASSERT_AGENT_POSITION(1, 8U);
    ASSERT_TILE(8, TILE_FLOOR_OCCUPIED);
    ASSERT_TILE(1, TILE_FLOOR);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0U, 0);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_try_move_moves_agent_left_into_free_tile(void)
{
    move_agent1(2);
    ASSERT_TILE(1, TILE_FLOOR);

    try_move(&g_world, ACTION_MOVE_LEFT, g_agents.positions + 1U);

    // agent 1 moved into the free tile
    ASSERT_AGENT_POSITION(1, 1U);
    ASSERT_TILE(1, TILE_FLOOR_OCCUPIED);
    ASSERT_TILE(2, TILE_FLOOR);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0U, 0);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_try_move_does_not_move_into_wall(void)
{
    move_agent1(1);
    g_map.tiles[2] = TILE_WALL;

    try_move(&g_world, ACTION_MOVE_RIGHT, g_agents.positions + 1U);

    // agent 1 must not move into wall
    ASSERT_AGENT_POSITION(1, 1U);
    ASSERT_TILE(1, TILE_FLOOR_OCCUPIED);
    ASSERT_TILE(2, TILE_WALL);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0U, 0);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_try_move_does_not_move_into_closed_door(void)
{
    move_agent1(1);
    g_map.tiles[2] = TILE_CLOSED_DOOR;

    try_move(&g_world, ACTION_MOVE_RIGHT, g_agents.positions + 1U);

    // agent 1 must not move into closed door
    ASSERT_AGENT_POSITION(1, 1U);
    ASSERT_TILE(1, TILE_FLOOR_OCCUPIED);
    ASSERT_TILE(2, TILE_CLOSED_DOOR);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0U, 0);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_try_move_does_not_move_into_occupied_tile(void)
{
    move_agent0(0);
    move_agent1(1);

    try_move(&g_world, ACTION_MOVE_LEFT, g_agents.positions + 1U);

    // agent 1 must not move into occupied tile (agent 0)
    ASSERT_AGENT_POSITION(1, 1U);
    ASSERT_TILE(1, TILE_FLOOR_OCCUPIED);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0U, 0);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_try_move_leaving_open_door_keeps_door_open(void)
{
    move_agent1(1);
    g_map.tiles[1] = TILE_OPEN_DOOR_OCCUPIED;

    try_move(&g_world, ACTION_MOVE_RIGHT, g_agents.positions + 1U);

    // agent 1 moved off the door to the right
    ASSERT_AGENT_POSITION(1, 2U);
    ASSERT_TILE(1, TILE_OPEN_DOOR);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0U, 0);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_try_move_into_open_door_marks_it_occupied(void)
{
    move_agent1(1);
    g_map.tiles[2] = TILE_OPEN_DOOR;

    try_move(&g_world, ACTION_MOVE_RIGHT, g_agents.positions + 1U);

    // agent 1 should have moved onto the door tile
    ASSERT_AGENT_POSITION(1, 2U);
    ASSERT_TILE(2, TILE_OPEN_DOOR_OCCUPIED);
    ASSERT_TILE(1, TILE_FLOOR);

    // agent 0 is unaffected
    ASSERT_AGENT_POSITION(0U, 0);
    ASSERT_TILE(0, TILE_FLOOR_OCCUPIED);
}

void test_turn_agent_by_90_degrees_clockwise(void)
{
    enum Orientation orientation = ORIENTATION_UP;

    turn(ACTION_TURN_90, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_RIGHT, orientation);

    turn(ACTION_TURN_90, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_DOWN, orientation);

    turn(ACTION_TURN_90, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_LEFT, orientation);

    turn(ACTION_TURN_90, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_UP, orientation);
}

void test_turn_agent_by_180_degrees_clockwise(void)
{
    enum Orientation orientation = ORIENTATION_UP;

    turn(ACTION_TURN_180, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_DOWN, orientation);

    turn(ACTION_TURN_180, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_UP, orientation);

    orientation = ORIENTATION_LEFT;

    turn(ACTION_TURN_180, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_RIGHT, orientation);

    turn(ACTION_TURN_180, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_LEFT, orientation);
}

void test_turn_agent_by_270_degrees_clockwise(void)
{
    enum Orientation orientation = ORIENTATION_UP;

    turn(ACTION_TURN_270, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_LEFT, orientation);

    turn(ACTION_TURN_270, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_DOWN, orientation);

    turn(ACTION_TURN_270, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_RIGHT, orientation);

    turn(ACTION_TURN_270, &orientation);
    TEST_ASSERT_EQUAL_UINT32(ORIENTATION_UP, orientation);
}

void test_fill_agent_fov(void)
{
    const enum Tile o = TILE_FLOOR;
    const enum Tile x = TILE_FLOOR_OCCUPIED;
    const enum Tile w = TILE_WALL;
    const enum Tile d = TILE_OPEN_DOOR;
    const enum Tile h = TILE_HIDDEN;

    // clang-format off
    enum Tile map[] = {
        x, o, o, o, o, w, o,
        o, x, o, o, o, w, o,
        o, o, o, o, o, w, o,
        w, d, w, w, w, w, o,
        o, o, w, o, o, o, o,
        o, o, w, o, o, o, o,
    };
    // clang-format on

    TEST_ASSERT_EQUAL_UINT32(6U, g_map.n_rows);
    TEST_ASSERT_EQUAL_UINT32(7U, g_map.n_cols);
    memcpy(g_map.tiles, map, sizeof(map));

    move_agent0(0);
    move_agent1(8);

    TEST_ASSERT_EQUAL_UINT32(FOV_SIZE, 5U);
    enum Tile tiles[25];

    {
        g_agents.orientations[1] = ORIENTATION_UP;
        fill_agent_fov(&g_world, 1U, tiles);

        // clang-format off
        enum Tile expected[] = {
            h, h, h, h, h,
            h, h, h, h, h,
            h, h, h, h, h,
            h, x, o, o, o,
            h, o, x, o, o,
        };
        // clang-format on

        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, tiles, 25);
    }

    {
        g_agents.orientations[1] = ORIENTATION_RIGHT;
        fill_agent_fov(&g_world, 1U, tiles);

        // clang-format off
        enum Tile expected[] = {
            h, w, w, w, w,
            h, o, o, o, w,
            h, o, o, o, w,
            h, o, o, o, w,
            h, o, x, o, d,
        };
        // clang-format on

        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, tiles, 25);
    }

    {
        g_agents.orientations[1] = ORIENTATION_DOWN;
        fill_agent_fov(&g_world, 1U, tiles);

        // clang-format off
        enum Tile expected[] = {
            o, w, o, o, h,
            o, w, o, o, h,
            w, w, d, w, h,
            o, o, o, o, h,
            o, o, x, o, h,
        };
        // clang-format on

        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, tiles, 25);
    }

    {
        g_agents.orientations[1] = ORIENTATION_LEFT;
        fill_agent_fov(&g_world, 1U, tiles);

        // clang-format off
        enum Tile expected[] = {
            h, h, h, h, h,
            h, h, h, h, h,
            h, h, h, h, h,
            w, o, o, x, h,
            d, o, x, o, h,
        };
        // clang-format on

        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, tiles, 25);
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_load_world_with_zero_seed);
    RUN_TEST(test_load_world_initializes_world);

    RUN_TEST(test_try_move_moves_agent_up_into_free_tile);
    RUN_TEST(test_try_move_moves_agent_right_into_free_tile);
    RUN_TEST(test_try_move_moves_agent_down_into_free_tile);
    RUN_TEST(test_try_move_moves_agent_left_into_free_tile);
    RUN_TEST(test_try_move_does_not_move_into_wall);
    RUN_TEST(test_try_move_does_not_move_into_closed_door);
    RUN_TEST(test_try_move_does_not_move_into_occupied_tile);
    RUN_TEST(test_try_move_leaving_open_door_keeps_door_open);
    RUN_TEST(test_try_move_into_open_door_marks_it_occupied);

    RUN_TEST(test_turn_agent_by_90_degrees_clockwise);
    RUN_TEST(test_turn_agent_by_180_degrees_clockwise);
    RUN_TEST(test_turn_agent_by_270_degrees_clockwise);

    RUN_TEST(test_fill_agent_fov);

    return UNITY_END();
}
