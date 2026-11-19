#include "jrte_inverse_transform.h"

#include <cstddef>
#include <limits>

namespace jrte::h264
{
    namespace
    {
        using Block64 = std::array<int64_t, 16>;

        void transform_1d(
            const int64_t* input,
            int64_t* output)
        {
            const int64_t s0 = input[0] + input[2];
            const int64_t s1 = input[0] - input[2];
            const int64_t s2 = (input[1] >> 1) - input[3];
            const int64_t s3 = input[1] + (input[3] >> 1);

            output[0] = s0 + s3;
            output[1] = s1 + s2;
            output[2] = s1 - s2;
            output[3] = s0 - s3;
        }
    }

    bool inverse_transform_4x4(
        const std::array<int32_t, 16>& input,
        std::array<int32_t, 16>& output,
        std::string& error)
    {
        error.clear();
        output.fill(0);

        Block64 source{};
        Block64 horizontal{};
        Block64 vertical{};

        for (size_t i = 0; i < 16; ++i)
            source[i] = input[i];

        for (int row = 0; row < 4; ++row)
        {
            transform_1d(
                source.data() + row * 4,
                horizontal.data() + row * 4);
        }

        for (int col = 0; col < 4; ++col)
        {
            int64_t in[4] = {
                horizontal[col],
                horizontal[4 + col],
                horizontal[8 + col],
                horizontal[12 + col]
            };

            int64_t out[4]{};
            transform_1d(in, out);

            for (int row = 0; row < 4; ++row)
                vertical[row * 4 + col] = out[row];
        }

        for (size_t i = 0; i < 16; ++i)
        {
            const int64_t value = (vertical[i] + 32) >> 6;

            if (value < std::numeric_limits<int32_t>::min() ||
                value > std::numeric_limits<int32_t>::max())
            {
                output.fill(0);
                error = "Depassement pendant la transformation inverse.";
                return false;
            }

            output[i] = static_cast<int32_t>(value);
        }

        return true;
    }
}
