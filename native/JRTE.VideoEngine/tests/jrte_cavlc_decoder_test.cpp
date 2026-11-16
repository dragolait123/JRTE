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

        if (decode_coeff_token(reader, -1, token, error))
        {
            std::cerr << "[FAIL] contexte chroma non supporte accepte\n";
            return false;
        }

        std::cout << "[PASS] contexte chroma non supporte rejete\n";
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
    run(check_invalid_context());
    run(check_invalid_fixed_code());

    std::cout << "\nCAVLC tests: " << passed
              << " passed, " << failed << " failed.\n";

    return failed == 0 ? 0 : 1;
}
