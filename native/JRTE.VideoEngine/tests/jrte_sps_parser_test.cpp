#include "jrte_sps_parser.h"

#include <iostream>
#include <cstdint>

int main()
{
    const uint8_t sps[] = {
        0x27, 0x4D, 0x00, 0x29, 0x95, 0xB0, 0x14,
        0x41, 0x7E, 0xC0, 0x44, 0x00, 0x00, 0x0F,
        0xA4, 0x00, 0x03, 0xA9, 0x83, 0x81, 0x00,
        0x00, 0x4A, 0x5B, 0x00, 0x00, 0x12, 0x95,
        0xD6, 0xF7, 0xBE, 0x0E, 0xD0, 0xE1, 0x97
    };

    const auto info = jrte::h264::parse_sps(
        sps,
        static_cast<uint32_t>(sizeof(sps))
    );

    std::cout << "===== JRTE SPS PARSER TEST =====\n";

    if (!info.valid)
    {
        std::cerr << "ECHEC : " << info.error << "\n";
        return 1;
    }

    std::cout << "Profil : " << info.profile_idc << "\n";
    std::cout << "Niveau : " << info.level_idc << "\n";
    std::cout << "Identifiant SPS : " << info.sps_id << "\n";
    std::cout << "Chroma format : " << info.chroma_format_idc << "\n";
    std::cout << "Bit depth luma : " << info.bit_depth_luma << "\n";
    std::cout << "Bit depth chroma : " << info.bit_depth_chroma << "\n";
    std::cout << "Largeur calculee : " << info.width << "\n";
    std::cout << "Hauteur calculee : " << info.height << "\n";
    std::cout << "Recadrage : " << (info.frame_cropping ? "oui" : "non") << "\n";

    if (info.profile_idc != 77 ||
        info.level_idc != 41 ||
        info.width != 1296 ||
        info.height != 752)
    {
        std::cerr << "ECHEC : les valeurs ne correspondent pas a la video.\n";
        return 2;
    }

    for (uint32_t length = 1;
         length < static_cast<uint32_t>(sizeof(sps));
         ++length)
    {
        const auto truncated = jrte::h264::parse_sps(sps, length);

        if (truncated.valid)
        {
            std::cerr
                << "ECHEC : SPS tronque accepte avec "
                << length << " octets.\n";
            return 3;
        }
    }

    std::cout << "SUCCES : SPS valide, resolution confirmee "
              << "et SPS tronques rejetes.\n";
    return 0;
}
