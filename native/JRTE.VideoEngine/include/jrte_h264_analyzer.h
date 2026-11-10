#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "jrte_sps_parser.h"
#include "jrte_pps_parser.h"
#include "jrte_slice_parser.h"

namespace jrte::h264
{
    struct AvcConfiguration
    {
        uint8_t profile = 0;
        uint8_t compatibility = 0;
        uint8_t level = 0;
        uint32_t length_size = 0;

        std::vector<std::vector<uint8_t>> sps_units;
        std::vector<std::vector<uint8_t>> pps_units;

        bool valid = false;
        std::string error;
    };

    struct NalUnitInfo
    {
        uint32_t index = 0;
        uint32_t size = 0;
        uint8_t nal_type = 0;
        uint8_t nal_ref_idc = 0;
        std::string type_name;
    };

    struct AnalysisResult
    {
        bool success = false;
        uint32_t length_size = 0;
        uint32_t total_nal_units = 0;
	uint32_t valid_slice_count = 0;
	std::vector<SliceInfo> slice_info;

        uint32_t idr_count = 0;
        uint32_t slice_count = 0;
        uint32_t sps_count = 0;
        uint32_t pps_count = 0;
        uint32_t valid_sps_count = 0;
        uint32_t valid_pps_count = 0;

        std::vector<SpsInfo> sps_info;
        std::vector<PpsInfo> pps_info;
        std::vector<NalUnitInfo> units;
        std::string error;
    };

    AvcConfiguration parse_avcc(
        const uint8_t* data,
        uint32_t size
    );

    AnalysisResult analyze_sample(
        const uint8_t* data,
        uint32_t size,
        uint32_t length_size
    );

    AnalysisResult analyze_sample(
        const uint8_t* data,
        uint32_t size,
        const AvcConfiguration& configuration
    );
}


