#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace jrte::cabac_context_tables {

struct InitPair {
    int16_t m = 0;
    int16_t n = 0;
};

inline constexpr size_t CONTEXT_COUNT = 460;
inline constexpr size_t MODEL_COUNT = 4;

// Mod?les :
// 0 = tranches I et SI
// 1 = P, SP et B avec cabac_init_idc = 0
// 2 = P, SP et B avec cabac_init_idc = 1
// 3 = P, SP et B avec cabac_init_idc = 2

using ContextInitTable =
    std::array<std::array<InitPair, MODEL_COUNT>, CONTEXT_COUNT>;

} // namespace jrte::cabac_context_tables
