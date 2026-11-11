#include "jrte_bit_reader.h"

#include <limits>

namespace jrte::h264
{
    bool ebsp_to_rbsp(
        const uint8_t* input,
        size_t input_size,
        std::vector<uint8_t>& output,
        std::string& error)
    {
        output.clear();
        error.clear();

        if (input == nullptr && input_size != 0)
        {
            error = "Pointeur EBSP invalide.";
            return false;
        }

        unsigned int zero_count = 0;

        for (size_t i = 0; i < input_size; ++i)
        {
            const uint8_t byte = input[i];

            if (zero_count >= 2)
            {
                if (byte == 0x03 &&
                    i + 1 < input_size &&
                    input[i + 1] <= 0x03)
                {
                    zero_count = 0;
                    continue;
                }

                if (byte <= 0x02)
                {
                    output.clear();
                    error = "Sequence EBSP invalide : prevention absente.";
                    return false;
                }
            }

            output.push_back(byte);

            if (byte == 0x00)
                ++zero_count;
            else
                zero_count = 0;
        }

        return true;
    }

    BitReader::BitReader(const uint8_t* data, size_t size)
        : data_(data),
          size_(size),
          bit_position_(0),
          total_bits_(0),
          valid_(data != nullptr || size == 0)
    {
        if (!valid_)
        {
            error_ = "Pointeur de donnees invalide.";
            return;
        }

        if (size_ > std::numeric_limits<size_t>::max() / 8)
        {
            valid_ = false;
            error_ = "Taille du flux trop grande.";
            return;
        }

        total_bits_ = size_ * 8;
    }

    bool BitReader::fail(const std::string& message)
    {
        valid_ = false;
        error_ = message;
        return false;
    }

    bool BitReader::read_bit(uint32_t& value)
    {
        value = 0;

        if (!valid_)
            return false;

        if (bit_position_ >= total_bits_)
            return fail("Fin du flux de bits.");

        const size_t byte_index = bit_position_ / 8;
        const uint32_t bit_index =
            7 - static_cast<uint32_t>(bit_position_ % 8);

        value = (data_[byte_index] >> bit_index) & 1;
        ++bit_position_;

        return true;
    }

    bool BitReader::read_bits(uint32_t count, uint32_t& value)
    {
        value = 0;

        if (!valid_)
            return false;

        if (count > 32)
            return fail("Lecture superieure a 32 bits.");

        if (count > bits_remaining())
            return fail("Pas assez de bits disponibles.");

        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t bit = 0;

            if (!read_bit(bit))
                return false;

            value = (value << 1) | bit;
        }

        return true;
    }

    bool BitReader::read_ue(uint32_t& value)
    {
        value = 0;

        if (!valid_)
            return false;

        uint32_t leading_zeros = 0;
        uint32_t bit = 0;

        while (true)
        {
            if (!read_bit(bit))
                return false;

            if (bit == 1)
                break;

            ++leading_zeros;

            if (leading_zeros > 31)
                return fail("Code Exp-Golomb non signe trop long.");
        }

        uint32_t suffix = 0;

        if (leading_zeros > 0 &&
            !read_bits(leading_zeros, suffix))
        {
            return false;
        }

        const uint64_t result =
            ((uint64_t(1) << leading_zeros) - 1) + suffix;

        if (result > std::numeric_limits<uint32_t>::max())
            return fail("Valeur Exp-Golomb hors limites.");

        value = static_cast<uint32_t>(result);
        return true;
    }

    bool BitReader::read_se(int32_t& value)
    {
        value = 0;

        uint32_t code_num = 0;

        if (!read_ue(code_num))
            return false;

        const int64_t decoded =
            (code_num & 1)
                ? int64_t(code_num + uint64_t(1)) / 2
                : -int64_t(code_num) / 2;

        if (decoded < std::numeric_limits<int32_t>::min() ||
            decoded > std::numeric_limits<int32_t>::max())
        {
            return fail("Valeur Exp-Golomb signee hors limites.");
        }

        value = static_cast<int32_t>(decoded);
        return true;
    }

    size_t BitReader::bits_remaining() const
    {
        return bit_position_ <= total_bits_
            ? total_bits_ - bit_position_
            : 0;
    }

    bool BitReader::good() const
    {
        return valid_;
    }

    const std::string& BitReader::error() const
    {
        return error_;
    }
}



