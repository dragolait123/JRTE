#include "jrte_sei_parser.h"
#include "jrte_bit_reader.h"

#include <limits>

namespace jrte::h264
{
    static bool read_sei_value(
        const std::vector<uint8_t>& rbsp,
        size_t& offset,
        uint32_t& value,
        std::string& error)
    {
        uint64_t total = 0;

        while (offset < rbsp.size())
        {
            const uint8_t byte = rbsp[offset++];

            if (total > std::numeric_limits<uint32_t>::max() - byte)
            {
                error = "Valeur SEI depassant uint32_t.";
                return false;
            }

            total += byte;

            if (byte != 0xFF)
            {
                value = static_cast<uint32_t>(total);
                return true;
            }
        }

        error = "Champ payloadType ou payloadSize tronque.";
        return false;
    }

    SeiInfo parse_sei(const uint8_t* nal_data, size_t nal_size)
    {
        SeiInfo result;

        if (nal_data == nullptr || nal_size < 2)
        {
            result.error = "NAL SEI vide ou incomplet.";
            return result;
        }

        const uint8_t header = nal_data[0];

        if ((header & 0x80) != 0 || (header & 0x1F) != 6)
        {
            result.error = "En-tete NAL SEI invalide.";
            return result;
        }

        std::vector<uint8_t> rbsp;
        std::string conversion_error;

        if (!ebsp_to_rbsp(nal_data + 1, nal_size - 1, rbsp, conversion_error))
        {
            result.error = conversion_error;
            return result;
        }

        if (rbsp.empty())
        {
            result.error = "RBSP SEI vide.";
            return result;
        }

        size_t offset = 0;

        while (offset < rbsp.size())
        {
            if (rbsp[offset] == 0x80)
            {
                if (offset != rbsp.size() - 1)
                {
                    result.error = "Donnees presentes apres rbsp_trailing_bits.";
                    return result;
                }

                result.valid = true;
                return result;
            }

            SeiMessage message;

            if (!read_sei_value(rbsp, offset, message.payload_type, result.error))
                return result;

            if (!read_sei_value(rbsp, offset, message.payload_size, result.error))
                return result;

            if (message.payload_size > rbsp.size() - offset)
            {
                result.error = "Payload SEI tronque ou taille invalide.";
                return result;
            }

            message.payload.assign(
                rbsp.begin() + static_cast<std::ptrdiff_t>(offset),
                rbsp.begin() + static_cast<std::ptrdiff_t>(offset + message.payload_size)
            );

            offset += message.payload_size;
            result.messages.push_back(std::move(message));
        }

        result.error = "Bits de fin RBSP SEI absents.";
        return result;
    }
}
