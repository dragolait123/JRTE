#include "jrte_inverse_transform.h"

#include <array>
#include <iostream>
#include <string>

int main()
{
    using namespace jrte::h264;

    std::string error;
    std::array<int32_t, 16> input{};
    std::array<int32_t, 16> output{};

    // Test 1: zero block.
    if (!inverse_transform_4x4(input, output, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    for (int32_t value : output)
    {
        if (value != 0)
        {
            std::cerr << "Zero-block test failed.\n";
            return 1;
        }
    }

    // Test 2: DC-only block.
    input.fill(0);
    input[0] = 4096;

    if (!inverse_transform_4x4(input, output, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    for (int32_t value : output)
    {
        if (value != 64)
        {
            std::cerr << "DC-only test failed. Got: "
                      << value << '\n';
            return 1;
        }
    }

    std::cout << "JRTE inverse transform tests passed.\n";
    return 0;
}
