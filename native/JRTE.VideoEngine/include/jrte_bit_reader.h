#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace jrte::h264
{
    bool ebsp_to_rbsp(
        const uint8_t* input,
        size_t input_size,
        std::vector<uint8_t>& output,
        std::string& error
    );

    class BitReader
    {
    public:
        BitReader(const uint8_t* data, size_t size);

        bool read_bit(uint32_t& value);
        bool read_bits(uint32_t count, uint32_t& value);
        bool read_ue(uint32_t& value);
        bool read_se(int32_t& value);

        size_t bits_remaining() const;
        bool good() const;
        const std::string& error() const;

    private:
        const uint8_t* data_;
        size_t size_;
        size_t bit_position_;
        size_t total_bits_;
        bool valid_;
        std::string error_;

        bool fail(const std::string& message);
    };
}

