#include "jrte_pps_parser.h"

#include <iostream>
#include <cstdint>

int main()
{
    const uint8_t pps[] = {
        0x28, 0xEE, 0x3C, 0x80
    };

    const auto info = jrte::h264::parse_pps(
        pps,
        static_cast<uint32_t>(sizeof(pps))
    );

    std::cout << "===== JRTE PPS PARSER TEST =====\n";

    if (!info.valid)
    {
        std::cerr << "ECHEC : " << info.error << "\n";
        return 1;
    }

    std::cout << "Statut : VALIDE\n";
    std::cout << "Identifiant PPS : " << info.pps_id << "\n";
    std::cout << "Identifiant SPS associe : " << info.sps_id << "\n";
    std::cout << "Entropy coding : " << info.entropy_coding_mode << "\n";
    std::cout << "Groupes de tranches : " << info.num_slice_groups << "\n";
    std::cout << "References L0 : " << info.num_ref_idx_l0_default_active << "\n";
    std::cout << "References L1 : " << info.num_ref_idx_l1_default_active << "\n";
    std::cout << "Weighted prediction : " << info.weighted_pred << "\n";
    std::cout << "Weighted bipred : " << info.weighted_bipred_idc << "\n";
    std::cout << "QP initial : " << info.pic_init_qp_minus26 << "\n";
    std::cout << "QS initial : " << info.pic_init_qs_minus26 << "\n";
    std::cout << "Chroma QP offset : " << info.chroma_qp_index_offset << "\n";
    std::cout << "Deblocking control : " << info.deblocking_filter_control_present << "\n";
    std::cout << "Constrained intra : " << info.constrained_intra_pred << "\n";
    std::cout << "Redundant picture count : " << info.redundant_pic_cnt_present << "\n";

    return 0;
}
