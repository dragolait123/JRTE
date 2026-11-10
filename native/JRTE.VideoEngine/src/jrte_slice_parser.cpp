#include "jrte_slice_parser.h"
#include "jrte_bit_reader.h"

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>

namespace jrte::h264
{
    namespace
    {
        bool read_flag(BitReader& bits, bool& value)
        {
            uint32_t bit = 0;
            if (!bits.read_bit(bit))
                return false;

            value = bit != 0;
            return true;
        }

        bool read_ref_pic_list_modification(
            BitReader& bits,
            bool& modification_flag)
        {
            if (!read_flag(bits, modification_flag))
                return false;

            if (!modification_flag)
                return true;

            for (uint32_t i = 0; i < 64; ++i)
            {
                uint32_t idc = 0;

                if (!bits.read_ue(idc))
                    return false;

                if (idc == 3)
                    return true;

                if (idc > 2)
                    return false;

                uint32_t value = 0;

                if (!bits.read_ue(value))
                    return false;
            }

            return false;
        }

        bool read_pred_weight_table(
            BitReader& bits,
            const SpsInfo& sps,
            const PpsInfo& pps,
            uint32_t slice_type,
            uint32_t l0_count,
            uint32_t l1_count)
        {
            uint32_t luma_denom = 0;

            if (!bits.read_ue(luma_denom) || luma_denom > 7)
                return false;

            const uint32_t chroma_array_type =
                sps.separate_colour_plane ? 0 : sps.chroma_format_idc;

            if (chroma_array_type != 0)
            {
                uint32_t chroma_denom = 0;

                if (!bits.read_ue(chroma_denom) || chroma_denom > 7)
                    return false;
            }

            auto read_list = [&](uint32_t count) -> bool
            {
                if (count > 32)
                    return false;

                for (uint32_t i = 0; i < count; ++i)
                {
                    bool flag = false;

                    if (!read_flag(bits, flag))
                        return false;

                    if (flag)
                    {
                        int32_t weight = 0;
                        int32_t offset = 0;

                        if (!bits.read_se(weight) ||
                            !bits.read_se(offset))
                            return false;
                    }

                    if (chroma_array_type != 0)
                    {
                        if (!read_flag(bits, flag))
                            return false;

                        if (flag)
                        {
                            for (uint32_t j = 0; j < 2; ++j)
                            {
                                int32_t weight = 0;
                                int32_t offset = 0;

                                if (!bits.read_se(weight) ||
                                    !bits.read_se(offset))
                                    return false;
                            }
                        }
                    }
                }

                return true;
            };

            if (!read_list(l0_count))
                return false;

            if (slice_type == 1)
            {
                if (!read_list(l1_count))
                    return false;
            }

            (void)pps;
            return true;
        }

        bool read_dec_ref_pic_marking(
            BitReader& bits,
            SliceInfo& result)
        {
            if (result.nal_type == 5)
            {
                if (!read_flag(bits, result.no_output_of_prior_pics_flag) ||
                    !read_flag(bits, result.long_term_reference_flag))
                    return false;

                return true;
            }

            if (!read_flag(
                    bits,
                    result.adaptive_ref_pic_marking_mode_flag))
                return false;

            if (!result.adaptive_ref_pic_marking_mode_flag)
                return true;

            for (uint32_t i = 0; i < 64; ++i)
            {
                uint32_t operation = 0;

                if (!bits.read_ue(operation))
                    return false;

                if (operation == 0)
                    return true;

                if (operation > 6)
                    return false;

                uint32_t value = 0;

                if (operation == 1 || operation == 3)
                {
                    if (!bits.read_ue(value))
                        return false;
                }

                if (operation == 2)
                {
                    if (!bits.read_ue(value))
                        return false;
                }

                if (operation == 3 || operation == 6)
                {
                    if (!bits.read_ue(value))
                        return false;
                }

                if (operation == 4)
                {
                    if (!bits.read_ue(value))
                        return false;
                }
            }

            return false;
        }

        bool prepare_slice(
            const uint8_t* nal,
            uint32_t nal_size,
            SliceInfo& result,
            std::vector<uint8_t>& rbsp)
        {
            if (nal == nullptr || nal_size < 2)
            {
                result.error = "Unite NAL Slice invalide ou trop courte.";
                return false;
            }

            if ((nal[0] & 0x80) != 0)
            {
                result.error = "forbidden_zero_bit non nul.";
                return false;
            }

            result.nal_ref_idc = (nal[0] >> 5) & 0x03;
            result.nal_type = nal[0] & 0x1F;

            if (result.nal_type != 1 && result.nal_type != 5)
            {
                result.error = "Type NAL non pris en charge pour un Slice.";
                return false;
            }

            std::string conversion_error;

            if (!ebsp_to_rbsp(
                    nal + 1,
                    nal_size - 1,
                    rbsp,
                    conversion_error))
            {
                result.error = conversion_error;
                return false;
            }

            if (rbsp.empty())
            {
                result.error = "Donnees RBSP Slice absentes.";
                return false;
            }

            return true;
        }

        bool read_slice_identity(
            BitReader& bits,
            SliceInfo& result)
        {
            if (!bits.read_ue(result.first_mb_in_slice) ||
                !bits.read_ue(result.slice_type) ||
                !bits.read_ue(result.pic_parameter_set_id))
            {
                result.error = "En-tete Slice incomplet.";
                return false;
            }

            if (result.slice_type > 9)
            {
                result.error = "slice_type hors limites.";
                return false;
            }

            if (result.pic_parameter_set_id > 255)
            {
                result.error = "pic_parameter_set_id hors limites.";
                return false;
            }

            static const char* names[] = {
                "P", "B", "I", "SP", "SI"
            };

            result.slice_type_name =
                names[result.slice_type % 5];

            return true;
        }

        bool read_slice_group_change_cycle(
            BitReader& bits,
            const SpsInfo& sps,
            const PpsInfo& pps,
            SliceInfo& result)
        {
            if (pps.num_slice_groups <= 1 ||
                pps.slice_group_map_type < 3 ||
                pps.slice_group_map_type > 5)
                return true;

            if (pps.slice_group_change_rate == 0)
                return false;

            const uint64_t picture_size =
                static_cast<uint64_t>(sps.width_in_mbs) *
                sps.height_in_map_units;

            if (picture_size == 0)
                return false;

            const uint64_t maximum =
                (picture_size + pps.slice_group_change_rate - 1) /
                pps.slice_group_change_rate;

            uint32_t bit_count = 0;
            uint64_t value = maximum;

            while (value != 0)
            {
                ++bit_count;
                value >>= 1;
            }

            if (bit_count > 32)
                return false;

            if (bit_count != 0 &&
                !bits.read_bits(bit_count, result.slice_group_change_cycle))
                return false;

            return true;
        }
    }

    SliceInfo parse_slice_header(
        const uint8_t* nal,
        uint32_t nal_size)
    {
        SliceInfo result;
        std::vector<uint8_t> rbsp;

        if (!prepare_slice(nal, nal_size, result, rbsp))
            return result;

        BitReader bits(rbsp.data(), rbsp.size());

        if (!read_slice_identity(bits, result))
            return result;

        result.valid = true;
        return result;
    }

    SliceInfo parse_slice_header(
        const uint8_t* nal,
        uint32_t nal_size,
        const SpsInfo& sps,
        const PpsInfo& pps)
    {
        SliceInfo result;
        std::vector<uint8_t> rbsp;

        if (!prepare_slice(nal, nal_size, result, rbsp))
            return result;

        if (!sps.valid || !pps.valid)
        {
            result.error = "SPS ou PPS invalide.";
            return result;
        }

        BitReader bits(rbsp.data(), rbsp.size());

        if (!read_slice_identity(bits, result))
            return result;

        if (result.pic_parameter_set_id != pps.pps_id ||
            pps.sps_id != sps.sps_id)
        {
            result.error = "Association SPS/PPS invalide.";
            return result;
        }

        if (sps.log2_max_frame_num < 4 ||
            sps.log2_max_frame_num > 16 ||
            sps.pic_order_cnt_type > 2)
        {
            result.error = "Parametres SPS invalides.";
            return result;
        }

        if (sps.pic_order_cnt_type == 0 &&
            (sps.log2_max_pic_order_cnt_lsb < 4 ||
             sps.log2_max_pic_order_cnt_lsb > 16))
        {
            result.error = "log2_max_pic_order_cnt_lsb hors limites.";
            return result;
        }

        if (sps.separate_colour_plane)
        {
            if (!bits.read_bits(2, result.colour_plane_id) ||
                result.colour_plane_id > 2)
            {
                result.error = "colour_plane_id invalide.";
                return result;
            }
        }

        if (!bits.read_bits(
                sps.log2_max_frame_num,
                result.frame_num))
        {
            result.error = "frame_num absent.";
            return result;
        }

        if (!sps.frame_mbs_only)
        {
            if (!read_flag(bits, result.field_pic_flag))
            {
                result.error = "field_pic_flag absent.";
                return result;
            }

            if (result.field_pic_flag &&
                !read_flag(bits, result.bottom_field_flag))
            {
                result.error = "bottom_field_flag absent.";
                return result;
            }
        }

        if (result.nal_type == 5)
        {
            if (!bits.read_ue(result.idr_pic_id) ||
                result.idr_pic_id > 65535)
            {
                result.error = "idr_pic_id invalide.";
                return result;
            }
        }

        if (sps.pic_order_cnt_type == 0)
        {
            if (!bits.read_bits(
                    sps.log2_max_pic_order_cnt_lsb,
                    result.pic_order_cnt_lsb))
            {
                result.error = "pic_order_cnt_lsb absent.";
                return result;
            }

            if (pps.bottom_field_pic_order_in_frame_present &&
                !result.field_pic_flag &&
                !bits.read_se(result.delta_pic_order_cnt_bottom))
            {
                result.error = "delta_pic_order_cnt_bottom absent.";
                return result;
            }
        }
        else if (sps.pic_order_cnt_type == 1 &&
                 !sps.delta_pic_order_always_zero)
        {
            if (!bits.read_se(result.delta_pic_order_cnt0))
            {
                result.error = "delta_pic_order_cnt[0] absent.";
                return result;
            }

            if (pps.bottom_field_pic_order_in_frame_present &&
                !result.field_pic_flag &&
                !bits.read_se(result.delta_pic_order_cnt1))
            {
                result.error = "delta_pic_order_cnt[1] absent.";
                return result;
            }
        }

        if (pps.redundant_pic_cnt_present)
        {
            if (!bits.read_ue(result.redundant_pic_cnt) ||
                result.redundant_pic_cnt > 127)
            {
                result.error = "redundant_pic_cnt invalide.";
                return result;
            }
        }

        const uint32_t normalized_type = result.slice_type % 5;
        const bool is_p = normalized_type == 0;
        const bool is_b = normalized_type == 1;
        const bool is_i = normalized_type == 2;
        const bool is_sp = normalized_type == 3;
        const bool is_si = normalized_type == 4;

        if (is_b &&
            !read_flag(bits, result.direct_spatial_mv_pred_flag))
        {
            result.error = "direct_spatial_mv_pred_flag absent.";
            return result;
        }

        result.effective_num_ref_idx_l0 =
            pps.num_ref_idx_l0_default_active;

        result.effective_num_ref_idx_l1 =
            pps.num_ref_idx_l1_default_active;

        if (is_p || is_sp || is_b)
        {
            if (!read_flag(
                    bits,
                    result.num_ref_idx_active_override_flag))
            {
                result.error = "num_ref_idx_active_override_flag absent.";
                return result;
            }

            if (result.num_ref_idx_active_override_flag)
            {
                if (!bits.read_ue(result.num_ref_idx_l0_active_minus1) ||
                    result.num_ref_idx_l0_active_minus1 > 31)
                {
                    result.error = "Nombre de references L0 invalide.";
                    return result;
                }

                result.effective_num_ref_idx_l0 =
                    result.num_ref_idx_l0_active_minus1 + 1;

                if (is_b)
                {
                    if (!bits.read_ue(
                            result.num_ref_idx_l1_active_minus1) ||
                        result.num_ref_idx_l1_active_minus1 > 31)
                    {
                        result.error = "Nombre de references L1 invalide.";
                        return result;
                    }

                    result.effective_num_ref_idx_l1 =
                        result.num_ref_idx_l1_active_minus1 + 1;
                }
            }
        }

        if (!is_i && !is_si)
        {
            if (!read_ref_pic_list_modification(
                    bits,
                    result.ref_pic_list_modification_flag_l0))
            {
                result.error = "Modification de liste L0 invalide.";
                return result;
            }

            if (is_b &&
                !read_ref_pic_list_modification(
                    bits,
                    result.ref_pic_list_modification_flag_l1))
            {
                result.error = "Modification de liste L1 invalide.";
                return result;
            }
        }

        const bool weighted =
            (pps.weighted_pred && (is_p || is_sp)) ||
            (pps.weighted_bipred_idc == 1 && is_b);

        if (weighted)
        {
            if (!read_pred_weight_table(
                    bits,
                    sps,
                    pps,
                    normalized_type,
                    result.effective_num_ref_idx_l0,
                    result.effective_num_ref_idx_l1))
            {
                result.error = "Table de prediction ponderee invalide.";
                return result;
            }
        }

        if (result.nal_ref_idc != 0)
        {
            if (!read_dec_ref_pic_marking(bits, result))
            {
                result.error = "Marquage des images de reference invalide.";
                return result;
            }
        }

        if (pps.entropy_coding_mode && !is_i && !is_si)
        {
            if (!bits.read_ue(result.cabac_init_idc) ||
                result.cabac_init_idc > 2)
            {
                result.error = "cabac_init_idc invalide.";
                return result;
            }
        }

        if (!bits.read_se(result.slice_qp_delta))
        {
            result.error = "slice_qp_delta absent.";
            return result;
        }

        if (is_sp || is_si)
        {
            if (is_sp &&
                !read_flag(bits, result.sp_for_switch_flag))
            {
                result.error = "sp_for_switch_flag absent.";
                return result;
            }

            if (!bits.read_se(result.slice_qs_delta))
            {
                result.error = "slice_qs_delta absent.";
                return result;
            }
        }

        if (pps.deblocking_filter_control_present)
        {
            if (!bits.read_ue(result.disable_deblocking_filter_idc) ||
                result.disable_deblocking_filter_idc > 2)
            {
                result.error = "disable_deblocking_filter_idc invalide.";
                return result;
            }

            if (result.disable_deblocking_filter_idc != 1)
            {
                if (!bits.read_se(result.slice_alpha_c0_offset_div2) ||
                    !bits.read_se(result.slice_beta_offset_div2))
                {
                    result.error = "Offsets de deblocage absents.";
                    return result;
                }
            }
        }

        if (!read_slice_group_change_cycle(
                bits, sps, pps, result))
        {
            result.error = "slice_group_change_cycle invalide.";
            return result;
        }

        if (pps.entropy_coding_mode)
        {
            size_t current =
                rbsp.size() * 8 - bits.bits_remaining();

            // En CABAC, chaque bit d'alignement doit valoir 1.
            while (current % 8 != 0)
            {
                uint32_t alignment_bit = 0;

                if (!bits.read_bit(alignment_bit) ||
                    alignment_bit != 1)
                {
                    result.error = "cabac_alignment_one_bit invalide.";
                    return result;
                }

                current =
                    rbsp.size() * 8 - bits.bits_remaining();
            }

            result.cabac_alignment_valid = true;
            result.cabac_data_bit_offset = current;
            result.cabac_data_byte_offset = current / 8;
        }

        result.core_header_complete = true;
        result.valid = true;

        return result;
    }
}

