#include "jrte_cavlc_decoder.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

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

namespace jrte::h264
{
    bool decode_trailing_one_signs(
        BitReader& bits,
        uint8_t trailing_ones,
        std::vector<int8_t>& levels,
        std::string& error)
    {
        levels.clear();
        error.clear();

        if (trailing_ones > 3)
        {
            error = "Nombre de trailing ones invalide.";
            return false;
        }

        if (!bits.good())
        {
            error = "BitReader invalide : " + bits.error();
            return false;
        }

        for (uint8_t i = 0; i < trailing_ones; ++i)
        {
            uint32_t sign = 0;

            if (!bits.read_bit(sign))
            {
                levels.clear();
                error = "Flux tronque pendant les signes trailing ones : "
                    + bits.error();
                return false;
            }

            levels.push_back(sign ? -1 : 1);
        }

        return true;
    }
}

namespace jrte::h264
{
    bool decode_level_values(
        BitReader& bits,
        const CoeffToken& token,
        std::vector<int32_t>& levels,
        std::string& error)
    {
        levels.clear();
        error.clear();

        if (token.total_coeff > 16 ||
            token.trailing_ones > 3 ||
            token.trailing_ones > token.total_coeff)
        {
            error = "CoeffToken invalide.";
            return false;
        }

        if (!bits.good())
        {
            error = "BitReader invalide : " + bits.error();
            return false;
        }

        std::vector<int32_t> decoded;
        decoded.reserve(token.total_coeff);

        // Decoder les trailing ones et leurs signes.
        for (uint8_t i = 0; i < token.trailing_ones; ++i)
        {
            uint32_t sign = 0;

            if (!bits.read_bit(sign))
            {
                error = "Flux tronque pendant trailing_ones_sign_flag : "
                    + bits.error();
                return false;
            }

            decoded.push_back(sign ? -1 : 1);
        }

        // Initialiser suffixLength selon H.264, section 9.2.2.
        uint32_t suffix_length =
            (token.total_coeff > 10 && token.trailing_ones < 3)
                ? 1u
                : 0u;

        // Decoder les niveaux restants.
        for (uint8_t i = token.trailing_ones;
             i < token.total_coeff;
             ++i)
        {
            uint32_t level_prefix = 0;
            uint32_t bit = 0;

            // level_prefix : compter les zeros jusqu'au premier 1.
            while (true)
            {
                if (!bits.read_bit(bit))
                {
                    error = "Flux tronque pendant level_prefix : "
                        + bits.error();
                    return false;
                }

                if (bit == 1)
                    break;

                ++level_prefix;

                // Limite de prefix valide pour ce decodeur.
                if (level_prefix > 28)
                {
                    error = "level_prefix invalide ou trop grand.";
                    return false;
                }
            }

            // Calcul de levelSuffixSize.
            uint32_t suffix_size = suffix_length;

            if (level_prefix == 14 && suffix_length == 0)
            {
                suffix_size = 4;
            }
            else if (level_prefix >= 15)
            {
                suffix_size = level_prefix - 3;
            }

            if (suffix_size > 25)
            {
                error = "level_suffix depasse la limite autorisee.";
                return false;
            }

            uint32_t level_suffix = 0;

            if (suffix_size > 0 &&
                !bits.read_bits(suffix_size, level_suffix))
            {
                error = "Flux tronque pendant level_suffix : "
                    + bits.error();
                return false;
            }

            // levelCode = (Min(15, level_prefix) << suffixLength)
            //             + level_suffix.
            const uint64_t prefix_value =
                level_prefix < 15 ? level_prefix : 15;

            uint64_t level_code =
                (prefix_value << suffix_length) + level_suffix;

            if (level_prefix >= 15 && suffix_length == 0)
            {
                level_code += 15;
            }

            if (level_prefix >= 16)
            {
                level_code +=
                    (uint64_t(1) << (level_prefix - 3)) - 4096;
            }

            // Correction du premier niveau non-trailing.
            if (i == token.trailing_ones &&
                token.trailing_ones < 3)
            {
                level_code += 2;
            }

            // Conversion levelCode vers levelVal signe.
            int64_t signed_level = 0;

            if ((level_code & 1u) == 0)
            {
                signed_level =
                    static_cast<int64_t>((level_code + 2) / 2);
            }
            else
            {
                signed_level =
                    -static_cast<int64_t>((level_code + 1) / 2);
            }

            if (signed_level < std::numeric_limits<int32_t>::min() ||
                signed_level > std::numeric_limits<int32_t>::max())
            {
                error = "Niveau CAVLC hors limites int32.";
                return false;
            }

            decoded.push_back(static_cast<int32_t>(signed_level));

            // Mise a jour de suffixLength.
            if (suffix_length == 0)
            {
                suffix_length = 1;
            }

            const uint64_t threshold =
                uint64_t(3) << (suffix_length - 1);

            const uint64_t magnitude =
                signed_level < 0
                    ? static_cast<uint64_t>(-signed_level)
                    : static_cast<uint64_t>(signed_level);

            if (magnitude > threshold && suffix_length < 6)
            {
                ++suffix_length;
            }
        }

        levels = std::move(decoded);
        return true;
    }
}

namespace jrte::h264
{
    bool decode_total_zeros(
        BitReader& bits,
        uint8_t total_coeff,
        uint8_t& total_zeros,
        std::string& error)
    {
        total_zeros = 0;
        error.clear();

        if (total_coeff == 0 || total_coeff > 16)
        {
            error = "TotalCoeff invalide pour total_zeros.";
            return false;
        }

        if (!bits.good())
        {
            error = "BitReader invalide : " + bits.error();
            return false;
        }

        // Lorsque les 16 coefficients sont non nuls,
        // total_zeros n'est pas present dans le flux.
        if (total_coeff == 16)
            return true;

        // Tables H.264 9-7 et 9-8.
        // Index de colonne : total_zeros.
        // Index de ligne : TotalCoeff - 1.
        static const char* const codes[15][16] =
        {
            {
                "1", "011", "010", "0011",
                "0010", "00011", "00010", "000011",
                "000010", "0000011", "0000010",
                "00000011", "00000010", "000000011",
                "000000010", "000000001"
            },
            {
                "111", "110", "101", "100",
                "011", "0101", "0100", "0011",
                "0010", "00011", "00010", "000011",
                "000010", "000001", "000000"
            },
            {
                "0101", "111", "110", "101",
                "0100", "0011", "100", "011",
                "0010", "00011", "00010", "000001",
                "00001", "000000"
            },
            {
                "00011", "111", "0101", "0100",
                "110", "101", "100", "0011",
                "011", "0010", "00010", "00001",
                "00000"
            },
            {
                "0101", "0100", "0011", "111",
                "110", "101", "100", "011",
                "0010", "00001", "0001", "00000"
            },
            {
                "000001", "00001", "111", "110",
                "101", "100", "011", "010",
                "0001", "001", "000000"
            },
            {
                "000001", "00001", "101", "100",
                "011", "11", "010", "0001",
                "001", "000000"
            },
            {
                "000001", "0001", "00001", "011",
                "11", "10", "010", "001", "000000"
            },
            {
                "000001", "000000", "0001", "11",
                "10", "001", "01", "00001"
            },
            {
                "00001", "00000", "001", "11",
                "10", "01", "0001"
            },
            {
                "0000", "0001", "001", "010",
                "1", "011"
            },
            {
                "0000", "0001", "01", "1", "001"
            },
            {
                "000", "001", "1", "01"
            },
            {
                "00", "01", "1"
            },
            {
                "0", "1"
            }
        };

        const uint32_t table_index =
            static_cast<uint32_t>(total_coeff - 1);

        std::string candidate;
        candidate.reserve(9);

        // Les mots de code de ces tables ont au maximum 9 bits.
        for (uint32_t length = 1; length <= 9; ++length)
        {
            uint32_t bit = 0;

            if (!bits.read_bit(bit))
            {
                error = "Flux tronque pendant total_zeros : "
                    + bits.error();
                return false;
            }

            candidate.push_back(bit ? '1' : '0');

            for (uint32_t value = 0; value < 16; ++value)
            {
                const char* expected = codes[table_index][value];

                if (expected == nullptr)
                    continue;

                if (candidate == expected)
                {
                    // Validation de la plage normative.
                    if (value > 16 - total_coeff)
                    {
                        error = "total_zeros depasse la plage autorisee.";
                        return false;
                    }

                    total_zeros = static_cast<uint8_t>(value);
                    return true;
                }
            }
        }

        error = "Code total_zeros inconnu ou invalide.";
        return false;
    }
}

namespace jrte::h264
{
    bool decode_run_before(
        BitReader& bits,
        uint8_t zeros_left,
        uint8_t& run_before,
        std::string& error)
    {
        run_before = 0;
        error.clear();

        if (zeros_left == 0 || zeros_left > 15)
        {
            error = "zerosLeft invalide pour run_before.";
            return false;
        }

        if (!bits.good())
        {
            error = bits.error();
            return false;
        }

        // Table 9-10 H.264.
        // Colonnes : zerosLeft = 1, 2, 3, 4, 5, 6, >6.
        // Lignes : run_before = 0 a 14.
        static const char* codes[7][15] =
        {
            {
                "1", "0"
            },
            {
                "1", "01", "00"
            },
            {
                "11", "10", "01", "00"
            },
            {
                "11", "10", "01", "001", "000"
            },
            {
                "11", "10", "011", "010", "001", "000"
            },
            {
                "11", "000", "001", "011", "010", "101", "100"
            },
            {
                "111", "110", "101", "100", "011",
                "010", "001", "0001", "00001", "000001",
                "0000001", "00000001", "000000001",
                "0000000001", "00000000001"
            }
        };

        const size_t column =
            zeros_left <= 6 ? zeros_left - 1 : 6;

        std::string candidate;
        candidate.reserve(11);

        for (size_t length = 1; length <= 11; ++length)
        {
            uint32_t bit = 0;

            if (!bits.read_bit(bit))
            {
                error = "Flux tronque pendant run_before : ";
                error += bits.error();
                return false;
            }

            candidate.push_back(bit ? '1' : '0');

            const uint8_t maximum_run =
                zeros_left < 14 ? zeros_left : 14;

            for (uint8_t value = 0; value <= maximum_run; ++value)
            {
                const char* expected = codes[column][value];

                if (expected != nullptr && candidate == expected)
                {
                    run_before = value;
                    return true;
                }
            }
        }

        error = "Code run_before inconnu ou invalide.";
        return false;
    }
}



namespace jrte::h264
{
    bool decode_residual_block_4x4(
        BitReader& bits,
        int nC,
        std::array<int32_t, 16>& coefficients,
        std::string& error)
    {
        coefficients.fill(0);
        error.clear();

        // Ordre zigzag 4x4 defini par H.264.
        static constexpr uint8_t scan[16] =
        {
            0, 1, 4, 8,
            5, 2, 3, 6,
            9, 12, 13, 10,
            7, 11, 14, 15
        };

        std::array<int32_t, 16> decoded_block{};

        // 1. Decoder coeff_token.
        CoeffToken token;

        if (!decode_coeff_token(bits, nC, token, error))
            return false;

        // Bloc vide : aucun autre element syntaxique.
        if (token.total_coeff == 0)
        {
            coefficients = decoded_block;
            return true;
        }

        if (token.total_coeff > 16)
        {
            error = "TotalCoeff depasse 16.";
            return false;
        }

        // 2. Decoder les niveaux.
        std::vector<int32_t> levels;

        if (!decode_level_values(bits, token, levels, error))
            return false;

        if (levels.size() != token.total_coeff)
        {
            error = "Nombre de niveaux decode incorrect.";
            return false;
        }

        // 3. Decoder total_zeros.
        uint8_t total_zeros = 0;

        if (token.total_coeff < 16)
        {
            if (!decode_total_zeros(
                bits,
                token.total_coeff,
                total_zeros,
                error))
            {
                return false;
            }
        }

        if (total_zeros > 16 - token.total_coeff)
        {
            error = "total_zeros hors limites pour le bloc 4x4.";
            return false;
        }

        // 4. Reconstruire les run_before.
        std::array<uint8_t, 16> runs{};
        uint8_t zeros_left = total_zeros;

        for (uint8_t i = 0; i + 1 < token.total_coeff; ++i)
        {
            if (zeros_left == 0)
            {
                runs[i] = 0;
                continue;
            }

            if (!decode_run_before(
                bits,
                zeros_left,
                runs[i],
                error))
            {
                return false;
            }

            if (runs[i] > zeros_left)
            {
                error = "run_before depasse zerosLeft.";
                return false;
            }

            zeros_left = static_cast<uint8_t>(
                zeros_left - runs[i]
            );
        }

        // Le dernier run contient les zeros restants.
        runs[token.total_coeff - 1] = zeros_left;

        // 5. Placer les coefficients dans le balayage zigzag.
        int coefficient_number = -1;

        for (int i = static_cast<int>(token.total_coeff) - 1;
             i >= 0;
             --i)
        {
            coefficient_number +=
                static_cast<int>(runs[i]) + 1;

            if (coefficient_number < 0 ||
                coefficient_number >= 16)
            {
                error = "Position de coefficient hors bloc 4x4.";
                return false;
            }

            decoded_block[scan[coefficient_number]] = levels[i];
        }

        coefficients = decoded_block;
        return true;
    }
}
