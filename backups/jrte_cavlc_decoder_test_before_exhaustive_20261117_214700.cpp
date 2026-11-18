#include "jrte_cavlc_decoder.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace jrte::h264;

namespace
{
    std::vector<uint8_t> make_bits(const std::string& text)
    {
        std::vector<uint8_t> bytes((text.size() + 7) / 8, 0);

        for (size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '1')
                bytes[i / 8] |=
                    static_cast<uint8_t>(1u << (7 - (i % 8)));
        }

        return bytes;
    }

    bool check(
        const std::string& name,
        const std::string& encoded,
        int nC,
        uint8_t expected_total,
        uint8_t expected_trailing)
    {
        const auto bytes = make_bits(encoded);
        BitReader reader(bytes.data(), bytes.size());

        CoeffToken token;
        std::string error;

        if (!decode_coeff_token(reader, nC, token, error))
        {
            std::cerr << "[FAIL] " << name << ": " << error << '\n';
            return false;
        }

        if (token.total_coeff != expected_total ||
            token.trailing_ones != expected_trailing)
        {
            std::cerr << "[FAIL] " << name
                      << ": obtenu TotalCoeff=" << unsigned(token.total_coeff)
                      << ", TrailingOnes=" << unsigned(token.trailing_ones)
                      << '\n';
            return false;
        }

        std::cout << "[PASS] " << name << '\n';
        return true;
    }

    bool check_invalid_context()
    {
        const uint8_t data[] = { 0xFF };
        BitReader reader(data, sizeof(data));

        CoeffToken token;
        std::string error;

        if (decode_coeff_token(reader, -3, token, error))
        {
            std::cerr << "[FAIL] contexte nC=-3 invalide accepte\n";
            return false;
        }

        std::cout << "[PASS] contexte nC=-3 invalide rejete\n";
        return true;
    }

    bool check_invalid_fixed_code()
    {
        const auto bytes = make_bits("000010");
        BitReader reader(bytes.data(), bytes.size());

        CoeffToken token;
        std::string error;

        if (decode_coeff_token(reader, 8, token, error))
        {
            std::cerr << "[FAIL] code fixe invalide accepte\n";
            return false;
        }

        std::cout << "[PASS] code fixe invalide rejete\n";
        return true;
    }

    bool check_levels(
        const std::string& name,
        const std::string& encoded,
        int nC,
        const std::vector<int32_t>& expected)
    {
        const auto bytes = make_bits(encoded);
        BitReader reader(bytes.data(), bytes.size());

        CoeffToken token;
        std::string error;

        if (!decode_coeff_token(reader, nC, token, error))
        {
            std::cerr << "[FAIL] " << name
                      << " coeff_token: " << error << '\n';
            return false;
        }

        std::vector<int32_t> levels;

        if (!decode_level_values(reader, token, levels, error))
        {
            std::cerr << "[FAIL] " << name
                      << " levels: " << error << '\n';
            return false;
        }

        if (levels != expected)
        {
            std::cerr << "[FAIL] " << name << " : niveaux inattendus\n";
            std::cerr << "Obtenu : ";

            for (const auto level : levels)
                std::cerr << level << " ";

            std::cerr << "\nAttendu : ";

            for (const auto level : expected)
                std::cerr << level << " ";

            std::cerr << '\n';
            return false;
        }

        std::cout << "[PASS] " << name << '\n';
        return true;
    }

    bool check_truncated_level()
    {
        const auto bytes = make_bits("000101");
        BitReader reader(bytes.data(), bytes.size());

        CoeffToken token;
        std::string error;

        if (!decode_coeff_token(reader, 0, token, error))
            return false;

        std::vector<int32_t> levels;

        if (decode_level_values(reader, token, levels, error))
        {
            std::cerr << "[FAIL] Flux level tronque accepte\n";
            return false;
        }

        std::cout << "[PASS] Flux level tronque rejete\n";
        return true;
    }
    bool check_total_zeros_tables()
    {
        static const char* codes[15][16] =
        {
            { "1", "011", "010", "0011", "0010", "00011", "00010", "000011", "000010", "0000011", "0000010", "00000011", "00000010", "000000011", "000000010", "000000001" },
            { "111", "110", "101", "100", "011", "0101", "0100", "0011", "0010", "00011", "00010", "000011", "000010", "000001", "000000" },
            { "0101", "111", "110", "101", "0100", "0011", "100", "011", "0010", "00011", "00010", "000001", "00001", "000000" },
            { "00011", "111", "0101", "0100", "110", "101", "100", "0011", "011", "0010", "00010", "00001", "00000" },
            { "0101", "0100", "0011", "111", "110", "101", "100", "011", "0010", "00001", "0001", "00000" },
            { "000001", "00001", "111", "110", "101", "100", "011", "010", "0001", "001", "000000" },
            { "000001", "00001", "101", "100", "011", "11", "010", "0001", "001", "000000" },
            { "000001", "0001", "00001", "011", "11", "10", "010", "001", "000000" },
            { "000001", "000000", "0001", "11", "10", "001", "01", "00001" },
            { "00001", "00000", "001", "11", "10", "01", "0001" },
            { "0000", "0001", "001", "010", "1", "011" },
            { "0000", "0001", "01", "1", "001" },
            { "000", "001", "1", "01" },
            { "00", "01", "1" },
            { "0", "1" }
        };

        for (int total_coeff = 1; total_coeff <= 15; ++total_coeff)
        {
            for (int zeros = 0; zeros <= 16 - total_coeff; ++zeros)
            {
                const char* encoded = codes[total_coeff - 1][zeros];

                if (encoded == nullptr)
                {
                    std::cerr << "[FAIL] Code absent de la table\n";
                    return false;
                }

                const auto bytes = make_bits(encoded);
                BitReader reader(bytes.data(), bytes.size());

                uint8_t actual = 255;
                std::string error;

                if (!decode_total_zeros(
                    reader,
                    static_cast<uint8_t>(total_coeff),
                    actual,
                    error))
                {
                    std::cerr << "[FAIL] TotalCoeff=" << total_coeff
                              << " zeros=" << zeros
                              << " : " << error << '\n';
                    return false;
                }

                if (actual != zeros)
                {
                    std::cerr << "[FAIL] TotalCoeff=" << total_coeff
                              << " attendu=" << zeros
                              << " obtenu=" << unsigned(actual) << '\n';
                    return false;
                }
            }

            std::cout << "[PASS] Table total_zeros TotalCoeff="
                      << total_coeff << '\n';
        }

        return true;
    }

    bool check_total_zeros_full_block()
    {
        const uint8_t data = 0xA5;
        BitReader reader(&data, 1);

        const size_t before = reader.bits_remaining();
        uint8_t zeros = 255;
        std::string error;

        if (!decode_total_zeros(reader, 16, zeros, error))
        {
            std::cerr << "[FAIL] TotalCoeff=16 : " << error << '\n';
            return false;
        }

        if (zeros != 0 || reader.bits_remaining() != before)
        {
            std::cerr << "[FAIL] TotalCoeff=16 : resultat incorrect\n";
            return false;
        }

        std::cout << "[PASS] TotalCoeff=16 sans lecture de bits\n";
        return true;
    }

    bool check_total_zeros_invalid()
    {
        const auto bytes = make_bits("000000000");
        BitReader reader(bytes.data(), bytes.size());

        uint8_t zeros = 255;
        std::string error;

        if (decode_total_zeros(reader, 1, zeros, error))
        {
            std::cerr << "[FAIL] Code invalide accepte\n";
            return false;
        }

        std::cout << "[PASS] Code total_zeros invalide rejete\n";
        return true;
    }
    bool check_run_before_tables()
    {
        static const char* codes[7][15] =
        {
            { "1", "0" },
            { "1", "01", "00" },
            { "11", "10", "01", "00" },
            { "11", "10", "01", "001", "000" },
            { "11", "10", "011", "010", "001", "000" },
            { "11", "000", "001", "011", "010", "101", "100" },
            {
                "111", "110", "101", "100", "011",
                "010", "001", "0001", "00001", "000001",
                "0000001", "00000001", "000000001",
                "0000000001", "00000000001"
            }
        };

        for (int zeros_left = 1; zeros_left <= 15; ++zeros_left)
        {
            const int column =
                zeros_left <= 6 ? zeros_left - 1 : 6;

            const int maximum_run =
                zeros_left < 14 ? zeros_left : 14;

            for (int expected = 0; expected <= maximum_run; ++expected)
            {
                const char* encoded = codes[column][expected];

                if (encoded == nullptr)
                {
                    std::cerr << "[FAIL] Code absent : zerosLeft="
                              << zeros_left << " run=" << expected << '\n';
                    return false;
                }

                const auto bytes = make_bits(encoded);
                BitReader reader(bytes.data(), bytes.size());

                uint8_t actual = 255;
                std::string error;

                if (!decode_run_before(
                    reader,
                    static_cast<uint8_t>(zeros_left),
                    actual,
                    error))
                {
                    std::cerr << "[FAIL] zerosLeft=" << zeros_left
                              << " run=" << expected
                              << " : " << error << '\n';
                    return false;
                }

                if (actual != expected)
                {
                    std::cerr << "[FAIL] zerosLeft=" << zeros_left
                              << " attendu=" << expected
                              << " obtenu=" << unsigned(actual) << '\n';
                    return false;
                }
            }

            std::cout << "[PASS] run_before zerosLeft="
                      << zeros_left << '\n';
        }

        return true;
    }

    bool check_run_before_invalid()
    {
        const uint8_t data = 0xFF;

        for (uint8_t invalid : { uint8_t(0), uint8_t(16) })
        {
            BitReader reader(&data, 1);
            uint8_t run = 255;
            std::string error;

            if (decode_run_before(reader, invalid, run, error))
            {
                std::cerr << "[FAIL] zerosLeft invalide accepte : "
                          << unsigned(invalid) << '\n';
                return false;
            }
        }

        std::cout << "[PASS] zerosLeft invalides rejetes\n";
        return true;
    }

    bool check_run_before_truncated()
    {
        const uint8_t data = 0x00;
        BitReader reader(&data, 1);

        uint8_t run = 255;
        std::string error;

        if (decode_run_before(reader, 7, run, error))
        {
            std::cerr << "[FAIL] Flux run_before tronque accepte\n";
            return false;
        }

        std::cout << "[PASS] Flux run_before tronque rejete\n";
        return true;
    }
    bool check_residual_block(
        const std::string& name,
        const std::string& encoded,
        int nC,
        const std::array<int32_t, 16>& expected)
    {
        const auto bytes = make_bits(encoded);
        BitReader reader(bytes.data(), bytes.size());

        std::array<int32_t, 16> actual{};
        std::string error;

        if (!decode_residual_block_4x4(
            reader, nC, actual, error))
        {
            std::cerr << "[FAIL] " << name
                      << " : " << error << '\n';
            return false;
        }

        if (actual != expected)
        {
            std::cerr << "[FAIL] " << name
                      << " : coefficients incorrect\n";

            std::cerr << "Obtenu : ";
            for (auto value : actual)
                std::cerr << value << " ";

            std::cerr << "\nAttendu : ";
            for (auto value : expected)
                std::cerr << value << " ";

            std::cerr << '\n';
            return false;
        }

        std::cout << "[PASS] " << name << '\n';
        return true;
    }

}

int main()
{
    unsigned int passed = 0;
    unsigned int failed = 0;

    const auto run = [&](bool ok)
    {
        if (ok) ++passed;
        else ++failed;
    };

    run(check("Table nC 0-1 : bloc vide", "1", 0, 0, 0));
    run(check("Table nC 0-1 : un coefficient", "000101", 0, 1, 0));
    run(check("Table nC 0-1 : trailing one", "01", 0, 1, 1));
    run(check("Table nC 2-3 : bloc vide", "11", 2, 0, 0));
    run(check("Table nC 4-7 : bloc vide", "1111", 4, 0, 0));
    run(check("Table nC 8+ : bloc vide", "000011", 8, 0, 0));
    run(check("Table nC 8+ : trailing one", "000001", 8, 1, 1));
    run(check("Chroma 4:2:0 : bloc vide", "01", -1, 0, 0));
    run(check("Chroma 4:2:0 : trailing one", "1", -1, 1, 1));
    run(check("Chroma 4:2:2 : bloc vide", "1", -2, 0, 0));
    run(check("Chroma 4:2:2 : trailing one", "01", -2, 1, 1));
    run(check_invalid_context());
    run(check_invalid_fixed_code());

    run(check_levels(
       "Level positif +2",
       "0001011",
       0,
       { 2 }
   ));

   run(check_levels(
       "Level negatif -2",
       "00010101",
       0,
       { -2 }
   ));

   run(check_levels(
       "Trailing one positif",
       "010",
       0,
       { 1 }
   ));

   run(check_levels(
       "Trailing one negatif",
       "011",
       0,
       { -1 }
   ));

    run(check_levels(
        "Deux niveaux +2 et -2",
        "000001111011",
        0,
        { 2, -2 }
    ));

    run(check_levels(
        "Trois niveaux avec trailing one",
        "0000011001011",
        0,
        { 1, 2, -2 }
    ));

    run(check_levels(
        "Augmentation suffix_length",
        "0000011100001110",
        0,
        { 4, 2 }
    ));

    run(check_truncated_level());
    run(check_total_zeros_tables());
    run(check_total_zeros_full_block());
    run(check_total_zeros_invalid());
    run(check_run_before_tables());
    run(check_run_before_invalid());
    run(check_run_before_truncated());
    std::array<int32_t, 16> expected_empty{};
    run(check_residual_block(
        "Bloc 4x4 vide",
        "1",
        0,
        expected_empty
    ));

    std::array<int32_t, 16> expected_positive{};
    expected_positive[1] = 2;

    run(check_residual_block(
        "Bloc 4x4 coefficient +2 position zigzag 1",
        "0001011011",
        0,
        expected_positive
    ));

    std::array<int32_t, 16> expected_trailing_positive{};
    expected_trailing_positive[1] = 1;

    run(check_residual_block(
        "Bloc 4x4 trailing one positif",
        "010011",
        0,
        expected_trailing_positive
    ));

    std::array<int32_t, 16> expected_trailing_negative{};
    expected_trailing_negative[1] = -1;

    run(check_residual_block(
        "Bloc 4x4 trailing one negatif",
        "011011",
        0,
        expected_trailing_negative
    ));

    // Test : deux trailing ones avec un zero entre les coefficients.
    std::array<int32_t, 16> expected_multi_trailing{};
    expected_multi_trailing[0] = -1;
    expected_multi_trailing[4] = 1;

    run(check_residual_block(
        "Bloc 4x4 deux trailing ones avec un zero",
        "001011100",
        0,
        expected_multi_trailing
    ));

    // Test : deux niveaux non-trailing avec deux zeros.
    std::array<int32_t, 16> expected_multi_levels{};
    expected_multi_levels[4] = -2;
    expected_multi_levels[8] = 2;

    run(check_residual_block(
        "Bloc 4x4 deux niveaux avec deux zeros",
        "0000011110111011",
        0,
        expected_multi_levels
    ));
std::cout << "\nCAVLC tests: " << passed
              << " passed, " << failed << " failed.\n";

    return failed == 0 ? 0 : 1;
}




