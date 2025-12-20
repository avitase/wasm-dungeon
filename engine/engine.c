#include <stddef.h>
#include <stdint.h>

#ifndef unreachable
#ifdef __GNUC__
#define unreachable() (__builtin_unreachable())
#elifdef _MSC_VER
#define unreachable() (__assume(false))
#else
#define unreachable() ((void)0)
#endif
#endif

#define AGENT_STATE_VERSION 0x00010001U
#define AGENT_STATE_SIZE 11U
#define RNG_SEED 0x12345678U
#define FOV_SIZE 5U
#define FOV_SELF_IDX 22U

#ifdef __cplusplus
extern "C"
{
#endif

enum Action : uint32_t
{
    ACTION_NONE = 0,
    ACTION_TURN_90 = 1,
    ACTION_TURN_180 = 2,
    ACTION_TURN_270 = 3,
    ACTION_MOVE_UP = 4,
    ACTION_MOVE_RIGHT = 5,
    ACTION_MOVE_DOWN = 6,
    ACTION_MOVE_LEFT = 7,
    ACTION_OPEN_DOOR = 8,
    ACTION_CLOSE_DOOR = 9,
};

enum Tile : uint8_t
{
    TILE_HIDDEN = 0x00,
    TILE_WALL = 0x11,
    TILE_FLOOR = 0x02,
    TILE_FLOOR_OCCUPIED = 0x12,
    TILE_OPEN_DOOR = 0x03,
    TILE_OPEN_DOOR_OCCUPIED = 0x13,
    TILE_CLOSED_DOOR = 0x33,
};

enum Orientation : uint32_t
{
    ORIENTATION_UP,
    ORIENTATION_RIGHT,
    ORIENTATION_DOWN,
    ORIENTATION_LEFT,
};

struct Agents
{
    uint32_t n_agents;
    uint32_t *positions;
    enum Orientation *orientations;
};

struct Map
{
    uint32_t n_rows;
    uint32_t n_cols;
    enum Tile *tiles;
};

struct World
{
    uint32_t rng_state;
    struct Agents agents;
    struct Map map;
};

struct Pose
{
    uint32_t position;
    enum Orientation heading;
};

[[nodiscard]] static uint32_t is_tile_blocked(const enum Tile tile)
{
    // NOLINTNEXTLINE(readability-magic-numbers)
    return (tile & 0x10) == 0x10;
}

[[nodiscard]] static enum Tile block_tile(const enum Tile tile)
{
    // NOLINTNEXTLINE(readability-magic-numbers)
    return tile | 0x10;
}

[[nodiscard]] static enum Tile unblock_tile(const enum Tile tile)
{
    // NOLINTNEXTLINE(readability-magic-numbers)
    return tile & ~0x10;
}

static struct World load_world(uint32_t *world_state, const uint32_t seed)
{
    const uint32_t n_agents = world_state[0];

    const struct Agents agents = {
        .n_agents = n_agents,
        .positions = world_state + 1U,
        .orientations = (enum Orientation *)(world_state + 1U + n_agents)};

    const struct Map map = {
        .n_rows = world_state[1U + (2U * n_agents)],
        .n_cols = world_state[2U + (2U * n_agents)],
        .tiles = (enum Tile *)(world_state + 3U + (size_t)(2U * n_agents))};

    const struct World world = {
        .rng_state = seed ? seed : RNG_SEED, .agents = agents, .map = map};

    return world;
}

[[nodiscard]] static uint32_t ahead(const struct Map map,
                                    const struct Pose pose)
{
    const uint32_t n_rows = map.n_rows;
    const uint32_t n_cols = map.n_cols;

    switch (pose.heading)
    {
    case ORIENTATION_UP:
        if (pose.position >= n_cols)
        {
            return pose.position - n_cols;
        }
        break;
    case ORIENTATION_RIGHT:
        if ((pose.position % n_cols) + 1U < n_cols)
        {
            return pose.position + 1U;
        }
        break;
    case ORIENTATION_DOWN:
        if (pose.position + n_cols < n_rows * n_cols)
        {
            return pose.position + n_cols;
        }
        break;
    case ORIENTATION_LEFT:
        if ((pose.position % n_cols) > 0)
        {
            return pose.position - 1U;
        }
        break;
    default:
        unreachable();
    }

    return pose.position;
}

static void
try_move(const struct World *world, const enum Action action, uint32_t *pos)
{
    const struct Pose pose = {.position = *pos,
                              .heading =
                                  (enum Orientation)(action - ACTION_MOVE_UP)};

    const uint32_t old_pos = *pos;
    *pos = ahead(world->map, pose);

    enum Tile *tile = world->map.tiles + *pos;
    if (is_tile_blocked(*tile))
    {
        *pos = old_pos;
    }
    else
    {
        *tile = block_tile(*tile);

        enum Tile *old_tile = world->map.tiles + old_pos;
        *old_tile = unblock_tile(*old_tile);
    }
}

static void turn(const enum Action action, enum Orientation *orientation)
{
    *orientation = ORIENTATION_UP
        + ((*orientation - ORIENTATION_UP + (uint32_t)action) % 4U);
}

static void try_open_door(const struct Map map,
                          const uint32_t pos,
                          const enum Orientation orientation)
{
    const struct Pose pose = {.position = pos, .heading = orientation};

    enum Tile *tile = map.tiles + ahead(map, pose);
    if (*tile == TILE_CLOSED_DOOR)
    {
        *tile = TILE_OPEN_DOOR;
    }
}

static void try_close_door(const struct Map map,
                           const uint32_t pos,
                           const enum Orientation orientation)
{
    const struct Pose pose = {.position = pos, .heading = orientation};

    enum Tile *tile = map.tiles + ahead(map, pose);
    if (*tile == TILE_OPEN_DOOR)
    {
        *tile = TILE_CLOSED_DOOR;
    }
}

static void try_realize_action(const struct World *world,
                               const enum Action action,
                               const uint32_t idx)
{
    switch (action)
    {
    case ACTION_NONE:
        break; // do nothing
    case ACTION_MOVE_UP:
    case ACTION_MOVE_RIGHT:
    case ACTION_MOVE_DOWN:
    case ACTION_MOVE_LEFT:
        try_move(world, action, world->agents.positions + idx);
        break;
    case ACTION_TURN_90:
    case ACTION_TURN_180:
    case ACTION_TURN_270:
        turn(action, world->agents.orientations + idx);
        break;
    case ACTION_OPEN_DOOR:
        try_open_door(world->map,
                      world->agents.positions[idx],
                      world->agents.orientations[idx]);
        break;
    case ACTION_CLOSE_DOOR:
        try_close_door(world->map,
                       world->agents.positions[idx],
                       world->agents.orientations[idx]);
        break;
    }
}

static void
fill_agent_fov(const struct World *world, const uint32_t idx, enum Tile *tiles)
{
    static_assert(FOV_SIZE % 2 == 1);

    const uint32_t n_rows = world->map.n_rows;
    const uint32_t n_cols = world->map.n_cols;

    const uint32_t pos = world->agents.positions[idx];

    uint32_t a; // NOLINT(readability-identifier-length)
    uint32_t b; // NOLINT(readability-identifier-length)
    uint32_t c; // NOLINT(readability-identifier-length)
    uint32_t row_offset = pos / n_cols;
    uint32_t col_offset = pos % n_cols;
    switch (world->agents.orientations[idx])
    {
    case ORIENTATION_UP:
        a = FOV_SIZE;
        b = 1U;
        c = 0U;
        row_offset -= FOV_SIZE - 1U;
        col_offset -= (FOV_SIZE / 2U);
        break;
    case ORIENTATION_RIGHT:
        a = 1U;
        b = -FOV_SIZE;
        c = FOV_SIZE * (FOV_SIZE - 1U);
        row_offset -= (FOV_SIZE / 2U);
        break;
    case ORIENTATION_DOWN:
        a = -FOV_SIZE;
        b = -1U;
        c = (FOV_SIZE * FOV_SIZE) - 1U;
        col_offset -= (FOV_SIZE / 2U);
        break;
    case ORIENTATION_LEFT:
        a = -1U;
        b = FOV_SIZE;
        c = FOV_SIZE - 1U;
        row_offset -= (FOV_SIZE / 2U);
        col_offset -= FOV_SIZE - 1U;
        break;
    default:
        unreachable();
    }

    for (uint32_t i = 0; i < FOV_SIZE; i++)
    {
        for (uint32_t j = 0; j < FOV_SIZE; j++)
        {
            const uint32_t row = row_offset + i;
            const uint32_t col = col_offset + j;

            const uint32_t tile_idx = (a * i) + (b * j) + c;
            const uint32_t map_idx = (row * n_cols) + col;
            tiles[tile_idx] = (col < n_cols && row < n_rows)
                ? world->map.tiles[map_idx]
                : TILE_HIDDEN;
        }
    }
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
static void apply_occlusion(enum Tile *tiles)
{
    // NOLINTNEXTLINE(misc-redundant-expression,readability-magic-numbers)
    static_assert(FOV_SIZE == 5U);

    enum : uint32_t
    {
        n_tiles = FOV_SIZE * FOV_SIZE
    };

    uint32_t m[n_tiles]; // NOLINT(readability-identifier-length)
    for (uint32_t i = 0; i < n_tiles; i++)
    {
        m[i] = is_tile_blocked(tiles[i]);
    }

    /* Indexing of FoV:
     *
     *  0  1  2  3  4
     *  5  6  7  8  9
     * 10 11 12 13 14
     * 15 16 17 18 19
     * 20 21 xx 23 24
     *       ^^--- agent facing up
     *
     * Examples:
     *  - agent cannot see tile 1 if 12 *or* 17 are blocked.
     *  - agent cannot see tile 13 if 17 *and* 18 are blocked.
     */

    // NOLINTNEXTLINE(misc-redundant-expression, readability-magic-numbers)
    static_assert(FOV_SELF_IDX == 22U);

    // NOLINTBEGIN(readability-magic-numbers)
    m[0] = m[11] || m[17] || (m[6] && (m[5] || m[16]));
    m[1] = m[12] || m[17];
    m[2] = m[7] || m[12] || m[17];
    m[3] = m[12] || m[17];
    m[4] = m[13] || m[17] || (m[8] && (m[9] || m[18]));
    m[5] = m[11] || m[16] || m[17];
    m[6] = m[17] || (m[12] && (m[11] || m[16]));
    m[7] = m[12] || m[17];
    m[8] = m[17] || (m[12] && (m[13] || m[18]));
    m[9] = m[13] || m[17] || m[18];
    m[10] = m[16] || ((m[11] || m[17]) && (m[15] || m[21]));
    m[11] = m[16] && m[17];
    m[12] = m[17];
    m[14] = m[18] || ((m[13] || m[17]) && (m[19] || m[23]));
    m[13] = m[17] && m[18];
    m[15] = m[21];
    m[16] = m[17] && m[21];
    m[18] = m[17] && m[23];
    m[19] = m[23];
    m[20] = m[21];
    m[22] = 0U;
    m[24] = m[23];
    // NOLINTEND(readability-magic-numbers)

    for (uint32_t i = 0; i < n_tiles; i++)
    {
        tiles[i] = m[i] ? TILE_HIDDEN : tiles[i];
    }
}
// NOLINTEND(readability-function-cognitive-complexity)

static void update_agent_state(const struct World *world,
                               uint32_t *agent_state,
                               const uint32_t idx)
{
    agent_state[0] = AGENT_STATE_VERSION;
    agent_state[1] = FOV_SIZE; // rows
    agent_state[2] = FOV_SIZE; // columns
    agent_state[3] = FOV_SELF_IDX;

    enum Tile *tiles = (enum Tile *)(agent_state + 4U);
    fill_agent_fov(world, idx, tiles);
    apply_occlusion(tiles);
}

[[nodiscard]] static uint32_t rng(uint32_t *rng_state)
{
    enum : uint32_t
    {
        RNG_LCG_MUL = 1664525U,
        RNG_LCG_INC = 1013904223U
    };

    *rng_state = (*rng_state * RNG_LCG_MUL) + RNG_LCG_INC;
    return *rng_state;
}

[[nodiscard]] uint32_t agent_state_size(void)
{
    return AGENT_STATE_SIZE;
}

void tick(
    uint32_t *world_state,  // NOLINT(bugprone-easily-swappable-parameters)
    uint32_t *agent_states, // NOLINT(bugprone-easily-swappable-parameters)
    const uint32_t *agent_actions,
    const uint32_t seed)
{
    struct World world = load_world(world_state, seed);

    const uint32_t n_agents = world.agents.n_agents;
    if (n_agents == 0)
    {
        return;
    }

    uint32_t idx = rng(&world.rng_state) % n_agents;
    const uint32_t idx_increment =
        (rng(&world.rng_state) % 2U == 0) ? 1U : (n_agents - 1U);

    for (uint32_t i = 0; i < n_agents; i++)
    {
        idx = (idx + idx_increment) % n_agents;
        try_realize_action(&world, agent_actions[idx], idx);
    }

    for (uint32_t i = 0; i < n_agents; i++)
    {
        update_agent_state(
            &world, agent_states + (size_t)(i * AGENT_STATE_SIZE), i);
    }
}

#ifdef __cplusplus
}
#endif
