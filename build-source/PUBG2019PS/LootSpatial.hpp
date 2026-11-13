#pragma once
#include <cmath>
#include <cstdint>
namespace LootSpatial {
constexpr float CellSize = 10000.f;
inline std::int32_t Cell(float coordinate) { return static_cast<std::int32_t>(std::floor(coordinate / CellSize)); }
inline std::uint64_t Key(std::int32_t x, std::int32_t y) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(y);
}
}
