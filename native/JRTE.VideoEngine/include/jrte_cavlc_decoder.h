#pragma once

#include <array>
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

    bool decode_level_values(
        BitReader& bits,
        const CoeffToken& token,
        std::vector<int32_t>& levels,
        std::string& error
    );

    bool decode_total_zeros(
        BitReader& bits,
        uint8_t total_coeff,
        uint8_t& total_zeros,
        std::string& error
    );

    bool decode_chroma_dc_total_zeros(
        BitReader& bits,
        uint8_t total_coeff,
        uint8_t max_num_coeff,
        uint8_t& total_zeros,
        std::string& error
    );
    bool decode_run_before(
        BitReader& bits,
        uint8_t zeros_left,
        uint8_t& run_before,
        std::string& error
    );
    // Decode un bloc CAVLC 4x4 complet en ordre matriciel.
    bool decode_residual_block_4x4(
        BitReader& bits,
        int nC,
        std::array<int32_t, 16>& coefficients,
        std::string& error
    );
}

