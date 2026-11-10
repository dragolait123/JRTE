#pragma once

#include <cstdint>
#include <string>

namespace jrte::h264
{
    struct PpsInfo
    {
        bool valid = false;

        uint32_t pps_id = 0;
        uint32_t sps_id = 0;

        bool entropy_coding_mode = false;
        bool bottom_field_pic_order_in_frame_present = false;

        uint32_t num_slice_groups = 1;
        uint32_t slice_group_map_type = 0;

        bool slice_group_change_direction = false;
        uint32_t slice_group_change_rate = 1;
        uint32_t num_ref_idx_l0_default_active = 1;
        uint32_t num_ref_idx_l1_default_active = 1;

        bool weighted_pred = false;
        uint32_t weighted_bipred_idc = 0;

        int32_t pic_init_qp_minus26 = 0;
        int32_t pic_init_qs_minus26 = 0;
        int32_t chroma_qp_index_offset = 0;

        bool deblocking_filter_control_present = false;
        bool constrained_intra_pred = false;
        bool redundant_pic_cnt_present = false;

        std::string error;
    };

    // Accepte une unite NAL PPS complete, avec son octet d'en-tete.
    PpsInfo parse_pps(const uint8_t* nal, uint32_t nal_size);
}

