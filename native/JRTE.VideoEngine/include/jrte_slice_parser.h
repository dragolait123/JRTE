#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include "jrte_sps_parser.h"
#include "jrte_pps_parser.h"

namespace jrte::h264
{
    struct SliceInfo
    {
        bool valid = false;
        bool core_header_complete = false;

        uint32_t nal_type = 0;
        uint32_t nal_ref_idc = 0;

        uint32_t first_mb_in_slice = 0;
        uint32_t slice_type = 0;
        uint32_t pic_parameter_set_id = 0;
        std::string slice_type_name;

        uint32_t colour_plane_id = 0;
        uint32_t frame_num = 0;
        bool field_pic_flag = false;
        bool bottom_field_flag = false;
        uint32_t idr_pic_id = 0;

        uint32_t pic_order_cnt_lsb = 0;
        int32_t delta_pic_order_cnt_bottom = 0;
        int32_t delta_pic_order_cnt0 = 0;
        int32_t delta_pic_order_cnt1 = 0;

        uint32_t redundant_pic_cnt = 0;

        bool direct_spatial_mv_pred_flag = false;

        bool num_ref_idx_active_override_flag = false;
        uint32_t num_ref_idx_l0_active_minus1 = 0;
        uint32_t num_ref_idx_l1_active_minus1 = 0;

        uint32_t effective_num_ref_idx_l0 = 0;
        uint32_t effective_num_ref_idx_l1 = 0;

        bool ref_pic_list_modification_flag_l0 = false;
        bool ref_pic_list_modification_flag_l1 = false;

        bool no_output_of_prior_pics_flag = false;
        bool long_term_reference_flag = false;
        bool adaptive_ref_pic_marking_mode_flag = false;

        uint32_t cabac_init_idc = 0;
        bool cabac_alignment_valid = false;
        size_t cabac_data_bit_offset = 0;
        size_t cabac_data_byte_offset = 0;

        int32_t slice_qp_delta = 0;
        bool sp_for_switch_flag = false;
        int32_t slice_qs_delta = 0;

        uint32_t disable_deblocking_filter_idc = 0;
        int32_t slice_alpha_c0_offset_div2 = 0;
        int32_t slice_beta_offset_div2 = 0;

        uint32_t slice_group_change_cycle = 0;

        std::string error;
    };

    SliceInfo parse_slice_header(
        const uint8_t* nal,
        uint32_t nal_size
    );

    SliceInfo parse_slice_header(
        const uint8_t* nal,
        uint32_t nal_size,
        const SpsInfo& sps,
        const PpsInfo& pps
    );
}
