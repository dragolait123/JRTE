#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jrte::h264
{
    struct SeiMessage
    {
        uint32_t payload_type = 0;
        uint32_t payload_size = 0;
        std::vector<uint8_t> payload;
    };

    struct SeiInfo
    {
        bool valid = false;
        std::vector<SeiMessage> messages;
        std::string error;
    };

    SeiInfo parse_sei(
        const uint8_t* nal_data,
        size_t nal_size
    );
}
