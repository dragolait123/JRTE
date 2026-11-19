#include "jrte_inverse_quant.h"

#include <array>
#include <iostream>
#include <string>
#include <limits>

int main()
{
    using namespace jrte::h264;

    std::string error;
    QuantBlock4x4 input{};
    QuantBlock4x4 output{};

    // Matrice plate de test : 16 partout.
    ScalingList4x4 scaling{};
    scaling.fill(16);

    // Test 1 : bloc nul.
    if (!inverse_quantize_4x4(input, 0, scaling, output, error))
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

    // Test 2 : QP 0, coefficients égaux à 1.
    input.fill(1);

    const QuantBlock4x4 expected_qp0 =
    {
        10, 13, 10, 13,
        13, 16, 13, 16,
        10, 13, 10, 13,
        13, 16, 13, 16
    };

    if (!inverse_quantize_4x4(input, 0, scaling, output, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    if (output != expected_qp0)
    {
        std::cerr << "QP 0 test failed.\n";
        return 1;
    }

    // Test 3 : QP 24.
    const QuantBlock4x4 expected_qp24 =
    {
        160, 208, 160, 208,
        208, 256, 208, 256,
        160, 208, 160, 208,
        208, 256, 208, 256
    };

    if (!inverse_quantize_4x4(input, 24, scaling, output, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    if (output != expected_qp24)
    {
        std::cerr << "QP 24 test failed.\n";
        return 1;
    }

    // Test 4 : coefficient négatif.
    input.fill(0);
    input[0] = -1;

    if (!inverse_quantize_4x4(input, 0, scaling, output, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    if (output[0] != -10)
    {
        std::cerr << "Negative coefficient test failed.\n";
        return 1;
    }

    // Test 5 : QP invalide.
    if (inverse_quantize_4x4(input, 52, scaling, output, error))
    {
        std::cerr << "Invalid QP test failed.\n";
        return 1;
    }

    // Test 6 : matrice invalide.
    scaling[0] = 0;

    if (inverse_quantize_4x4(input, 0, scaling, output, error))
    {
        std::cerr << "Zero scaling-list test failed.\n";
        return 1;
    }

    // Test 7 : dépassement de la plage H.264.
    scaling.fill(255);
    input.fill(0);
    input[0] = std::numeric_limits<int32_t>::max();

    if (inverse_quantize_4x4(input, 24, scaling, output, error))
    {
        std::cerr << "Overflow range test failed.\n";
        return 1;
    }

    std::cout << "JRTE inverse quantization tests passed.\n";
    return 0;
}
