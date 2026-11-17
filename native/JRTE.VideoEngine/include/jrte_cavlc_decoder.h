#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "jrte_bit_reader.h"

namespace jrte::h264
{
    struct CoeffToken
    {
        uint8_t total_coeff = 0;
        uint8_t trailing_ones = 0;
    };

    bool decode_coeff_token(
        BitReader& bits,
        int nC,
        CoeffToken& result,
        std::string& error
    );

    bool decode_trailing_one_signs(
        BitReader& bits,
        uint8_t trailing_ones,
        std::vector<int8_t>& levels,
        std::string& error
    );
}
