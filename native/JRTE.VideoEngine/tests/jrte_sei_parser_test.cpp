#include "jrte_sei_parser.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    using jrte::h264::parse_sei;

    int failures = 0;

    auto check = [&](bool condition, const char* name)
    {
        if (condition)
            std::cout << "[OK] " << name << "\n";
        else
        {
            std::cerr << "[ECHEC] " << name << "\n";
            ++failures;
        }
    };

    const std::vector<uint8_t> multiple = {
        0x06, 0x05, 0x02, 0xAA, 0xBB,
        0x01, 0x01, 0xCC,
        0x80
    };

    const auto a = parse_sei(multiple.data(), multiple.size());
    check(a.valid && a.messages.size() == 2, "Deux messages SEI");
    check(a.valid && a.messages[0].payload_type == 5, "Payload type 5");
    check(a.valid && a.messages[0].payload_size == 2, "Payload size 2");
    check(a.valid && a.messages[0].payload.size() == 2, "Payload conserve");
    check(a.valid && a.messages[1].payload_type == 1, "Second message type 1");

    const std::vector<uint8_t> extended = {
        0x06, 0xFF, 0x01, 0x00, 0x80
    };
    const auto b = parse_sei(extended.data(), extended.size());
    check(b.valid && b.messages.size() == 1 &&
          b.messages[0].payload_type == 256, "Payload type etendu");

    const std::vector<uint8_t> truncated = {
        0x06, 0x05, 0x03, 0xAA, 0x80
    };
    const auto c = parse_sei(truncated.data(), truncated.size());
    check(!c.valid, "Payload tronque rejete");

    const std::vector<uint8_t> missing_trailing = {
        0x06, 0x05, 0x01, 0xAA
    };
    const auto d = parse_sei(missing_trailing.data(), missing_trailing.size());
    check(!d.valid, "Bits de fin absents rejetes");

    check(!parse_sei(nullptr, 0).valid, "NAL vide rejete");

    if (failures != 0)
    {
        std::cerr << failures << " test(s) en echec.\n";
        return 1;
    }

    std::cout << "Tous les tests SEI sont reussis.\n";
    return 0;
}
