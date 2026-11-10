#include <map>
#include "jrte_api.h"
#include "jrte_h264_analyzer.h"

#include <sstream>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

namespace
{
    struct Box
    {
        uint64_t offset = 0;
        uint64_t size = 0;
        uint64_t header = 0;
        std::string type;
    };

    struct Stsc
    {
        uint32_t first_chunk;
        uint32_t samples_per_chunk;
        uint32_t description;
    };

    uint32_t get32(const unsigned char* p)
    {
        return (uint32_t(p[0]) << 24) |
               (uint32_t(p[1]) << 16) |
               (uint32_t(p[2]) << 8) |
               uint32_t(p[3]);
    }

    uint64_t get64(const unsigned char* p)
    {
        return (uint64_t(get32(p)) << 32) | get32(p + 4);
    }

    bool read_at(
        std::ifstream& file,
        uint64_t offset,
        unsigned char* data,
        size_t length)
    {
        if (offset >
            uint64_t(std::numeric_limits<std::streamoff>::max()))
            return false;

        file.clear();
        file.seekg(static_cast<std::streamoff>(offset));

        if (!file)
            return false;

        file.read(reinterpret_cast<char*>(data),
                  static_cast<std::streamsize>(length));

        return file.gcount() ==
               static_cast<std::streamsize>(length);
    }

    bool read_box(
        std::ifstream& file,
        uint64_t offset,
        uint64_t end,
        Box& box)
    {
        if (offset > end || end - offset < 8)
            return false;

        unsigned char h[16]{};

        if (!read_at(file, offset, h, 8))
            return false;

        uint64_t size = get32(h);
        uint64_t header = 8;

        box.type.assign(
            reinterpret_cast<const char*>(h + 4), 4);

        if (size == 1)
        {
            if (end - offset < 16 ||
                !read_at(file, offset, h, 16))
                return false;

            size = get64(h + 8);
            header = 16;
        }
        else if (size == 0)
        {
            size = end - offset;
        }

        if (size < header || size > end - offset)
            return false;

        box.offset = offset;
        box.size = size;
        box.header = header;

        return true;
    }

    bool find_child(
        std::ifstream& file,
        const Box& parent,
        const char* type,
        Box& result)
    {
        uint64_t pos = parent.offset + parent.header;
        const uint64_t end = parent.offset + parent.size;

        while (pos < end)
        {
            Box b{};

            if (!read_box(file, pos, end, b))
                return false;

            if (b.type == type)
            {
                result = b;
                return true;
            }

            pos += b.size;
        }

        return false;
    }

    bool parse_sizes(
        std::ifstream& file,
        const Box& box,
        std::vector<uint32_t>& sizes)
    {
        unsigned char h[12]{};

        if (box.size < box.header + 12 ||
            !read_at(file, box.offset + box.header, h, 12))
            return false;

        const uint32_t default_size = get32(h + 4);
        const uint32_t count = get32(h + 8);

        constexpr uint32_t MAX_SAMPLES = 10000000;

        if (count > MAX_SAMPLES)
            return false;

        sizes.clear();
        sizes.reserve(count);

        if (default_size != 0)
        {
            sizes.assign(count, default_size);
            return true;
        }

        const uint64_t data = box.offset + box.header + 12;
        const uint64_t available = box.size - box.header - 12;

        if (uint64_t(count) * 4 > available)
            return false;

        for (uint32_t i = 0; i < count; ++i)
        {
            unsigned char value[4]{};

            if (!read_at(file, data + uint64_t(i) * 4,
                         value, 4))
                return false;

            sizes.push_back(get32(value));
        }

        return true;
    }

    bool parse_stsc(
        std::ifstream& file,
        const Box& box,
        std::vector<Stsc>& entries)
    {
        unsigned char h[8]{};

        if (box.size < box.header + 8 ||
            !read_at(file, box.offset + box.header, h, 8))
            return false;

        const uint32_t count = get32(h + 4);

        if (count == 0 || count > 1000000 ||
            uint64_t(count) * 12 > box.size - box.header - 8)
            return false;

        entries.clear();
        entries.reserve(count);

        const uint64_t data = box.offset + box.header + 8;

        for (uint32_t i = 0; i < count; ++i)
        {
            unsigned char e[12]{};

            if (!read_at(file, data + uint64_t(i) * 12,
                         e, 12))
                return false;

            Stsc item{
                get32(e), get32(e + 4), get32(e + 8)
            };

            if (item.first_chunk == 0 ||
                item.samples_per_chunk == 0 ||
                item.description == 0)
                return false;

            if (!entries.empty() &&
                item.first_chunk <= entries.back().first_chunk)
                return false;

            entries.push_back(item);
        }

        return true;
    }

    bool parse_offsets(
        std::ifstream& file,
        const Box& box,
        bool wide,
        std::vector<uint64_t>& offsets)
    {
        unsigned char h[8]{};

        if (box.size < box.header + 8 ||
            !read_at(file, box.offset + box.header, h, 8))
            return false;

        const uint32_t count = get32(h + 4);
        const uint64_t width = wide ? 8 : 4;

        if (count == 0 || count > 1000000 ||
            uint64_t(count) * width > box.size - box.header - 8)
            return false;

        offsets.clear();
        offsets.reserve(count);

        const uint64_t data = box.offset + box.header + 8;

        for (uint32_t i = 0; i < count; ++i)
        {
            unsigned char value[8]{};

            if (!read_at(file, data + uint64_t(i) * width,
                         value, static_cast<size_t>(width)))
                return false;

            offsets.push_back(wide ? get64(value) : get32(value));
        }

        return true;
    }

    bool is_inside_mdat(
        uint64_t offset,
        uint64_t size,
        const std::vector<Box>& mdats)
    {
        for (const Box& mdat : mdats)
        {
            const uint64_t start = mdat.offset + mdat.header;
            const uint64_t end = mdat.offset + mdat.size;

            if (offset >= start &&
                offset <= end &&
                size <= end - offset)
                return true;
        }

        return false;
    }

    bool locate_video_sample(
        std::ifstream& file,
        const Box& trak,
        uint32_t wanted_sample,
        const std::vector<Box>& mdats,
        uint64_t file_size,
        uint64_t& sample_offset,
        uint32_t& sample_size)
    {
        Box mdia{}, hdlr{}, minf{}, stbl{};
        Box stsz{}, stsc{}, stco{}, co64{};

        if (!find_child(file, trak, "mdia", mdia) ||
            !find_child(file, mdia, "hdlr", hdlr))
            return false;

        unsigned char handler[12]{};

        if (hdlr.size < hdlr.header + 12 ||
            !read_at(file, hdlr.offset + hdlr.header,
                     handler, 12))
            return false;

        if (std::string(
                reinterpret_cast<char*>(handler + 8), 4) != "vide")
            return false;

        if (!find_child(file, mdia, "minf", minf) ||
            !find_child(file, minf, "stbl", stbl) ||
            !find_child(file, stbl, "stsz", stsz) ||
            !find_child(file, stbl, "stsc", stsc))
            return false;

        const bool has_stco = find_child(file, stbl, "stco", stco);
        const bool has_co64 = find_child(file, stbl, "co64", co64);

        if (has_stco == has_co64)
            return false;

        std::vector<uint32_t> sizes;
        std::vector<Stsc> mappings;
        std::vector<uint64_t> chunks;

        if (!parse_sizes(file, stsz, sizes) ||
            !parse_stsc(file, stsc, mappings) ||
            !parse_offsets(
                file, has_co64 ? co64 : stco,
                has_co64, chunks))
            return false;

        if (wanted_sample >= sizes.size())
            return false;

        uint64_t sample_index = 0;
        size_t mapping_index = 0;

        for (size_t chunk_index = 0;
             chunk_index < chunks.size(); ++chunk_index)
        {
            const uint64_t chunk_number = chunk_index + 1;

            while (mapping_index + 1 < mappings.size() &&
                   mappings[mapping_index + 1].first_chunk
                       <= chunk_number)
                ++mapping_index;

            if (mappings[mapping_index].first_chunk > chunk_number)
                return false;

            uint64_t current = chunks[chunk_index];

            for (uint32_t j = 0;
                 j < mappings[mapping_index].samples_per_chunk; ++j)
            {
                if (sample_index >= sizes.size())
                    return false;

                const uint32_t current_size =
                    sizes[static_cast<size_t>(sample_index)];

                if (current > file_size ||
                    current_size > file_size - current)
                    return false;

                if (sample_index == wanted_sample)
                {
                    if (!is_inside_mdat(current, current_size, mdats))
                        return false;

                    sample_offset = current;
                    sample_size = current_size;
                    return true;
                }

                if (current_size >
                    std::numeric_limits<uint64_t>::max() - current)
                    return false;

                current += current_size;
                ++sample_index;
            }
        }

        return false;
    }

    bool find_video_avcc(
        std::ifstream& file,
        const Box& trak,
        uint32_t wanted_sample,
        Box& avcc,
        std::string& codec)
    {
        Box mdia{}, hdlr{}, minf{}, stbl{};
        Box stsz{}, stsc{}, stco{}, co64{}, stsd{};

        if (!find_child(file, trak, "mdia", mdia) ||
            !find_child(file, mdia, "hdlr", hdlr))
            return false;

        unsigned char handler[12]{};

        if (hdlr.size < hdlr.header + 12 ||
            !read_at(file, hdlr.offset + hdlr.header,
                     handler, 12))
            return false;

        if (std::string(
                reinterpret_cast<char*>(handler + 8), 4) != "vide")
            return false;

        if (!find_child(file, mdia, "minf", minf) ||
            !find_child(file, minf, "stbl", stbl) ||
            !find_child(file, stbl, "stsz", stsz) ||
            !find_child(file, stbl, "stsc", stsc) ||
            !find_child(file, stbl, "stsd", stsd))
            return false;

        const bool has_stco = find_child(file, stbl, "stco", stco);
        const bool has_co64 = find_child(file, stbl, "co64", co64);

        if (has_stco == has_co64)
            return false;

        std::vector<uint32_t> sizes;
        std::vector<Stsc> mappings;
        std::vector<uint64_t> chunks;

        if (!parse_sizes(file, stsz, sizes) ||
            !parse_stsc(file, stsc, mappings) ||
            !parse_offsets(
                file, has_co64 ? co64 : stco,
                has_co64, chunks))
            return false;

        if (wanted_sample >= sizes.size())
            return false;

        // Trouver le sample_description_index du sample demandÃƒÆ’Ã‚Â©.
        uint64_t sample_index = 0;
        uint32_t description_index = 0;
        size_t mapping_index = 0;
        bool found = false;

        for (size_t chunk_index = 0;
             chunk_index < chunks.size(); ++chunk_index)
        {
            const uint64_t chunk_number = chunk_index + 1;

            while (mapping_index + 1 < mappings.size() &&
                   mappings[mapping_index + 1].first_chunk
                       <= chunk_number)
                ++mapping_index;

            if (mappings[mapping_index].first_chunk > chunk_number)
                return false;

            const uint32_t samples_per_chunk =
                mappings[mapping_index].samples_per_chunk;

            if (sample_index > sizes.size())
                return false;

            if (wanted_sample >= sample_index &&
                uint64_t(wanted_sample) <
                    sample_index + samples_per_chunk)
            {
                description_index =
                    mappings[mapping_index].description;

                found = true;
                break;
            }

            sample_index += samples_per_chunk;

            if (sample_index > sizes.size())
                return false;
        }

        if (!found || description_index == 0)
            return false;

        // Lire la table stsd.
        unsigned char stsd_header[8]{};

        if (stsd.size < stsd.header + 8 ||
            !read_at(file, stsd.offset + stsd.header,
                     stsd_header, 8))
            return false;

        const uint32_t entry_count = get32(stsd_header + 4);

        if (description_index > entry_count ||
            entry_count > 100000)
            return false;

        uint64_t pos = stsd.offset + stsd.header + 8;
        const uint64_t stsd_end = stsd.offset + stsd.size;

        Box sample_entry{};

        for (uint32_t i = 1; i <= description_index; ++i)
        {
            if (!read_box(file, pos, stsd_end, sample_entry))
                return false;

            if (i == description_index)
                break;

            pos += sample_entry.size;
        }

        codec = sample_entry.type;

        if (codec != "avc1" && codec != "avc3")
            return false;

        // Une VisualSampleEntry possÃƒÆ’Ã‚Â¨de 78 octets
        // de champs fixes avant ses boÃƒÆ’Ã‚Â®tes enfants.
        const uint64_t child_start =
            sample_entry.offset + sample_entry.header + 78;

        const uint64_t entry_end =
            sample_entry.offset + sample_entry.size;

        if (child_start > entry_end)
            return false;

        pos = child_start;

        while (pos < entry_end)
        {
            Box child{};

            if (!read_box(file, pos, entry_end, child))
                return false;

            if (child.type == "avcC")
            {
                avcc = child;
                return true;
            }

            pos += child.size;
        }

        return false;
    }

    struct SampleLocation
    {
        uint64_t offset = 0;
        uint32_t size = 0;
        uint32_t description = 0;
    };

    // Construit une seule fois l'index complet des samples de la piste video.
    bool build_video_sample_index(
        std::ifstream& file,
        const Box& trak,
        const std::vector<Box>& mdats,
        uint64_t file_size,
        std::vector<SampleLocation>& samples)
    {
        Box mdia{}, hdlr{}, minf{}, stbl{};
        Box stsz{}, stsc{}, stco{}, co64{};

        if (!find_child(file, trak, "mdia", mdia) ||
            !find_child(file, mdia, "hdlr", hdlr))
            return false;

        unsigned char handler[12]{};
        if (hdlr.size < hdlr.header + 12 ||
            !read_at(file, hdlr.offset + hdlr.header, handler, 12) ||
            std::string(reinterpret_cast<char*>(handler + 8), 4) != "vide")
            return false;

        if (!find_child(file, mdia, "minf", minf) ||
            !find_child(file, minf, "stbl", stbl) ||
            !find_child(file, stbl, "stsz", stsz) ||
            !find_child(file, stbl, "stsc", stsc))
            return false;

        const bool has_stco = find_child(file, stbl, "stco", stco);
        const bool has_co64 = find_child(file, stbl, "co64", co64);
        if (has_stco == has_co64)
            return false;

        std::vector<uint32_t> sizes;
        std::vector<Stsc> mappings;
        std::vector<uint64_t> chunks;
        if (!parse_sizes(file, stsz, sizes) ||
            !parse_stsc(file, stsc, mappings) ||
            !parse_offsets(file, has_co64 ? co64 : stco, has_co64, chunks) ||
            sizes.empty() || mappings.empty() || chunks.empty())
            return false;

        samples.clear();
        samples.reserve(sizes.size());
        uint64_t sample_index = 0;
        size_t mapping_index = 0;

        for (size_t chunk_index = 0; chunk_index < chunks.size(); ++chunk_index)
        {
            const uint64_t chunk_number = static_cast<uint64_t>(chunk_index) + 1;
            while (mapping_index + 1 < mappings.size() &&
                   mappings[mapping_index + 1].first_chunk <= chunk_number)
                ++mapping_index;

            if (mappings[mapping_index].first_chunk > chunk_number)
                return false;

            uint64_t current_offset = chunks[chunk_index];
            const Stsc& mapping = mappings[mapping_index];

            for (uint32_t j = 0; j < mapping.samples_per_chunk; ++j)
            {
                if (sample_index >= sizes.size())
                    return false;

                const uint32_t sample_size = sizes[static_cast<size_t>(sample_index)];
                if (current_offset > file_size ||
                    sample_size > file_size - current_offset ||
                    !is_inside_mdat(current_offset, sample_size, mdats))
                    return false;

                samples.push_back({current_offset, sample_size, mapping.description});
                if (sample_size > std::numeric_limits<uint64_t>::max() - current_offset)
                    return false;
                current_offset += sample_size;
                ++sample_index;
            }
        }

        return sample_index == sizes.size() && samples.size() == sizes.size();
    }

    bool find_sample_in_moov(
        std::ifstream& file,
        const Box& moov,
        uint32_t sample_index,
        const std::vector<Box>& mdats,
        uint64_t file_size,
        uint64_t& sample_offset,
        uint32_t& sample_size)
    {
        uint64_t pos = moov.offset + moov.header;
        const uint64_t end = moov.offset + moov.size;

        while (pos < end)
        {
            Box box{};

            if (!read_box(file, pos, end, box))
                return false;

            if (box.type == "trak" &&
                locate_video_sample(
                    file, box, sample_index, mdats,
                    file_size, sample_offset, sample_size))
                return true;

            pos += box.size;
        }

        return false;
    }
}

extern "C"
{
    JRTE_API int32_t jrte_mp4_get_video_sample_count(
        const char* utf8_path,
        uint32_t* sample_count)
    {
        if (sample_count)
            *sample_count = 0;

        if (!utf8_path || !utf8_path[0] || !sample_count)
            return -1;

        try
        {
            std::ifstream file(
                std::filesystem::u8path(utf8_path),
                std::ios::binary);

            if (!file)
                return -2;

            file.seekg(0, std::ios::end);
            const auto end_position = file.tellg();

            if (end_position < 0)
                return -3;

            const uint64_t file_size =
                static_cast<uint64_t>(end_position);

            Box moov{};
            bool found_moov = false;
            uint64_t pos = 0;

            while (pos < file_size)
            {
                Box box{};

                if (!read_box(file, pos, file_size, box))
                    return -3;

                if (box.type == "moov")
                {
                    moov = box;
                    found_moov = true;
                    break;
                }

                pos += box.size;
            }

            if (!found_moov)
                return -4;

            pos = moov.offset + moov.header;
            const uint64_t moov_end =
                moov.offset + moov.size;

            while (pos < moov_end)
            {
                Box trak{};

                if (!read_box(file, pos, moov_end, trak))
                    return -3;

                if (trak.type == "trak")
                {
                    Box mdia{}, hdlr{}, minf{}, stbl{}, stsz{};

                    if (find_child(file, trak, "mdia", mdia) &&
                        find_child(file, mdia, "hdlr", hdlr))
                    {
                        unsigned char handler[12]{};

                        if (hdlr.size >= hdlr.header + 12 &&
                            read_at(
                                file,
                                hdlr.offset + hdlr.header,
                                handler,
                                12))
                        {
                            const std::string handler_type(
                                reinterpret_cast<char*>(handler + 8), 4);

                            if (handler_type == "vide")
                            {
                                if (!find_child(file, mdia, "minf", minf) ||
                                    !find_child(file, minf, "stbl", stbl) ||
                                    !find_child(file, stbl, "stsz", stsz))
                                    return -5;

                                std::vector<uint32_t> sizes;

                                if (!parse_sizes(file, stsz, sizes))
                                    return -5;

                                if (sizes.size() >
                                    std::numeric_limits<uint32_t>::max())
                                    return -6;

                                *sample_count =
                                    static_cast<uint32_t>(sizes.size());

                                return 0;
                            }
                        }
                    }
                }

                pos += trak.size;
            }

            return -5;
        }
        catch (...)
        {
            *sample_count = 0;
            return -3;
        }
    }
    JRTE_API int32_t jrte_mp4_get_avc_configuration(
        const char* utf8_path,
        uint32_t sample_index,
        char* output,
        uint32_t output_capacity)
    {
        if (!utf8_path || !utf8_path[0] ||
            !output || output_capacity == 0)
            return -1;

        output[0] = '\0';

        try
        {
            std::ifstream file(
                std::filesystem::u8path(utf8_path),
                std::ios::binary);

            if (!file)
                return -2;

            file.seekg(0, std::ios::end);
            const auto end_position = file.tellg();

            if (end_position < 0)
                return -3;

            const uint64_t file_size =
                static_cast<uint64_t>(end_position);

            Box moov{};
            bool found_moov = false;
            uint64_t pos = 0;

            while (pos < file_size)
            {
                Box box{};

                if (!read_box(file, pos, file_size, box))
                    return -3;

                if (box.type == "moov")
                {
                    moov = box;
                    found_moov = true;
                    break;
                }

                pos += box.size;
            }

            if (!found_moov)
                return -4;

            Box avcc{};
            std::string codec;
            bool found_avcc = false;

            pos = moov.offset + moov.header;
            const uint64_t moov_end =
                moov.offset + moov.size;

            while (pos < moov_end)
            {
                Box box{};

                if (!read_box(file, pos, moov_end, box))
                    return -3;

                if (box.type == "trak" &&
                    find_video_avcc(
                        file, box, sample_index,
                        avcc, codec))
                {
                    found_avcc = true;
                    break;
                }

                pos += box.size;
            }

            if (!found_avcc)
                return -5;

            const uint64_t payload_size =
                avcc.size - avcc.header;

            constexpr uint64_t MAX_AVCC_SIZE = 1024 * 1024;

            if (payload_size == 0 ||
                payload_size > MAX_AVCC_SIZE)
                return -5;

            std::vector<uint8_t> data(
                static_cast<size_t>(payload_size));

            if (!read_at(
                    file,
                    avcc.offset + avcc.header,
                    data.data(),
                    data.size()))
                return -3;

            const auto config =
                jrte::h264::parse_avcc(
                    data.data(),
                    static_cast<uint32_t>(data.size()));

            if (!config.valid)
                return -5;

            std::ostringstream report;

            report
                << "JRTE H264 CONFIGURATION\n"
                << "Codec : " << codec << "\n"
                << "Profile : "
                << static_cast<unsigned>(config.profile) << "\n"
                << "Compatibility : "
                << static_cast<unsigned>(config.compatibility)
                << "\n"
                << "Level : "
                << static_cast<unsigned>(config.level) << "\n"
                << "Taille prefixe NAL : "
                << config.length_size << " octets\n"
                << "Nombre de SPS : "
                << config.sps_units.size() << "\n"
                << "Nombre de PPS : "
                << config.pps_units.size() << "\n";

            for (size_t i = 0;
                 i < config.sps_units.size(); ++i)
            {
                const auto& sps = config.sps_units[i];

                report << "SPS #" << (i + 1)
                       << " : "
                       << sps.size()
                       << " octets\n";

                report << "SPS HEX : ";

                for (unsigned char byte : sps)
                {
                    report << std::uppercase
                           << std::hex
                           << std::setw(2)
                           << std::setfill('0')
                           << static_cast<unsigned>(byte)
                           << " ";
                }

                report << std::dec << "\n";
            }

            // Analyse detaillee des SPS extraits du vrai avcC.
            report << "\n===== ANALYSE SPS =====\n";

            for (size_t i = 0;
                 i < config.sps_units.size(); ++i)
            {
                const auto& sps = config.sps_units[i];

                const auto info =
                    jrte::h264::parse_sps(
                        sps.data(),
                        static_cast<uint32_t>(sps.size())
                    );

                report << "\nSPS #" << (i + 1) << "\n";

                if (!info.valid)
                {
                    report << "Statut : INVALIDE\n";
                    report << "Erreur : " << info.error << "\n";
                    continue;
                }

                report
                    << "Statut : VALIDE\n"
                    << "Identifiant SPS : " << info.sps_id << "\n"
                    << "Profil SPS : " << info.profile_idc << "\n"
                    << "Niveau SPS : " << info.level_idc << "\n"
                    << "Largeur : " << info.width << " pixels\n"
                    << "Hauteur : " << info.height << " pixels\n"
                    << "Format chromatique : "
                    << info.chroma_format_idc << "\n"
                    << "Profondeur luminance : "
                    << info.bit_depth_luma << " bits\n"
                    << "Profondeur chrominance : "
                    << info.bit_depth_chroma << " bits\n"
                    << "Frame MBS only : "
                    << (info.frame_mbs_only ? "Oui" : "Non")
                    << "\n";
            }

            for (size_t i = 0;
                 i < config.pps_units.size(); ++i)
            {
                const auto& pps = config.pps_units[i];

                report << "PPS #" << (i + 1)
                       << " : "
                       << pps.size()
                       << " octets\n";

                report << "PPS HEX : ";

                for (unsigned char byte : pps)
                {
                    report << std::uppercase
                           << std::hex
                           << std::setw(2)
                           << std::setfill('0')
                           << static_cast<unsigned>(byte)
                           << " ";
                }

                report << std::dec << "\n";
            }

            // Analyse detaillee des PPS extraits du avcC.
            report << "\n===== ANALYSE PPS =====\n";

            for (size_t i = 0; i < config.pps_units.size(); ++i)
            {
                const auto& pps = config.pps_units[i];

                const auto info = jrte::h264::parse_pps(
                    pps.data(),
                    static_cast<uint32_t>(pps.size())
                );

                report << "\nPPS #" << (i + 1) << "\n";

                if (!info.valid)
                {
                    report << "Statut : INVALIDE\n";
                    report << "Erreur : " << info.error << "\n";
                    continue;
                }

                bool referenced_sps_found = false;

                for (const auto& sps : config.sps_units)
                {
                    const auto sps_info = jrte::h264::parse_sps(
                        sps.data(),
                        static_cast<uint32_t>(sps.size())
                    );

                    if (sps_info.valid &&
                        sps_info.sps_id == info.sps_id)
                    {
                        referenced_sps_found = true;
                        break;
                    }
                }

                report
                    << "Statut : VALIDE\n"
                    << "Identifiant PPS : " << info.pps_id << "\n"
                    << "Identifiant SPS reference : " << info.sps_id << "\n"
                    << "SPS correspondant trouve : "
                    << (referenced_sps_found ? "Oui" : "Non") << "\n"
                    << "Entropy coding : "
                    << (info.entropy_coding_mode ? "Oui" : "Non") << "\n"
                    << "Slice groups : " << info.num_slice_groups << "\n"
                    << "Weighted prediction : "
                    << (info.weighted_pred ? "Oui" : "Non") << "\n"
                    << "Weighted bipred IDC : "
                    << info.weighted_bipred_idc << "\n"
                    << "Pic init QP minus 26 : "
                    << info.pic_init_qp_minus26 << "\n"
                    << "Pic init QS minus 26 : "
                    << info.pic_init_qs_minus26 << "\n"
                    << "Chroma QP offset : "
                    << info.chroma_qp_index_offset << "\n"
                    << "Deblocking filter control : "
                    << (info.deblocking_filter_control_present ? "Oui" : "Non") << "\n"
                    << "Constrained intra prediction : "
                    << (info.constrained_intra_pred ? "Oui" : "Non") << "\n"
                    << "Redundant picture count : "
                    << (info.redundant_pic_cnt_present ? "Oui" : "Non") << "\n";
            }
            const std::string result = report.str();

            if (result.size() >= output_capacity)
                return -6;

            std::copy(
                result.begin(),
                result.end(),
                output);

            output[result.size()] = '\0';

            return 0;
        }
        catch (...)
        {
            return -3;
        }
    }

    JRTE_API int32_t jrte_mp4_analyze_video_sample(
        const char* utf8_path,
        uint32_t sample_index,
        char* output,
        uint32_t output_capacity)
    {
        if (!utf8_path || !utf8_path[0] ||
            !output || output_capacity == 0)
            return -1;

        output[0] = '\0';

        try
        {
            constexpr uint32_t MAX_SAMPLE_SIZE = 64 * 1024 * 1024;
            constexpr uint32_t CONFIG_BUFFER_SIZE = 4 * 1024 * 1024;

            // Extraire l'ÃƒÂ©chantillon vidÃƒÂ©o.
            std::vector<unsigned char> sample(MAX_SAMPLE_SIZE);
            uint32_t bytes_written = 0;

            const int32_t extract_status =
                jrte_mp4_extract_video_sample(
                    utf8_path,
                    sample_index,
                    sample.data(),
                    MAX_SAMPLE_SIZE,
                    &bytes_written
                );

            if (extract_status != 0)
                return extract_status;

            if (bytes_written == 0)
                return -7;

            sample.resize(bytes_written);

            // RÃƒÂ©cupÃƒÂ©rer la configuration AVC.
            std::vector<char> config_buffer(CONFIG_BUFFER_SIZE);

            const int32_t config_status =
                jrte_mp4_get_avc_configuration(
                    utf8_path,
                    sample_index,
                    config_buffer.data(),
                    CONFIG_BUFFER_SIZE
                );

            if (config_status != 0)
                return config_status;

            const std::string config_text(config_buffer.data());

            const std::string prefix =
                "Taille prefixe NAL : ";

            const size_t prefix_position =
                config_text.find(prefix);

            if (prefix_position == std::string::npos)
                return -8;

            const size_t value_position =
                prefix_position + prefix.size();

            size_t parsed_characters = 0;

            const unsigned long parsed_length =
                std::stoul(
                    config_text.substr(value_position),
                    &parsed_characters
                );

            if (parsed_length != 1 &&
                parsed_length != 2 &&
                parsed_length != 4)
                return -8;

            const uint32_t length_size =
                static_cast<uint32_t>(parsed_length);

            // Analyser les NAL units avec le moteur JRTE.
            const auto analysis =
                jrte::h264::analyze_sample(
                    sample.data(),
                    static_cast<uint32_t>(sample.size()),
                    length_size
                );

            std::ostringstream report;

            report
                << "===== JRTE - ANALYSE H264 =====\n"
                << "Echantillon : " << sample_index << "\n"
                << "Taille : " << sample.size() << " octets\n"
                << "Prefixe NAL : " << length_size << " octets\n"
                << "Analyse reussie : "
                << (analysis.success ? "Oui" : "Non") << "\n";

            if (!analysis.success)
            {
                report << "Erreur : " << analysis.error << "\n";
            }
            else
            {
                uint32_t p_count = 0;
                uint32_t b_count = 0;
                uint32_t i_count = 0;
                uint32_t invalid_slices = 0;

                for (const auto& slice : analysis.slice_info)
                {
                    if (!slice.valid)
                    {
                        ++invalid_slices;
                        continue;
                    }

                    if (slice.slice_type_name == "P")
                        ++p_count;
                    else if (slice.slice_type_name == "B")
                        ++b_count;
                    else if (slice.slice_type_name == "I")
                        ++i_count;
                }

                report
                    << "\n===== NAL UNITS =====\n"
                    << "Total : " << analysis.total_nal_units << "\n"
                    << "Slices : " << analysis.slice_count << "\n"
                    << "Slices IDR : " << analysis.idr_count << "\n"
                    << "SPS : " << analysis.sps_count << "\n"
                    << "PPS : " << analysis.pps_count << "\n"

                    << "\n===== TYPES DE SLICES VALIDES =====\n"
                    << "P : " << p_count << "\n"
                    << "B : " << b_count << "\n"
                    << "I : " << i_count << "\n"
                    << "Slices valides : "
                    << analysis.valid_slice_count << "\n"
                    << "Slices invalides : "
                    << invalid_slices << "\n"

                    << "\n===== VALIDATION SPS / PPS =====\n"
                    << "SPS valides : "
                    << analysis.valid_sps_count << "\n"
                    << "PPS valides : "
                    << analysis.valid_pps_count << "\n";
            }

            const std::string result = report.str();

            if (result.size() >= output_capacity)
                return -6;

            std::copy(
                result.begin(),
                result.end(),
                output
            );

            output[result.size()] = '\0';

            return 0;
        }
        catch (...)
        {
            return -3;
        }
    }
    JRTE_API int32_t jrte_mp4_analyze_all_video_samples(
        const char* utf8_path,
        char* output,
        uint32_t output_capacity)
    {
        if (!utf8_path || !utf8_path[0] || !output || output_capacity == 0)
            return -1;
        output[0] = '\0';

        try
        {
            constexpr uint32_t MAX_SAMPLE_SIZE = 64 * 1024 * 1024;
            std::ifstream file(std::filesystem::u8path(utf8_path), std::ios::binary);
            if (!file)
                return -2;

            file.seekg(0, std::ios::end);
            const auto end_position = file.tellg();
            if (end_position < 0)
                return -3;
            const uint64_t file_size = static_cast<uint64_t>(end_position);

            Box moov{};
            bool found_moov = false;
            std::vector<Box> mdats;
            uint64_t pos = 0;
            while (pos < file_size)
            {
                Box box{};
                if (!read_box(file, pos, file_size, box))
                    return -3;
                if (box.type == "moov")
                {
                    moov = box;
                    found_moov = true;
                }
                else if (box.type == "mdat")
                    mdats.push_back(box);
                pos += box.size;
            }
            if (!found_moov || mdats.empty())
                return -4;

            std::vector<SampleLocation> samples;
            Box video_trak{};
            pos = moov.offset + moov.header;
            const uint64_t moov_end = moov.offset + moov.size;
            while (pos < moov_end)
            {
                Box trak{};
                if (!read_box(file, pos, moov_end, trak))
                    return -3;
                if (trak.type == "trak" &&
                    build_video_sample_index(file, trak, mdats, file_size, samples))
                {
                    video_trak = trak;
                    break;
                }
                pos += trak.size;
            }
            if (samples.empty())
                return -5;

            // PremiÃ¨re version : une seule description AVC pour toute la piste.
            const uint32_t description = samples.front().description;
            for (const auto& sample : samples)
                if (sample.description != description)
                    return -9;

            Box avcc{};
            std::string codec;
            if (!find_video_avcc(file, video_trak, 0, avcc, codec))
                return -5;
            if (codec != "avc1" && codec != "avc3")
                return -8;

            const uint64_t avcc_size = avcc.size - avcc.header;
            if (avcc_size == 0 || avcc_size > 1024 * 1024)
                return -5;
            std::vector<uint8_t> avcc_data(static_cast<size_t>(avcc_size));
            if (!read_at(file, avcc.offset + avcc.header, avcc_data.data(), avcc_data.size()))
                return -3;
            const auto config = jrte::h264::parse_avcc(
                avcc_data.data(), static_cast<uint32_t>(avcc_data.size()));
            if (!config.valid)
                return -8;

            uint64_t successful_samples = 0;
            uint64_t failed_samples = 0;
            uint64_t total_nals = 0;
            uint64_t total_slices = 0;
            uint64_t total_idr = 0;
            uint64_t total_p = 0, total_b = 0, total_i = 0;
            uint64_t valid_slices = 0;
            uint64_t invalid_slices = 0;
            uint64_t invalid_slice_details = 0;
            uint64_t invalid_samples_reported = 0;

            std::map<std::string, uint64_t> invalid_slice_reasons;
            std::ostringstream errors;
            std::ostringstream slice_errors;

            std::vector<unsigned char> sample_buffer;

            for (size_t index = 0; index < samples.size(); ++index)
            {
                const SampleLocation& location = samples[index];
                if (location.size == 0 || location.size > MAX_SAMPLE_SIZE)
                {
                    ++failed_samples;
                    if (invalid_samples_reported++ < 20)
                        errors << "Sample " << index << " : taille invalide ("
                               << location.size << ")\n";
                    continue;
                }

                sample_buffer.resize(location.size);
                if (!read_at(file, location.offset, sample_buffer.data(), sample_buffer.size()))
                {
                    ++failed_samples;
                    if (invalid_samples_reported++ < 20)
                        errors << "Sample " << index << " : lecture impossible\n";
                    continue;
                }

                const auto analysis = jrte::h264::analyze_sample(
                    sample_buffer.data(),
                    static_cast<uint32_t>(sample_buffer.size()),
                    config);
                if (!analysis.success)
                {
                    ++failed_samples;
                    if (invalid_samples_reported++ < 20)
                        errors << "Sample " << index << " : " << analysis.error << "\n";
                    continue;
                }

                ++successful_samples;
                total_nals += analysis.total_nal_units;
                total_slices += analysis.slice_count;
                total_idr += analysis.idr_count;
                valid_slices += analysis.valid_slice_count;
                for (size_t slice_index = 0;
                     slice_index < analysis.slice_info.size();
                     ++slice_index)
                {
                    const auto& slice = analysis.slice_info[slice_index];

                    if (!slice.valid)
                    {
                        ++invalid_slices;
                        ++invalid_slice_reasons[slice.error];

                        if (invalid_slice_details < 200)
                        {
                            slice_errors
                                << "Sample " << index
                                << " | Slice " << slice_index
                                << " | NAL type " << static_cast<uint32_t>(slice.nal_type)
                                << " | Erreur : "
                                << (slice.error.empty()
                                    ? "Erreur non precise"
                                    : slice.error)
                                << "\n";

                            ++invalid_slice_details;
                        }

                        continue;
                    }

                    if (slice.slice_type_name == "P") ++total_p;
                    else if (slice.slice_type_name == "B") ++total_b;
                    else if (slice.slice_type_name == "I") ++total_i;
                }
            }

            uint64_t valid_sps = 0, valid_pps = 0;
            for (const auto& sps : config.sps_units)
                if (jrte::h264::parse_sps(sps.data(), static_cast<uint32_t>(sps.size())).valid)
                    ++valid_sps;
            for (const auto& pps : config.pps_units)
                if (jrte::h264::parse_pps(pps.data(), static_cast<uint32_t>(pps.size())).valid)
                    ++valid_pps;

            std::ostringstream report;
            report << "===== JRTE - ANALYSE GLOBALE H.264 =====\n"
                   << "Codec : " << codec << "\n"
                   << "Prefixe NAL : " << config.length_size << " octets\n"
                   << "Samples totaux : " << samples.size() << "\n"
                   << "Samples analyses avec succes : " << successful_samples << "\n"
                   << "Samples en erreur : " << failed_samples << "\n"
                   << "Progression : 100 % (analyse terminee)\n\n"
                   << "===== STATISTIQUES H.264 =====\n"
                   << "NAL units : " << total_nals << "\n"
                   << "Slices : " << total_slices << "\n"
                   << "Slices valides : " << valid_slices << "\n"
                   << "Slices invalides : " << invalid_slices << "\n"
                   << "Slices P : " << total_p << "\n"
                   << "Slices B : " << total_b << "\n"
                   << "Slices I : " << total_i << "\n"
                   << "Images IDR : " << total_idr << "\n"
                   << "SPS valides (avcC) : " << valid_sps << "/" << config.sps_units.size() << "\n"
                   << "PPS valides (avcC) : " << valid_pps << "/" << config.pps_units.size() << "\n";

            if (failed_samples)
                report << "\n===== PREMIERES ERREURS (maximum 20) =====\n"
                       << errors.str();

            if (invalid_slices > 0)
            {
                report << "\n===== REPARTITION DES ERREURS DE SLICES =====\n";

                for (const auto& reason : invalid_slice_reasons)
                {
                    report << reason.first
                           << " : " << reason.second << "\n";
                }

                report << "\n===== DETAILS DES SLICES INVALIDES =====\n"
                       << slice_errors.str();

                if (invalid_slices > invalid_slice_details)
                {
                    report << "... "
                           << (invalid_slices - invalid_slice_details)
                           << " autres erreurs non affichees.\n";
                }
            }

            const std::string result = report.str();
            if (result.size() >= output_capacity)
                return -6;
            std::copy(result.begin(), result.end(), output);
            output[result.size()] = '\0';
            return 0;
        }
        catch (...)
        {
            return -3;
        }
    }

    JRTE_API int32_t jrte_mp4_extract_video_sample(
        const char* utf8_path,
        uint32_t sample_index,
        unsigned char* output,
        uint32_t output_capacity,
        uint32_t* bytes_written)
    {
        if (bytes_written)
            *bytes_written = 0;

        if (!utf8_path || !utf8_path[0] ||
            !output || !bytes_written || output_capacity == 0)
            return -1;

        try
        {
            std::ifstream file(
                std::filesystem::u8path(utf8_path),
                std::ios::binary);

            if (!file)
                return -2;

            file.seekg(0, std::ios::end);
            const auto end_position = file.tellg();

            if (end_position < 0)
                return -3;

            const uint64_t file_size =
                static_cast<uint64_t>(end_position);

            std::vector<Box> mdats;
            Box moov{};
            bool found_moov = false;

            uint64_t pos = 0;

            while (pos < file_size)
            {
                Box box{};

                if (!read_box(file, pos, file_size, box))
                    return -3;

                if (box.type == "moov")
                {
                    moov = box;
                    found_moov = true;
                }
                else if (box.type == "mdat")
                {
                    mdats.push_back(box);
                }

                pos += box.size;
            }

            if (!found_moov || mdats.empty())
                return -4;

            uint64_t sample_offset = 0;
            uint32_t sample_size = 0;

            if (!find_sample_in_moov(
                    file, moov, sample_index, mdats,
                    file_size, sample_offset, sample_size))
                return -5;

            if (sample_size > output_capacity)
                return -6;

            if (sample_size == 0)
            {
                *bytes_written = 0;
                return 0;
            }

            if (!read_at(file, sample_offset, output, sample_size))
                return -3;

            *bytes_written = sample_size;
            return 0;
        }
        catch (...)
        {
            return -3;
        }
    }
}




