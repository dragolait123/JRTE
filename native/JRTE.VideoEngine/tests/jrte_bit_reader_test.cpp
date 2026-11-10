#include "jrte_bit_reader.h"

#include <iostream>
#include <vector>
#include <string>

using namespace jrte::h264;

int failures = 0;

void check(bool condition, const std::string& name)
{
    if (condition)
        std::cout << "[OK] " << name << "\n";
    else
    {
        std::cout << "[ECHEC] " << name << "\n";
        ++failures;
    }
}

int main()
{
    std::cout << "===== JRTE BIT READER TESTS =====\n\n";

    {
        const uint8_t data[] = { 0x80 };
        BitReader reader(data, sizeof(data));
        uint32_t value = 99;

        check(reader.read_ue(value) && value == 0,
              "Exp-Golomb ue(0)");
    }

    {
        const uint8_t data[] = { 0x40 };
        BitReader reader(data, sizeof(data));
        uint32_t value = 99;

        check(reader.read_ue(value) && value == 1,
              "Exp-Golomb ue(1)");
    }

    {
        const uint8_t data[] = { 0x60 };
        BitReader reader(data, sizeof(data));
        uint32_t value = 99;

        check(reader.read_ue(value) && value == 2,
              "Exp-Golomb ue(2)");
    }

    {
        const uint8_t data[] = { 0x20 };
        BitReader reader(data, sizeof(data));
        uint32_t value = 99;

        check(reader.read_ue(value) && value == 3,
              "Exp-Golomb ue(3)");
    }

    {
        const uint8_t data[] = { 0x80 };
        BitReader reader(data, sizeof(data));
        int32_t value = 99;

        check(reader.read_se(value) && value == 0,
              "Exp-Golomb se(0)");
    }

    {
        const uint8_t data[] = { 0x40 };
        BitReader reader(data, sizeof(data));
        int32_t value = 99;

        check(reader.read_se(value) && value == 1,
              "Exp-Golomb se(+1)");
    }

    {
        const uint8_t data[] = { 0x60 };
        BitReader reader(data, sizeof(data));
        int32_t value = 99;

        check(reader.read_se(value) && value == -1,
              "Exp-Golomb se(-1)");
    }

    {
        const uint8_t input[] = {
            0x11, 0x00, 0x00, 0x03, 0x01, 0x22
        };

        std::vector<uint8_t> output;
        std::string error;

        const bool result = ebsp_to_rbsp(
            input, sizeof(input), output, error);

        const std::vector<uint8_t> expected = {
            0x11, 0x00, 0x00, 0x01, 0x22
        };

        check(result && output == expected,
              "Conversion EBSP vers RBSP");
    }

    {
        const uint8_t input[] = {
            0x00, 0x00, 0x03, 0x00,
            0x00, 0x03, 0x01
        };

        std::vector<uint8_t> output;
        std::string error;

        const bool result = ebsp_to_rbsp(
            input, sizeof(input), output, error);

        const std::vector<uint8_t> expected = {
            0x00, 0x00, 0x00, 0x00, 0x01
        };

        check(result && output == expected,
              "EBSP avec echappements consecutifs");
    }
    {
        const uint8_t data[] = { 0x80 };
        BitReader reader(data, sizeof(data));
        uint32_t value = 0;

        const bool first = reader.read_bit(value);
        const bool second = reader.read_bits(8, value);

        check(first && !second && !reader.good(),
              "Detection de fin du flux");
    }

    {
        const uint8_t data[] = { 0x12, 0x34, 0x56, 0x78 };
        BitReader reader(data, sizeof(data));
        uint32_t value = 0;

        check(reader.read_bits(32, value) &&
              value == 0x12345678,
              "Lecture de 32 bits");
    }

    {
        const uint8_t data[] = { 0xAB, 0xC0 };
        BitReader reader(data, sizeof(data));
        uint32_t value = 0;

        check(reader.read_bits(12, value) &&
              value == 0xABC,
              "Lecture de bits entre deux octets");
    }

    {
        const uint8_t data[] = { 0xFF };
        BitReader reader(data, sizeof(data));
        uint32_t value = 99;

        check(reader.read_bits(0, value) &&
              value == 0 &&
              reader.bits_remaining() == 8,
              "Lecture de zero bit");
    }

    {
        BitReader reader(nullptr, 0);
        uint32_t value = 0;

        check(reader.good() &&
              !reader.read_bit(value) &&
              !reader.good(),
              "Flux vide");
    }

    {
        BitReader reader(nullptr, 10);

        check(!reader.good(),
              "Pointeur nul avec taille non nulle");
    }

    {
        const uint8_t data[] = { 0x00 };
        BitReader reader(data, sizeof(data));
        uint32_t value = 0;

        check(!reader.read_ue(value) &&
              !reader.good(),
              "Code Exp-Golomb incomplet");
    }

    {
        const uint8_t input[] = { 0x00, 0x00, 0x03 };
        std::vector<uint8_t> output;
        std::string error;

        check(ebsp_to_rbsp(input, sizeof(input), output, error) &&
              output.size() == 3 &&
              output[2] == 0x03,
              "Octet 03 final conserve");
    }

    {
        const uint8_t input[] = { 0x00, 0x00, 0x03, 0x03 };
        std::vector<uint8_t> output;
        std::string error;

        const std::vector<uint8_t> expected = {
            0x00, 0x00, 0x03
        };

        check(ebsp_to_rbsp(input, sizeof(input), output, error) &&
              output == expected,
              "Echappement avant octet 03");
    }
    std::cout << "\nResultat : "
              << failures << " echec(s)\n";

    return failures == 0 ? 0 : 1;
}


