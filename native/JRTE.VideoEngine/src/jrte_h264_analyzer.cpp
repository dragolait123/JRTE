
#include "jrte_h264_analyzer.h"
#include "jrte_slice_parser.h"
namespace jrte::h264
{
    static std::string get_type_name(uint8_t type)
    {
        switch (type)
        {
            case 1: return "NON-IDR SLICE";
            case 5: return "IDR SLICE";
            case 6: return "SEI";
            case 7: return "SPS";
            case 8: return "PPS";
            case 9: return "ACCESS UNIT DELIMITER";
            case 10: return "END OF SEQUENCE";
            case 11: return "END OF STREAM";
            case 12: return "FILLER DATA";
            default: return "UNKNOWN";
        }
    }

    AvcConfiguration parse_avcc(
        const uint8_t* data,
        uint32_t size)
    {
        AvcConfiguration result;

        if (data == nullptr || size < 7)
        {
            result.error = "Configuration avcC trop courte.";
            return result;
        }

        if (data[0] != 1)
        {
            result.error = "Version avcC non supportee.";
            return result;
        }

        result.profile = data[1];
        result.compatibility = data[2];
        result.level = data[3];

        const uint8_t length_field = data[4];

        if ((length_field & 0xFC) != 0xFC)
        {
            result.error = "Bits reserves avcC invalides.";
            return result;
        }

        result.length_size = (length_field & 0x03) + 1;

        if (result.length_size == 3)
        {
            result.error = "Prefixe NAL de 3 octets reserve.";
            return result;
        }

        if ((data[5] & 0xE0) != 0xE0)
        {
            result.error = "Bits reserves SPS invalides.";
            return result;
        }

        uint32_t offset = 6;
        const uint32_t sps_count = data[5] & 0x1F;

        for (uint32_t i = 0; i < sps_count; ++i)
        {
            if (size - offset < 2)
            {
                result.error = "Longueur SPS incomplete.";
                return result;
            }

            const uint32_t nal_size =
                (uint32_t(data[offset]) << 8) | data[offset + 1];

            offset += 2;

            if (nal_size == 0 || nal_size > size - offset)
            {
                result.error = "SPS invalide ou tronque.";
                return result;
            }

            if ((data[offset] & 0x1F) != 7)
            {
                result.error = "Unite SPS de type incorrect.";
                return result;
            }

            result.sps_units.emplace_back(
                data + offset,
                data + offset + nal_size);

            offset += nal_size;
        }

        if (offset >= size)
        {
            result.error = "Nombre de PPS absent.";
            return result;
        }

        const uint32_t pps_count = data[offset++];

        for (uint32_t i = 0; i < pps_count; ++i)
        {
            if (size - offset < 2)
            {
                result.error = "Longueur PPS incomplete.";
                return result;
            }

            const uint32_t nal_size =
                (uint32_t(data[offset]) << 8) | data[offset + 1];

            offset += 2;

            if (nal_size == 0 || nal_size > size - offset)
            {
                result.error = "PPS invalide ou tronque.";
                return result;
            }

            if ((data[offset] & 0x1F) != 8)
            {
                result.error = "Unite PPS de type incorrect.";
                return result;
            }

            result.pps_units.emplace_back(
                data + offset,
                data + offset + nal_size);

            offset += nal_size;
        }

        result.valid = true;
        return result;
    }

    AnalysisResult analyze_sample(
        const uint8_t* data,
        uint32_t size,
        uint32_t length_size)
    {
        AnalysisResult result;

        if (data == nullptr || size == 0)
        {
            result.error = "Echantillon vide ou invalide.";
            return result;
        }

        if (length_size != 1 &&
            length_size != 2 &&
            length_size != 4)
        {
            result.error = "Taille de prefixe NAL invalide.";
            return result;
        }

        uint32_t offset = 0;

        while (offset < size)
        {
            if (size - offset < length_size)
            {
                result.error = "Prefixe NAL incomplet.";
                return result;
            }

            uint32_t nal_size = 0;

            for (uint32_t i = 0; i < length_size; ++i)
            {
                nal_size = (nal_size << 8) | data[offset + i];
            }

            offset += length_size;

            if (nal_size == 0 || nal_size > size - offset)
            {
                result.error = "Longueur NAL invalide.";
                return result;
            }

            const uint8_t header = data[offset];

            if ((header & 0x80) != 0)
            {
                result.error = "Bit forbidden_zero_bit invalide.";
                return result;
            }

            const uint8_t type = header & 0x1F;

            if (type == 0 || type >= 24)
            {
                result.error = "Type NAL invalide ou non supporte.";
                return result;
            }

            NalUnitInfo unit;

            unit.index = result.total_nal_units;
            unit.size = nal_size;
            unit.nal_type = type;
            unit.nal_ref_idc = (header >> 5) & 0x03;
            unit.type_name = get_type_name(type);
// Analyse des champs initiaux du slice header H.264.
if (type == 1 || type == 5)
{
    const SliceInfo info = parse_slice_header(
        data + offset,
        nal_size
    );

    if (info.valid)
        ++result.valid_slice_count;

    result.slice_info.push_back(info);
}
            switch (type)
            {
                case 1:
                    ++result.slice_count;
                    break;
                case 5:
                    ++result.idr_count;
                    ++result.slice_count;
                    break;
                case 7:
{
    ++result.sps_count;

    const SpsInfo info = parse_sps(
        data + offset,
        nal_size
    );

    if (info.valid)
        ++result.valid_sps_count;

    result.sps_info.push_back(info);
    break;
}

case 8:
{
    ++result.pps_count;

    const PpsInfo info = parse_pps(
        data + offset,
        nal_size
    );

    if (info.valid)
        ++result.valid_pps_count;

    result.pps_info.push_back(info);
    break;
}default:
                    break;
            }

            result.units.push_back(unit);
            ++result.total_nal_units;

            offset += nal_size;
        }

        result.length_size = length_size;
        result.success = true;

        return result;
    }

    AnalysisResult analyze_sample(
        const uint8_t* data,
        uint32_t size,
        const AvcConfiguration& configuration)
    {
        AnalysisResult result;

        if (!configuration.valid)
        {
            result.error = "Configuration AVC invalide.";
            return result;
        }

        if (configuration.length_size != 1 &&
            configuration.length_size != 2 &&
            configuration.length_size != 4)
        {
            result.error = "Taille de prefixe NAL invalide.";
            return result;
        }

        // Analyse initiale de l'echantillon.
        result = analyze_sample(
            data,
            size,
            configuration.length_size
        );

        if (!result.success)
            return result;

        // Analyse des SPS provenant de avcC.
        std::vector<SpsInfo> sps_list;

        for (const auto& nal : configuration.sps_units)
        {
            SpsInfo sps = parse_sps(
                nal.data(),
                static_cast<uint32_t>(nal.size())
            );

            if (!sps.valid)
            {
                result.success = false;
                result.error = "SPS de avcC invalide : " + sps.error;
                return result;
            }

            sps_list.push_back(sps);
        }

        // Analyse des PPS provenant de avcC.
        std::vector<PpsInfo> pps_list;

        for (const auto& nal : configuration.pps_units)
        {
            PpsInfo pps = parse_pps(
                nal.data(),
                static_cast<uint32_t>(nal.size())
            );

            if (!pps.valid)
            {
                result.success = false;
                result.error = "PPS de avcC invalide : " + pps.error;
                return result;
            }

            pps_list.push_back(pps);
        }

        result.sps_info = sps_list;
        result.pps_info = pps_list;

        result.valid_sps_count =
            static_cast<uint32_t>(sps_list.size());

        result.valid_pps_count =
            static_cast<uint32_t>(pps_list.size());

        // Reanalyse des slices avec les parametres SPS/PPS.
        result.valid_slice_count = 0;

        uint32_t offset = 0;
        uint32_t slice_index = 0;

        while (offset < size)
        {
            uint32_t nal_size = 0;

            for (uint32_t i = 0;
                 i < configuration.length_size;
                 ++i)
            {
                nal_size = (nal_size << 8) | data[offset + i];
            }

            offset += configuration.length_size;

            const uint8_t* nal_data = data + offset;
            const uint8_t nal_type = nal_data[0] & 0x1F;

            if (nal_type == 1 || nal_type == 5)
            {
                SliceInfo identity = parse_slice_header(
                    nal_data,
                    nal_size
                );

                SliceInfo full_info = identity;

                if (identity.valid)
                {
                    const PpsInfo* matched_pps = nullptr;
                    const SpsInfo* matched_sps = nullptr;

                    for (const auto& pps : pps_list)
                    {
                        if (pps.pps_id == identity.pic_parameter_set_id)
                        {
                            matched_pps = &pps;
                            break;
                        }
                    }

                    if (matched_pps != nullptr)
                    {
                        for (const auto& sps : sps_list)
                        {
                            if (sps.sps_id == matched_pps->sps_id)
                            {
                                matched_sps = &sps;
                                break;
                            }
                        }
                    }

                    if (matched_pps != nullptr && matched_sps != nullptr)
                    {
                        full_info = parse_slice_header(
                            nal_data,
                            nal_size,
                            *matched_sps,
                            *matched_pps
                        );
                    }
                    else
                    {
                        full_info.valid = false;
                        full_info.core_header_complete = false;

                        full_info.error =
                            matched_pps == nullptr
                            ? "PPS reference par le slice introuvable."
                            : "SPS reference par le PPS introuvable.";
                    }
                }

                if (full_info.valid)
                    ++result.valid_slice_count;

                if (slice_index < result.slice_info.size())
                    result.slice_info[slice_index] = full_info;

                ++slice_index;
            }

            offset += nal_size;
        }

        return result;
    }
}




