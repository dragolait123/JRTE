#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jrte::h264
{
    struct SpsInfo
    {
        bool valid = false;

        uint32_t profile_idc = 0;
        uint32_t level_idc = 0;
        uint32_t sps_id = 0;

    // Parametres necessaires au DPB H.264.
    uint32_t log2_max_frame_num = 4;
    uint32_t pic_order_cnt_type = 0;
    uint32_t log2_max_pic_order_cnt_lsb = 4;

    bool delta_pic_order_always_zero = false;
    int32_t offset_for_non_ref_pic = 0;
    int32_t offset_for_top_to_bottom_field = 0;
    std::vector<int32_t> offset_for_ref_frame;

    uint32_t max_num_ref_frames = 0;
    bool gaps_in_frame_num_allowed = false;
        uint32_t chroma_format_idc = 1;
        uint32_t bit_depth_luma = 8;
        uint32_t bit_depth_chroma = 8;

        bool separate_colour_plane = false;
        bool mb_adaptive_frame_field = false;
        bool direct_8x8_inference = false;
        uint32_t width_in_mbs = 0;
        uint32_t height_in_map_units = 0;

        uint32_t width = 0;
        uint32_t height = 0;

        bool frame_mbs_only = true;
        bool frame_cropping = false;

        uint32_t crop_left = 0;
        uint32_t crop_right = 0;
        uint32_t crop_top = 0;
        uint32_t crop_bottom = 0;

        std::string error;
    };

    // Accepte une unite NAL SPS complete, avec son octet d'en-tete.
    SpsInfo parse_sps(const uint8_t* nal, uint32_t nal_size);
}


