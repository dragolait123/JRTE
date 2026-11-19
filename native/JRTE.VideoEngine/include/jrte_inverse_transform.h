#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace jrte::h264
{
    // Input: 4x4 dequantized H.264 transform coefficients.
    // Output: 4x4 spatial residual samples.
    bool inverse_transform_4x4(
        const std::array<int32_t, 16>& input,
        std::array<int32_t, 16>& output,
        std::string& error
    );
}
