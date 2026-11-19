#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "jrte_macroblock_decoder.h"

namespace jrte::h264
{
    class BlockCoeffContext
    {
    public:
        bool reset(
            uint32_t width,
            uint32_t height,
            std::string& error
        );

        bool record_block(
            uint32_t x,
            uint32_t y,
            uint8_t total_coeff,
            std::string& error
        );

        bool get_neighbor_info(
            uint32_t x,
            uint32_t y,
            NeighborCoeffInfo& neighbors,
            std::string& error
        ) const;

        bool decode_block(
            uint32_t x,
            uint32_t y,
            BitReader& reader,
            std::array<int32_t, 16>& coefficients,
            std::string& error
        );
        uint32_t width() const;
        uint32_t height() const;

    private:
        uint32_t width_ = 0;
        uint32_t height_ = 0;

        std::vector<uint8_t> total_coeff_;
        std::vector<uint8_t> decoded_;

        size_t index(uint32_t x, uint32_t y) const;
    };
}


