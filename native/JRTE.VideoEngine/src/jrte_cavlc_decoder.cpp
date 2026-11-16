#include "jrte_cavlc_decoder.h"

#include <cstddef>
#include <cstdint>

namespace jrte::h264
{
    namespace
    {
        constexpr uint8_t kCoeffTokenLengths[3][68] =
        {
            {
                1,0,0,0, 6,2,0,0, 8,6,3,0, 9,8,7,5,
                10,9,8,6, 11,10,9,7, 13,11,10,8, 13,13,11,9,
                13,13,13,10, 14,14,13,11, 14,14,14,13,
                15,15,14,14, 15,15,15,14, 16,15,15,15,
                16,16,16,15, 16,16,16,16, 16,16,16,16
            },
            {
                2,0,0,0, 6,2,0,0, 6,5,3,0, 7,6,6,4,
                8,6,6,4, 8,7,7,5, 9,8,8,6, 11,9,9,6,
                11,11,11,7, 12,11,11,9, 12,12,12,11,
                12,12,12,11, 13,13,13,12, 13,13,13,13,
                13,14,13,13, 14,14,14,13, 14,14,14,14
            },
            {
                4,0,0,0, 6,4,0,0, 6,5,4,0, 6,5,5,4,
                7,5,5,4, 7,5,5,4, 7,6,6,4, 7,6,6,4,
                8,7,7,5, 8,8,7,6, 9,8,8,7, 9,9,8,8,
                9,9,9,8, 10,9,9,9, 10,10,10,10,
                10,10,10,10, 10,10,10,10
            }
        };

        constexpr uint8_t kCoeffTokenCodes[3][68] =
        {
            {
                1,0,0,0, 5,1,0,0, 7,4,1,0, 7,6,5,3,
                7,6,5,3, 7,6,5,4, 15,6,5,4, 11,14,5,4,
                8,10,13,4, 15,14,9,4, 11,10,13,12,
                15,14,9,12, 11,10,13,8, 15,1,9,12,
                11,14,13,8, 7,10,9,12, 4,6,5,8
            },
            {
                3,0,0,0, 11,2,0,0, 7,7,3,0, 7,10,9,5,
                7,6,5,4, 4,6,5,6, 7,6,5,8, 15,6,5,4,
                11,14,13,4, 15,10,9,4, 11,14,13,12,
                8,10,9,8, 15,14,13,12, 11,10,9,12,
                7,11,6,8, 9,8,10,1, 7,6,5,4
            },
            {
                15,0,0,0, 15,14,0,0, 11,15,13,0, 8,12,14,12,
                15,10,11,11, 11,8,9,10, 9,14,13,9, 8,10,9,8,
                15,14,13,13, 11,14,10,12, 15,10,13,12,
                11,14,9,12, 8,10,13,8, 13,7,9,12,
                9,12,11,10, 5,8,7,6, 1,4,3,2
            }
        };

        bool decode_fixed_coeff_token(
            BitReader& bits,
            CoeffToken& result,
            std::string& error)
        {
            uint32_t code = 0;

            if (!bits.read_bits(6, code))
            {
                error = "Flux tronque pendant le coeff_token fixe.";
                return false;
            }

            if (code == 3)
            {
                result.total_coeff = 0;
                result.trailing_ones = 0;
                return true;
            }

            const uint32_t total_coeff = (code >> 2) + 1;
            const uint32_t trailing_ones = code & 3;

            if (total_coeff > 16 || trailing_ones > 3 ||
                trailing_ones > total_coeff)
            {
                error = "Code coeff_token fixe invalide.";
                return false;
            }

            result.total_coeff = static_cast<uint8_t>(total_coeff);
            result.trailing_ones = static_cast<uint8_t>(trailing_ones);
            return true;
        }
    }

    bool decode_coeff_token(
        BitReader& bits,
        int nC,
        CoeffToken& result,
        std::string& error)
    {
        result = {};
        error.clear();

        if (nC < 0)
        {
            error = "Les contextes chroma DC nC=-1/-2 ne sont pas encore implementes.";
            return false;
        }

        if (nC > 16)
        {
            error = "Valeur nC hors limites.";
            return false;
        }

        if (!bits.good())
        {
            error = "BitReader invalide : " + bits.error();
            return false;
        }

        if (nC >= 8)
            return decode_fixed_coeff_token(bits, result, error);

        const unsigned int table =
            nC < 2 ? 0u : (nC < 4 ? 1u : 2u);

        uint32_t code = 0;

        for (uint8_t length = 1; length <= 16; ++length)
        {
            uint32_t bit = 0;

            if (!bits.read_bit(bit))
            {
                error = "Flux tronque pendant le coeff_token : " +
                    bits.error();
                return false;
            }

            code = (code << 1) | bit;

            for (uint8_t total = 0; total <= 16; ++total)
            {
                for (uint8_t trailing = 0; trailing <= 3; ++trailing)
                {
                    const size_t index =
                        static_cast<size_t>(total) * 4 + trailing;

                    if (kCoeffTokenLengths[table][index] != length)
                        continue;

                    if (kCoeffTokenCodes[table][index] != code)
                        continue;

                    if (trailing > total)
                        continue;

                    result.total_coeff = total;
                    result.trailing_ones = trailing;
                    return true;
                }
            }
        }

        error = "Code coeff_token inconnu ou invalide.";
        return false;
    }
}
