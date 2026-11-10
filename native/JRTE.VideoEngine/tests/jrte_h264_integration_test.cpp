#include "jrte_h264_analyzer.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    // SPS reel extrait de la video de test JRTE.
    const uint8_t sps[] = {
        0x27, 0x4D, 0x00, 0x29, 0x95, 0xB0, 0x14,
        0x41, 0x7E, 0xC0, 0x44, 0x00, 0x00, 0x0F,
        0xA4, 0x00, 0x03, 0xA9, 0x83, 0x81, 0x00,
        0x00, 0x4A, 0x5B, 0x00, 0x00, 0x12, 0x95,
        0xD6, 0xF7, 0xBE, 0x0E, 0xD0, 0xE1, 0x97
    };

    const uint32_t sps_size = static_cast<uint32_t>(sizeof(sps));

    // Echantillon AVCC : longueur NAL sur 4 octets + SPS.
    std::vector<uint8_t> sample = {
        static_cast<uint8_t>((sps_size >> 24) & 0xFF),
        static_cast<uint8_t>((sps_size >> 16) & 0xFF),
        static_cast<uint8_t>((sps_size >> 8) & 0xFF),
        static_cast<uint8_t>(sps_size & 0xFF)
    };

    sample.insert(sample.end(), std::begin(sps), std::end(sps));

    const auto result = jrte::h264::analyze_sample(
        sample.data(),
        static_cast<uint32_t>(sample.size()),
        4
    );

    std::cout << "===== JRTE H264 INTEGRATION TEST =====\n";

    if (!result.success)
    {
        std::cerr << "ECHEC ANALYSE : " << result.error << "\n";
        return 1;
    }

    std::cout << "Analyse reussie : oui\n";
    std::cout << "Unites NAL : " << result.total_nal_units << "\n";
    std::cout << "SPS detectes : " << result.sps_count << "\n";
    std::cout << "SPS valides : " << result.valid_sps_count << "\n";

    if (result.sps_info.empty())
    {
        std::cerr << "ECHEC : aucune information SPS.\n";
        return 2;
    }

    const auto& info = result.sps_info.front();

    if (!info.valid)
    {
        std::cerr << "ECHEC PARSEUR : " << info.error << "\n";
        return 3;
    }

    std::cout << "Profil : " << info.profile_idc << "\n";
    std::cout << "Niveau : " << info.level_idc << "\n";
    std::cout << "Chroma : " << info.chroma_format_idc << "\n";
    std::cout << "Resolution : "
              << info.width << " x " << info.height << "\n";

    if (result.total_nal_units != 1 ||
        result.sps_count != 1 ||
        result.valid_sps_count != 1 ||
        info.profile_idc != 77 ||
        info.level_idc != 41 ||
        info.width != 1296 ||
        info.height != 752)
    {
        std::cerr << "ECHEC : valeurs inattendues.\n";
        return 4;
    }

    std::cout << "\nSUCCES : analyseur et parseur SPS integres.\n";
    return 0;
}
