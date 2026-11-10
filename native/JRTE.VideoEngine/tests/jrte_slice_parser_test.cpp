#include "jrte_slice_parser.h"

#include <iostream>
#include <vector>
#include <string>

using namespace jrte::h264;

bool test_slice(
    const std::string& name,
    const std::vector<uint8_t>& nal,
    uint32_t expected_type,
    const std::string& expected_name)
{
    const SliceInfo info = parse_slice_header(
        nal.data(),
        static_cast<uint32_t>(nal.size())
    );

    if (!info.valid)
    {
        std::cout << "[ECHEC] " << name
                  << " : " << info.error << "\n";
        return false;
    }

    const bool passed =
        info.first_mb_in_slice == 0 &&
        info.slice_type == expected_type &&
        info.pic_parameter_set_id == 0 &&
        info.slice_type_name == expected_name;

    std::cout << (passed ? "[OK] " : "[ECHEC] ")
              << name
              << " | first_mb=" << info.first_mb_in_slice
              << " | slice_type=" << info.slice_type
              << " | PPS=" << info.pic_parameter_set_id
              << " | type=" << info.slice_type_name
              << "\n";

    return passed;
}

int main()
{
    std::cout << "====================================\n";
    std::cout << "     JRTE - SLICE PARSER TEST\n";
    std::cout << "====================================\n";

    // NAL type 1 : P slice
    const std::vector<uint8_t> p_slice = {
        0x41, 0xE0
    };

    // NAL type 1 : B slice
    const std::vector<uint8_t> b_slice = {
        0x41, 0xA8
    };

    // NAL type 5 : IDR I slice
    const std::vector<uint8_t> i_slice = {
        0x65, 0xB8
    };

    int passed = 0;
    int total = 3;

    if (test_slice("P-SLICE", p_slice, 0, "P"))
        ++passed;

    if (test_slice("B-SLICE", b_slice, 1, "B"))
        ++passed;

    if (test_slice("I-SLICE", i_slice, 2, "I"))
        ++passed;

    std::cout << "------------------------------------\n";
    std::cout << "RESULTAT : " << passed
              << "/" << total << " tests reussis\n";

    return passed == total ? 0 : 1;
}
