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
#define RNG_SEED 0x12345678U

#ifdef __cplusplus
extern "C"
{
#endif

enum Action : uint32_t
{
    ACTION_NONE,
    ACTION_MOVE_UP,
    ACTION_MOVE_RIGHT,
    ACTION_MOVE_DOWN,
    ACTION_MOVE_LEFT,
    ACTION_TURN_90,
    ACTION_TURN_180,
    ACTION_TURN_270
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

static void
try_move(const struct World *world, const enum Action action, uint32_t *pos)
{
    const uint32_t old_pos = *pos;

    const uint32_t n_rows = world->map.n_rows;
    const uint32_t n_cols = world->map.n_cols;

    switch (action)
    {
    case ACTION_MOVE_UP:
        if (*pos >= n_cols)
        {
            *pos -= n_cols;
        }
        break;
    case ACTION_MOVE_RIGHT:
        if ((*pos % n_cols) + 1U < n_cols)
        {
            *pos += 1U;
        }
        break;
    case ACTION_MOVE_DOWN:
        if (*pos + n_cols < n_rows * n_cols)
        {
            *pos += n_cols;
        }
        break;
    case ACTION_MOVE_LEFT:
        if ((*pos % n_cols) > 0)
        {
            *pos -= 1U;
        }
        break;
    default:
        unreachable();
        return;
    }

    enum Tile *tile = world->map.tiles + *pos;
    if (is_tile_blocked(*tile))
    {
        *pos = old_pos;
        return;
    }

    *tile = block_tile(*tile);

    enum Tile *old_tile = world->map.tiles + old_pos;
    *old_tile = unblock_tile(*old_tile);
}

static void turn(const enum Action action, enum Orientation *orientation)
{
    switch (action)
    {
    case ACTION_TURN_90:
        *orientation += 1U;
        break;
    case ACTION_TURN_180:
        *orientation += 2U;
        break;
    case ACTION_TURN_270:
        *orientation += 3U;
        break;
    default:
        unreachable();
        return;
    }

    *orientation %= 4U;
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
    }
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
static void apply_occlusion(enum Tile *tiles)
{
    enum : uint32_t
    {
        n_tiles = 25U
    };

    uint32_t m[n_tiles]; // NOLINT(readability-identifier-length)
    for (uint32_t i = 0; i < n_tiles; i++)
    {
        m[i] = is_tile_blocked(tiles[i]);
    }

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
    m[11] = m[16] || m[17];
    m[12] = m[17];
    m[14] = m[18] || ((m[13] || m[17]) && (m[19] || m[23]));
    m[13] = m[17] || m[18];
    m[15] = m[21];
    m[16] = m[17] && m[21];
    m[18] = m[17] && m[23];
    m[19] = m[23];
    m[20] = m[21];
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
    const uint32_t size = 5U;

    agent_state[0] = AGENT_STATE_VERSION;
    agent_state[1] = size;
    agent_state[2] = size;

    const uint32_t map_offset = 3U;
    enum Tile *tiles = (enum Tile *)(agent_state + map_offset);

    const uint32_t n_rows = world->map.n_rows;
    const uint32_t n_cols = world->map.n_cols;

    const uint32_t pos = world->agents.positions[idx];
    const uint32_t row_offset = (pos / n_cols) - size + 1U;
    const uint32_t col_offset = (pos % n_cols) - size + 1U;

    uint32_t a; // NOLINT(readability-identifier-length)
    uint32_t b; // NOLINT(readability-identifier-length)
    uint32_t c; // NOLINT(readability-identifier-length)
    switch (world->agents.orientations[idx])
    {
    case ORIENTATION_UP:
        a = size;
        b = 1U;
        c = 0U;
        break;
    case ORIENTATION_RIGHT:
        a = -1U;
        b = size;
        c = size - 1U;
        break;
    case ORIENTATION_DOWN:
        a = -size;
        b = -1U;
        c = size * size - 1U;
        break;
    case ORIENTATION_LEFT:
        a = 1U;
        b = -size;
        c = size * (size - 1U);
        break;
    default:
        unreachable();
        return;
    }

    for (uint32_t i = 0; i < size; i++)
    {
        for (uint32_t j = 0; j < size; j++)
        {
            const uint32_t row = row_offset + i;
            const uint32_t col = col_offset + j;

            const uint32_t tile_idx = (a * i) + (b * j) + c;
            tiles[tile_idx] = (col < n_cols && row < n_rows)
                ? world->map.tiles[(row * n_cols) + col]
                : TILE_HIDDEN;
        }
    }

    apply_occlusion(tiles);
}

[[nodiscard]] static uint32_t rng(uint32_t *rng_state)
{
    enum : uint32_t
    {
        RNG_LCG_MUL = 1664525U,
        RNG_LCG_INC = 1013904223U
    };

    *rng_state = *rng_state * RNG_LCG_MUL + RNG_LCG_INC;
    return *rng_state;
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
        update_agent_state(&world, agent_states + i, i);
    }
}

#ifdef __cplusplus
}
#endif
