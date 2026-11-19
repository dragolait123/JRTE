#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "jrte_bit_reader.h"

namespace jrte::h264
{
    struct NeighborCoeffInfo
    {
        bool left_available = false;
        bool top_available = false;

        uint8_t left_total_coeff = 0;
        uint8_t top_total_coeff = 0;
    };

    bool derive_nc(
        const NeighborCoeffInfo& neighbors,
        int& nc,
        std::string& error
    );

    bool decode_macroblock_residual_block(
        BitReader& reader,
        const NeighborCoeffInfo& neighbors,
        std::array<int32_t, 16>& coefficients,
        uint8_t& total_coeff,
        std::string& error
    );
}
