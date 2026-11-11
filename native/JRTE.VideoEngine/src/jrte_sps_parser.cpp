#include "jrte_sps_parser.h"
#include "jrte_bit_reader.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace jrte::h264
{
    static bool read_flag(BitReader& bits, bool& value)
    {
        uint32_t bit = 0;
        if (!bits.read_bit(bit))
            return false;

        value = bit != 0;
        return true;
    }

    static bool skip_scaling_list(BitReader& bits, uint32_t size)
    {
        int32_t last_scale = 8;
        int32_t next_scale = 8;

        for (uint32_t i = 0; i < size; ++i)
        {
            if (next_scale != 0)
            {
                int32_t delta_scale = 0;
                if (!bits.read_se(delta_scale))
                    return false;

                next_scale = (last_scale + delta_scale + 256) % 256;
            }

            if (next_scale != 0)
                last_scale = next_scale;
        }

        return true;
    }

    static bool is_high_profile(uint32_t profile)
    {
        switch (profile)
        {
            case 44: case 83: case 86: case 100:
            case 110: case 118: case 122: case 128:
            case 134: case 135: case 138: case 139:
            case 144: case 244:
                return true;
            default:
                return false;
        }
    }


    bool skip_hrd_parameters(BitReader& bits)
    {
        uint32_t cpb_count_minus1 = 0;
        uint32_t value = 0;

        if (!bits.read_ue(cpb_count_minus1) || cpb_count_minus1 > 31)
            return false;

        if (!bits.read_bits(4, value) ||
            !bits.read_bits(4, value))
            return false;

        for (uint32_t i = 0; i <= cpb_count_minus1; ++i)
        {
            if (!bits.read_ue(value) ||
                !bits.read_ue(value) ||
                !bits.read_bit(value))
                return false;
        }

        return bits.read_bits(5, value) &&
               bits.read_bits(5, value) &&
               bits.read_bits(5, value) &&
               bits.read_bits(5, value);
    }

    bool skip_vui_parameters(BitReader& bits)
    {
        uint32_t value = 0;
        uint32_t flag = 0;

        // aspect_ratio_info_present_flag
        if (!bits.read_bit(flag))
            return false;

        if (flag)
        {
            if (!bits.read_bits(8, value))
                return false;

            if (value == 255)
            {
                uint32_t sar_width = 0;
                uint32_t sar_height = 0;

                if (!bits.read_bits(16, sar_width) ||
                    !bits.read_bits(16, sar_height) ||
                    sar_width == 0 || sar_height == 0)
                    return false;
            }
        }

        // overscan_info_present_flag
        if (!bits.read_bit(flag))
            return false;

        if (flag && !bits.read_bit(value))
            return false;

        // video_signal_type_present_flag
        if (!bits.read_bit(flag))
            return false;

        if (flag)
        {
            if (!bits.read_bits(3, value) ||
                !bits.read_bit(value) ||
                !bits.read_bit(flag))
                return false;

            if (flag)
            {
                if (!bits.read_bits(8, value) ||
                    !bits.read_bits(8, value) ||
                    !bits.read_bits(8, value))
                    return false;
            }
        }

        // chroma_loc_info_present_flag
        if (!bits.read_bit(flag))
            return false;

        if (flag)
        {
            if (!bits.read_ue(value) || !bits.read_ue(value))
                return false;
        }

        // timing_info_present_flag
        if (!bits.read_bit(flag))
            return false;

        if (flag)
        {
            if (!bits.read_bits(32, value) ||
                !bits.read_bits(32, value) ||
                !bits.read_bit(value))
                return false;
        }

        // nal_hrd_parameters_present_flag
        uint32_t nal_hrd = 0;
        uint32_t vcl_hrd = 0;

        if (!bits.read_bit(nal_hrd))
            return false;

        if (nal_hrd && !skip_hrd_parameters(bits))
            return false;

        // vcl_hrd_parameters_present_flag
        if (!bits.read_bit(vcl_hrd))
            return false;

        if (vcl_hrd && !skip_hrd_parameters(bits))
            return false;

        // low_delay_hrd_flag
        if ((nal_hrd || vcl_hrd) && !bits.read_bit(value))
            return false;

        // pic_struct_present_flag
        if (!bits.read_bit(value))
            return false;

        // bitstream_restriction_flag
        if (!bits.read_bit(flag))
            return false;

        if (flag)
        {
            if (!bits.read_bit(value))
                return false;

            for (uint32_t i = 0; i < 6; ++i)
            {
                if (!bits.read_ue(value))
                    return false;
            }
        }

        return bits.good();
    }

    bool read_rbsp_trailing_bits(BitReader& bits)
    {
        uint32_t value = 0;

        // rbsp_stop_one_bit doit être égal à 1.
        if (!bits.read_bit(value) || value != 1)
            return false;

        // Les bits d'alignement doivent être égaux à 0.
        while ((bits.bits_remaining() % 8) != 0)
        {
            if (!bits.read_bit(value) || value != 0)
                return false;
        }

        // Aucun octet supplémentaire ne doit rester.
        return bits.bits_remaining() == 0 && bits.good();
    }

    SpsInfo parse_sps(const uint8_t* nal, uint32_t nal_size)
    {
        SpsInfo result;

        auto fail = [&](const std::string& message) -> SpsInfo
        {
            result.valid = false;
            result.error = message;
            return result;
        };

        if (nal == nullptr || nal_size < 4)
            return fail("Unite SPS absente ou trop courte.");

        if ((nal[0] & 0x80) != 0 || (nal[0] & 0x1F) != 7)
            return fail("L'en-tete NAL n'est pas un SPS valide.");

        std::vector<uint8_t> rbsp;
        std::string conversion_error;

        if (!ebsp_to_rbsp(nal + 1, nal_size - 1, rbsp, conversion_error))
            return fail("Conversion EBSP vers RBSP impossible : " + conversion_error);

        if (rbsp.size() < 3)
            return fail("RBSP SPS trop court.");

        BitReader bits(rbsp.data(), rbsp.size());

        uint32_t profile = 0;
        uint32_t constraints = 0;
        uint32_t level = 0;
        uint32_t sps_id = 0;

        if (!bits.read_bits(8, profile) ||
            !bits.read_bits(8, constraints) ||
            !bits.read_bits(8, level) ||
            !bits.read_ue(sps_id))
            return fail("En-tete SPS incomplet.");

        if ((constraints & 0x03) != 0)
            return fail("Bits reserves SPS non nuls.");

        if (sps_id > 31)
            return fail("Identifiant SPS hors limites.");

        result.profile_idc = profile;
        result.level_idc = level;
        result.sps_id = sps_id;

        uint32_t chroma_format = 1;
        uint32_t bit_depth_luma_minus8 = 0;
        uint32_t bit_depth_chroma_minus8 = 0;
        bool separate_colour_plane = false;

        if (is_high_profile(profile))
        {
            if (!bits.read_ue(chroma_format) || chroma_format > 3)
                return fail("Format chromatique SPS invalide.");

            if (chroma_format == 3 &&
                !read_flag(bits, separate_colour_plane))
                return fail("Champ separate_colour_plane_flag absent.");

            if (!bits.read_ue(bit_depth_luma_minus8) ||
                !bits.read_ue(bit_depth_chroma_minus8))
                return fail("Profondeur de couleur SPS incomplete.");

            if (bit_depth_luma_minus8 > 6 || bit_depth_chroma_minus8 > 6)
                return fail("Profondeur de couleur non supportee.");

            bool qpprime_y_zero_transform_bypass = false;
            bool scaling_matrix_present = false;

            if (!read_flag(bits, qpprime_y_zero_transform_bypass) ||
                !read_flag(bits, scaling_matrix_present))
                return fail("Parametres de matrice SPS incomplets.");

            if (scaling_matrix_present)
            {
                const uint32_t list_count =
                    chroma_format == 3 ? 12 : 8;

                for (uint32_t i = 0; i < list_count; ++i)
                {
                    bool list_present = false;
                    if (!read_flag(bits, list_present))
                        return fail("Indicateur de matrice SPS absent.");

                    if (list_present)
                    {
                        const uint32_t list_size = i < 6 ? 16 : 64;
                        if (!skip_scaling_list(bits, list_size))
                            return fail("Matrice de quantification SPS invalide.");
                    }
                }
            }
        }

        result.separate_colour_plane = separate_colour_plane;
        result.chroma_format_idc = chroma_format;
        result.bit_depth_luma = 8 + bit_depth_luma_minus8;
        result.bit_depth_chroma = 8 + bit_depth_chroma_minus8;

        uint32_t log2_max_frame_num_minus4 = 0;
        uint32_t pic_order_cnt_type = 0;
        uint32_t log2_max_pic_order_cnt_lsb_minus4 = 0;

        bool delta_pic_order_always_zero = false;
        int32_t offset_for_non_ref_pic = 0;
        int32_t offset_for_top_to_bottom_field = 0;
        std::vector<int32_t> offset_for_ref_frame;

        if (!bits.read_ue(log2_max_frame_num_minus4) ||
            log2_max_frame_num_minus4 > 12)
            return fail("Parametre log2_max_frame_num_minus4 invalide.");

        if (!bits.read_ue(pic_order_cnt_type) || pic_order_cnt_type > 2)
            return fail("Type pic_order_cnt_type invalide.");

        if (pic_order_cnt_type == 0)
        {
            if (!bits.read_ue(log2_max_pic_order_cnt_lsb_minus4) ||
                log2_max_pic_order_cnt_lsb_minus4 > 12)
                return fail("Parametre POC invalide.");
        }
        else if (pic_order_cnt_type == 1)
        {
            uint32_t cycle_count = 0;

            if (!read_flag(bits, delta_pic_order_always_zero) ||
                !bits.read_se(offset_for_non_ref_pic) ||
                !bits.read_se(offset_for_top_to_bottom_field) ||
                !bits.read_ue(cycle_count) ||
                cycle_count > 255)
                return fail("Parametres POC de type 1 invalides.");

            offset_for_ref_frame.reserve(cycle_count);

            for (uint32_t i = 0; i < cycle_count; ++i)
            {
                int32_t offset = 0;

                if (!bits.read_se(offset))
                    return fail("Cycle POC incomplet.");

                offset_for_ref_frame.push_back(offset);
            }
        }

        result.log2_max_frame_num =
            log2_max_frame_num_minus4 + 4;

        result.pic_order_cnt_type = pic_order_cnt_type;

        if (pic_order_cnt_type == 0)
            result.log2_max_pic_order_cnt_lsb =
                log2_max_pic_order_cnt_lsb_minus4 + 4;

        result.delta_pic_order_always_zero =
            delta_pic_order_always_zero;

        result.offset_for_non_ref_pic = offset_for_non_ref_pic;
        result.offset_for_top_to_bottom_field =
            offset_for_top_to_bottom_field;

        result.offset_for_ref_frame = offset_for_ref_frame;
        uint32_t max_num_ref_frames = 0;
        bool gaps_in_frame_num = false;

        if (!bits.read_ue(max_num_ref_frames) ||
            !read_flag(bits, gaps_in_frame_num))
            return fail("Parametres de reference SPS incomplets.");

        if (max_num_ref_frames > 16)
            return fail("Nombre maximal de references SPS hors limites.");

        result.max_num_ref_frames = max_num_ref_frames;
        result.gaps_in_frame_num_allowed = gaps_in_frame_num;

        uint32_t width_mbs_minus1 = 0;
        uint32_t height_map_units_minus1 = 0;

        if (!bits.read_ue(width_mbs_minus1) ||
            !bits.read_ue(height_map_units_minus1))
            return fail("Dimensions SPS absentes.");

        if (width_mbs_minus1 > 65535 || height_map_units_minus1 > 65535)
            return fail("Dimensions SPS hors limites.");

        bool frame_mbs_only = false;

        if (!read_flag(bits, frame_mbs_only))
            return fail("Champ frame_mbs_only_flag absent.");

        result.frame_mbs_only = frame_mbs_only;

        if (!frame_mbs_only)
        {
            if (!read_flag(bits, result.mb_adaptive_frame_field))
                return fail("Champ mb_adaptive_frame_field_flag absent.");
        }

        if (!read_flag(bits, result.direct_8x8_inference))
            return fail("Champ direct_8x8_inference_flag absent.");

        bool cropping = false;
        uint32_t crop_left = 0;
        uint32_t crop_right = 0;
        uint32_t crop_top = 0;
        uint32_t crop_bottom = 0;

        if (!read_flag(bits, cropping))
            return fail("Champ frame_cropping_flag absent.");

        if (cropping)
        {
            if (!bits.read_ue(crop_left) ||
                !bits.read_ue(crop_right) ||
                !bits.read_ue(crop_top) ||
                !bits.read_ue(crop_bottom))
                return fail("Parametres de recadrage SPS incomplets.");
        }

        const uint32_t chroma_array_type =
            separate_colour_plane ? 0 : chroma_format;

        uint32_t sub_width_c = 1;
        uint32_t sub_height_c = 1;

        if (chroma_array_type == 1)
        {
            sub_width_c = 2;
            sub_height_c = 2;
        }
        else if (chroma_array_type == 2)
        {
            sub_width_c = 2;
            sub_height_c = 1;
        }

        const uint64_t coded_width =
            (uint64_t(width_mbs_minus1) + 1) * 16;

        const uint64_t coded_height =
            (uint64_t(height_map_units_minus1) + 1) *
            (frame_mbs_only ? 1 : 2) * 16;

        const uint64_t crop_unit_x =
            chroma_array_type == 0 ? 1 : sub_width_c;

        const uint64_t crop_unit_y =
            chroma_array_type == 0
                ? (frame_mbs_only ? 1 : 2)
                : sub_height_c * (frame_mbs_only ? 1 : 2);

        uint32_t vui_present = 0;

        if (!bits.read_bit(vui_present))
            return fail("Flag VUI SPS tronque.");

        if (vui_present && !skip_vui_parameters(bits))
            return fail("Parametres VUI SPS invalides ou tronques.");
        const uint64_t crop_x =
            uint64_t(crop_left + uint64_t(crop_right)) * crop_unit_x;

        const uint64_t crop_y =
            uint64_t(crop_top + uint64_t(crop_bottom)) * crop_unit_y;

        if (crop_x >= coded_width || crop_y >= coded_height)
            return fail("Recadrage SPS superieur aux dimensions.");

        const uint64_t final_width = coded_width - crop_x;
        const uint64_t final_height = coded_height - crop_y;

        if (final_width == 0 || final_height == 0 ||
            final_width > std::numeric_limits<uint32_t>::max() ||
            final_height > std::numeric_limits<uint32_t>::max())
            return fail("Dimensions finales SPS invalides.");

        result.width_in_mbs = width_mbs_minus1 + 1;
        result.height_in_map_units = height_map_units_minus1 + 1;
        result.width = static_cast<uint32_t>(final_width);
        result.height = static_cast<uint32_t>(final_height);
        result.frame_mbs_only = frame_mbs_only;
        result.frame_cropping = cropping;
        result.crop_left = crop_left;
        result.crop_right = crop_right;
        result.crop_top = crop_top;
        result.crop_bottom = crop_bottom;

        if (!read_rbsp_trailing_bits(bits))
            return fail("Bits de fin RBSP SPS invalides ou tronques.");

        result.valid = true;
        return result;
    }
}
