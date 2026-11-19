#include "jrte_api.h"
#include "jrte_inverse_quant.h"

#include <cstdint>

namespace jrte::h264
{
    namespace
    {
        // LevelScale factors from H.264.
        // Columns: even/even, mixed parity, odd/odd.
        constexpr int64_t LEVEL_SCALE[6][3] =
        {
            {10, 13, 16},
            {11, 14, 18},
            {13, 16, 20},
            {14, 18, 23},
            {16, 20, 25},
            {18, 23, 29}
        };

        int get_position_class(int row, int col)
        {
            if ((row % 2 == 0) && (col % 2 == 0))
                return 0;

            if ((row % 2 == 1) && (col % 2 == 1))
                return 2;

            return 1;
        }

        int64_t floor_div_pow2(int64_t value, int shift)
        {
            const int64_t divisor = int64_t{1} << shift;

            if (value >= 0)
                return value / divisor;

            return -((-value + divisor - 1) / divisor);
        }
    }

    bool inverse_quantize_4x4(
        const QuantBlock4x4& coefficients,
        int qp,
        const ScalingList4x4& scaling_list,
        QuantBlock4x4& output,
        std::string& error)
    {
        error.clear();
        output.fill(0);

        if (qp < 0 || qp > 51)
        {
            error = "QP invalide : la plage prise en charge est 0 a 51.";
            return false;
        }

        for (uint8_t value : scaling_list)
        {
            if (value == 0)
            {
                error = "La matrice d'echelle ne peut pas contenir de zero.";
                return false;
            }
        }

        const int qp_div6 = qp / 6;
        const int qp_mod6 = qp % 6;

        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                const int index = row * 4 + col;
                const int position_class =
                    get_position_class(row, col);

                const int64_t level_scale =
                    LEVEL_SCALE[qp_mod6][position_class];

                const int64_t product =
                    static_cast<int64_t>(coefficients[index]) *
                    static_cast<int64_t>(scaling_list[index]) *
                    level_scale;

                int64_t value = 0;

                if (qp >= 24)
                {
                    const int shift = qp_div6 - 4;
                    value = product * (int64_t{1} << shift);
                }
                else
                {
                    const int shift = 4 - qp_div6;
                    const int64_t rounding =
                        int64_t{1} << (shift - 1);

                    value = floor_div_pow2(
                        product + rounding,
                        shift);
                }

                // H.264 8-bit residual coefficient range.
                if (value < -32768 || value > 32767)
                {
                    output.fill(0);
                    error = "Coefficient hors de la plage H.264 8 bits.";
                    return false;
                }

                output[index] = static_cast<int32_t>(value);
            }
        }

        return true;
    }
}
