#include "rng.h"

namespace
{

std::uint32_t hash(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

struct DirectionTable {
    std::uint32_t values[32];
};

std::uint32_t sobol(std::uint32_t index, const std::uint32_t* directions)
{
    std::uint32_t value = 0;
    for (unsigned bit = 0; index; ++bit, index >>= 1) {
        if (index & 1)
            value ^= directions[bit];
    }
    return value;
}

void directions(std::uint32_t*  values,
                unsigned        degree,
                unsigned        polynomial,
                const unsigned* m)
{
    for (unsigned bit = 0; bit < degree; ++bit)
        values[bit] = m[bit] << (31 - bit);
    for (unsigned bit = degree; bit < 32; ++bit) {
        std::uint32_t value = values[bit - degree] ^ (values[bit - degree] >> degree);
        for (unsigned coefficient = 1; coefficient < degree; ++coefficient) {
            if ((polynomial >> (degree - 1 - coefficient)) & 1)
                value ^= values[bit - coefficient];
        }
        values[bit] = value;
    }
}

} // namespace

Rng::Rng(unsigned seed, int x, int y, int sample, int depth)
{
    std::uint32_t state = seed;
    state ^= hash(static_cast<std::uint32_t>(x));
    state ^= hash(static_cast<std::uint32_t>(y) + 0x9e3779b9u);
    state ^= hash(static_cast<std::uint32_t>(depth) + 0xc2b2ae35u);
    scramble_ = hash(state);
    index_    = static_cast<std::uint32_t>(sample);
}

Imath::V3f Rng::next()
{
    static const std::uint32_t direction_0[32] = {
        0x80000000u, 0x40000000u, 0x20000000u, 0x10000000u, 0x08000000u, 0x04000000u,
        0x02000000u, 0x01000000u, 0x00800000u, 0x00400000u, 0x00200000u, 0x00100000u,
        0x00080000u, 0x00040000u, 0x00020000u, 0x00010000u, 0x00008000u, 0x00004000u,
        0x00002000u, 0x00001000u, 0x00000800u, 0x00000400u, 0x00000200u, 0x00000100u,
        0x00000080u, 0x00000040u, 0x00000020u, 0x00000010u, 0x00000008u, 0x00000004u,
        0x00000002u, 0x00000001u
    };
    static const DirectionTable direction_1 = [] {
        DirectionTable     result{};
        constexpr unsigned m[] = { 1, 3 };
        directions(result.values, 2, 1, m);
        return result;
    }();
    static const DirectionTable direction_2 = [] {
        DirectionTable     result{};
        constexpr unsigned m[] = { 1, 3, 1 };
        directions(result.values, 3, 1, m);
        return result;
    }();
    constexpr float     scale  = 1.0f / 4294967296.0f;
    const std::uint32_t sample = index_++;
    return { (sobol(sample, direction_0) ^ scramble_) * scale,
             (sobol(sample, direction_1.values) ^ scramble_) * scale,
             (sobol(sample, direction_2.values) ^ scramble_) * scale };
}
