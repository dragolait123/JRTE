#pragma once

#include <cstdint>
#include <string>

#include "jrte_bit_reader.h"

namespace jrte::h264
{
    struct CoeffToken
    {
        uint8_t total_coeff = 0;
        uint8_t trailing_ones = 0;
    };

    // Decode H.264 coeff_token for nC >= 0.
    // Chroma DC contexts (nC = -1 and -2) are not implemented yet.
    bool decode_coeff_token(
        BitReader& bits,
        int nC,
        CoeffToken& result,
        std::string& error
    );
}
