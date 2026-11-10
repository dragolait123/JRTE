#include "jrte_pps_parser.h"
#include "jrte_bit_reader.h"

#include <vector>
#include <cstdint>

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

    static uint32_t ceil_log2(uint32_t value)
    {
        uint32_t result = 0;
        uint32_t current = 1;

        while (current < value)
        {
            current <<= 1;
            ++result;
        }

        return result;
    }

    PpsInfo parse_pps(const uint8_t* nal, uint32_t nal_size)
    {
        PpsInfo result;

        auto fail = [&](const std::string& message) -> PpsInfo
        {
            result.valid = false;
            result.error = message;
            return result;
        };

        if (nal == nullptr || nal_size < 2)
            return fail("Unite PPS absente ou trop courte.");

        if ((nal[0] & 0x80) != 0 || (nal[0] & 0x1F) != 8)
            return fail("L'en-tete NAL n'est pas un PPS valide.");

        std::vector<uint8_t> rbsp;
        std::string conversion_error;

        if (!ebsp_to_rbsp(
            nal + 1,
            nal_size - 1,
            rbsp,
            conversion_error))
        {
            return fail("Conversion EBSP vers RBSP impossible : " +
                        conversion_error);
        }

        if (rbsp.empty())
            return fail("RBSP PPS vide.");

        BitReader bits(rbsp.data(), rbsp.size());

        uint32_t pps_id = 0;
        uint32_t sps_id = 0;

        if (!bits.read_ue(pps_id) || !bits.read_ue(sps_id))
            return fail("Identifiants PPS/SPS incomplets.");

        if (pps_id > 255 || sps_id > 31)
            return fail("Identifiant PPS ou SPS hors limites.");

        result.pps_id = pps_id;
        result.sps_id = sps_id;

        if (!read_flag(bits, result.entropy_coding_mode) ||
            !read_flag(bits, result.bottom_field_pic_order_in_frame_present))
        {
            return fail("Drapeaux initiaux PPS incomplets.");
        }

        uint32_t slice_groups_minus1 = 0;

        if (!bits.read_ue(slice_groups_minus1) ||
            slice_groups_minus1 > 7)
        {
            return fail("Nombre de groupes de tranches invalide.");
        }

        result.num_slice_groups = slice_groups_minus1 + 1;

        if (slice_groups_minus1 > 0)
        {
            uint32_t map_type = 0;

            if (!bits.read_ue(map_type) || map_type > 6)
                return fail("Type de carte FMO invalide.");

            result.slice_group_map_type = map_type;

            if (map_type == 0)
            {
                for (uint32_t i = 0; i <= slice_groups_minus1; ++i)
                {
                    uint32_t run_length_minus1 = 0;
                    if (!bits.read_ue(run_length_minus1))
                        return fail("Carte FMO de type 0 incomplete.");
                }
            }
            else if (map_type == 2)
            {
                for (uint32_t i = 0; i < slice_groups_minus1; ++i)
                {
                    uint32_t top_left = 0;
                    uint32_t bottom_right = 0;

                    if (!bits.read_ue(top_left) ||
                        !bits.read_ue(bottom_right) ||
                        bottom_right < top_left)
                    {
                        return fail("Carte FMO de type 2 invalide.");
                    }
                }
            }
            else if (map_type >= 3 && map_type <= 5)
            {
                bool direction = false;
                uint32_t change_rate_minus1 = 0;

                if (!read_flag(bits, direction) ||
                    !bits.read_ue(change_rate_minus1))
                {
                    return fail("Carte FMO de type 3, 4 ou 5 incomplete.");
                }

                if (change_rate_minus1 >= 1000000)
                    return fail("Taux de changement FMO hors limites.");

                result.slice_group_change_direction = direction;
                result.slice_group_change_rate = change_rate_minus1 + 1;
            }
            else if (map_type == 6)
            {
                uint32_t pic_size_minus1 = 0;

                if (!bits.read_ue(pic_size_minus1) ||
                    pic_size_minus1 > 1000000)
                {
                    return fail("Taille de carte FMO hors limites.");
                }

                const uint32_t map_bits =
                    ceil_log2(result.num_slice_groups);

                for (uint32_t i = 0; i <= pic_size_minus1; ++i)
                {
                    uint32_t group_id = 0;

                    if (map_bits > 0 &&
                        !bits.read_bits(map_bits, group_id))
                    {
                        return fail("Carte FMO de type 6 incomplete.");
                    }

                    if (group_id >= result.num_slice_groups)
                        return fail("Identifiant de groupe FMO invalide.");
                }
            }
            else if (map_type == 1)
            {
                // Carte dispersee : aucun champ supplementaire.
            }
        }

        uint32_t l0_minus1 = 0;
        uint32_t l1_minus1 = 0;

        if (!bits.read_ue(l0_minus1) ||
            !bits.read_ue(l1_minus1) ||
            l0_minus1 > 31 ||
            l1_minus1 > 31)
        {
            return fail("Parametres de references PPS invalides.");
        }

        result.num_ref_idx_l0_default_active = l0_minus1 + 1;
        result.num_ref_idx_l1_default_active = l1_minus1 + 1;

        if (!read_flag(bits, result.weighted_pred))
            return fail("weighted_pred_flag absent.");

        if (!bits.read_bits(2, result.weighted_bipred_idc) ||
            result.weighted_bipred_idc > 2)
        {
            return fail("weighted_bipred_idc invalide.");
        }

        if (!bits.read_se(result.pic_init_qp_minus26) ||
            !bits.read_se(result.pic_init_qs_minus26) ||
            !bits.read_se(result.chroma_qp_index_offset))
        {
            return fail("Parametres de quantification PPS incomplets.");
        }

        if (result.pic_init_qp_minus26 < -26 ||
            result.pic_init_qp_minus26 > 25 ||
            result.pic_init_qs_minus26 < -26 ||
            result.pic_init_qs_minus26 > 25 ||
            result.chroma_qp_index_offset < -12 ||
            result.chroma_qp_index_offset > 12)
        {
            return fail("Valeur de quantification PPS hors limites.");
        }

        if (!read_flag(bits, result.deblocking_filter_control_present) ||
            !read_flag(bits, result.constrained_intra_pred) ||
            !read_flag(bits, result.redundant_pic_cnt_present))
        {
            return fail("Drapeaux finaux PPS incomplets.");
        }

        result.valid = true;
        return result;
    }
}

