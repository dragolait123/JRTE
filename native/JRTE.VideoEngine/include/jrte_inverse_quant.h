#pragma once

#include "jrte_api.h"

#include <array>
#include <cstdint>
#include <string>

namespace jrte::h264
{
    using QuantBlock4x4 = std::array<int32_t, 16>;
    using ScalingList4x4 = std::array<uint8_t, 16>;

    // scaling_list must be resolved and stored in raster order.
    // This function handles ordinary residual 4x4 blocks.
    bool inverse_quantize_4x4(
        const QuantBlock4x4& coefficients,
        int qp,
        const ScalingList4x4& scaling_list,
        QuantBlock4x4& output,
        std::string& error);
}
