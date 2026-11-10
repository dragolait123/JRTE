#include "jrte_sample_reader.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

namespace
{
    constexpr uint64_t MAX_SAMPLES = 10000000;

    struct Box
    {
        uint64_t offset = 0;
        uint64_t size = 0;
        uint64_t header = 0;
        std::string type;
    };

    struct StscEntry
    {
        uint32_t first_chunk = 0;
        uint32_t samples_per_chunk = 0;
        uint32_t description_index = 0;
    };

    uint32_t u32(const unsigned char* p)
    {
        return (uint32_t(p[0]) << 24) |
               (uint32_t(p[1]) << 16) |
               (uint32_t(p[2]) << 8) |
               uint32_t(p[3]);
    }

    uint64_t u64(const unsigned char* p)
    {
        return (uint64_t(u32(p)) << 32) | u32(p + 4);
    }

    bool read_bytes(
        std::ifstream& file,
        uint64_t offset,
        unsigned char* data,
        size_t count)
    {
        if (offset > uint64_t(
                std::numeric_limits<std::streamoff>::max()))
            return false;

        file.clear();
        file.seekg(static_cast<std::streamoff>(offset));

        if (!file)
            return false;

        file.read(reinterpret_cast<char*>(data),
                  static_cast<std::streamsize>(count));

        return file.gcount() == static_cast<std::streamsize>(count);
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

        if (!read_bytes(file, offset, h, 8))
            return false;

        uint64_t size = u32(h);
        uint64_t header = 8;

        box.type.assign(reinterpret_cast<char*>(h + 4), 4);

        if (size == 1)
        {
            if (end - offset < 16 ||
                !read_bytes(file, offset, h, 16))
                return false;

            size = u64(h + 8);
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

    bool child(
        std::ifstream& file,
        const Box& parent,
        const char* wanted,
        Box& result)
    {
        uint64_t pos = parent.offset + parent.header;
        const uint64_t end = parent.offset + parent.size;

        while (pos < end)
        {
            Box b{};

            if (!read_box(file, pos, end, b))
                return false;

            if (b.type == wanted)
            {
                result = b;
                return true;
            }

            pos += b.size;
        }

        return false;
    }

    bool read_fullbox_header(
        std::ifstream& file,
        const Box& box,
        uint32_t& count,
        uint64_t& data_offset)
    {
        unsigned char h[8]{};

        if (box.size < box.header + 8 ||
            !read_bytes(file, box.offset + box.header, h, 8))
            return false;

        count = u32(h + 4);
        data_offset = box.offset + box.header + 8;

        return true;
    }

    bool parse_stsz(
        std::ifstream& file,
        const Box& box,
        std::vector<uint32_t>& sizes)
    {
        unsigned char h[12]{};

        if (box.size < box.header + 12 ||
            !read_bytes(file, box.offset + box.header, h, 12))
            return false;

        const uint32_t default_size = u32(h + 4);
        const uint32_t count = u32(h + 8);

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

            if (!read_bytes(file, data + uint64_t(i) * 4,
                            value, 4))
                return false;

            sizes.push_back(u32(value));
        }

        return true;
    }

    bool parse_stts(
        std::ifstream& file,
        const Box& box,
        uint64_t& sample_count,
        uint64_t& total_duration)
    {
        uint32_t count = 0;
        uint64_t data = 0;

        if (!read_fullbox_header(file, box, count, data))
            return false;

        if (uint64_t(count) * 8 > box.size - box.header - 8)
            return false;

        sample_count = 0;
        total_duration = 0;

        for (uint32_t i = 0; i < count; ++i)
        {
            unsigned char entry[8]{};

            if (!read_bytes(file, data + uint64_t(i) * 8,
                            entry, 8))
                return false;

            const uint64_t n = u32(entry);
            const uint64_t delta = u32(entry + 4);

            if (n > MAX_SAMPLES - sample_count)
                return false;

            if (delta != 0 &&
                n > (std::numeric_limits<uint64_t>::max()
                     - total_duration) / delta)
                return false;

            sample_count += n;
            total_duration += n * delta;
        }

        return true;
    }

    bool parse_stsc(
        std::ifstream& file,
        const Box& box,
        std::vector<StscEntry>& entries)
    {
        uint32_t count = 0;
        uint64_t data = 0;

        if (!read_fullbox_header(file, box, count, data))
            return false;

        if (uint64_t(count) * 12 > box.size - box.header - 8)
            return false;

        entries.clear();
        entries.reserve(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            unsigned char e[12]{};

            if (!read_bytes(file, data + uint64_t(i) * 12,
                            e, 12))
                return false;

            StscEntry item{
                u32(e), u32(e + 4), u32(e + 8)
            };

            if (item.first_chunk == 0 ||
                item.samples_per_chunk == 0 ||
                item.description_index == 0)
                return false;

            if (!entries.empty() &&
                item.first_chunk <= entries.back().first_chunk)
                return false;

            entries.push_back(item);
        }

        return !entries.empty();
    }

    bool parse_chunk_offsets(
        std::ifstream& file,
        const Box& box,
        std::vector<uint64_t>& offsets,
        bool wide)
    {
        uint32_t count = 0;
        uint64_t data = 0;

        if (!read_fullbox_header(file, box, count, data))
            return false;

        const uint64_t width = wide ? 8 : 4;

        if (uint64_t(count) * width > box.size - box.header - 8)
            return false;

        offsets.clear();
        offsets.reserve(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            unsigned char value[8]{};

            if (!read_bytes(file, data + uint64_t(i) * width,
                            value, static_cast<size_t>(width)))
                return false;

            offsets.push_back(wide ? u64(value) : u32(value));
        }

        return true;
    }

    bool parse_stss(
        std::ifstream& file,
        const Box& box,
        std::vector<uint32_t>& sync_samples)
    {
        uint32_t count = 0;
        uint64_t data = 0;

        if (!read_fullbox_header(file, box, count, data))
            return false;

        if (count > MAX_SAMPLES ||
            uint64_t(count) * 4 > box.size - box.header - 8)
            return false;

        sync_samples.clear();
        sync_samples.reserve(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            unsigned char value[4]{};

            if (!read_bytes(file, data + uint64_t(i) * 4,
                            value, 4))
                return false;

            const uint32_t sample = u32(value);

            if (sample == 0 ||
                (!sync_samples.empty() &&
                 sample <= sync_samples.back()))
                return false;

            sync_samples.push_back(sample);
        }

        return true;
    }

    bool read_video_table_report(
        std::ifstream& file,
        const Box& trak,
        uint64_t file_size,
        std::ostringstream& out)
    {
        Box mdia{}, hdlr{}, minf{}, stbl{};
        Box stts{}, stsz{}, stsc{}, stco{}, co64{}, stss{};

        if (!child(file, trak, "mdia", mdia) ||
            !child(file, mdia, "hdlr", hdlr))
            return false;

        unsigned char handler[12]{};

        if (hdlr.size < hdlr.header + 12 ||
            !read_bytes(file, hdlr.offset + hdlr.header,
                        handler, 12))
            return false;

        if (std::string(
                reinterpret_cast<char*>(handler + 8), 4) != "vide")
            return true;

        if (!child(file, mdia, "minf", minf) ||
            !child(file, minf, "stbl", stbl))
            return false;

        if (!child(file, stbl, "stts", stts) ||
            !child(file, stbl, "stsz", stsz) ||
            !child(file, stbl, "stsc", stsc))
            return false;

        const bool has_stco = child(file, stbl, "stco", stco);
        const bool has_co64 = child(file, stbl, "co64", co64);

        if (has_stco == has_co64)
            return false;

        std::vector<uint32_t> sizes;
        std::vector<StscEntry> mappings;
        std::vector<uint64_t> chunks;
        std::vector<uint32_t> sync;

        uint64_t timing_samples = 0;
        uint64_t total_duration = 0;

        if (!parse_stsz(file, stsz, sizes) ||
            !parse_stts(file, stts, timing_samples, total_duration) ||
            !parse_stsc(file, stsc, mappings) ||
            !parse_chunk_offsets(
                file, has_co64 ? co64 : stco,
                chunks, has_co64))
            return false;

        if (sizes.size() != timing_samples)
            return false;

        const bool has_stss = child(file, stbl, "stss", stss);

        if (has_stss)
        {
            if (!parse_stss(file, stss, sync))
                return false;

            if (!sync.empty() && sync.back() > sizes.size())
                return false;
        }

        uint64_t sample_index = 0;
        uint64_t first_offset = 0;
        uint32_t first_size = 0;
        uint64_t valid_samples = 0;
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

            uint64_t current_offset = chunks[chunk_index];

            for (uint32_t j = 0;
                 j < mappings[mapping_index].samples_per_chunk; ++j)
            {
                if (sample_index >= sizes.size())
                    return false;

                const uint32_t sample_size =
                    sizes[static_cast<size_t>(sample_index)];

                if (current_offset > file_size ||
                    sample_size > file_size - current_offset)
                    return false;

                if (sample_index == 0)
                {
                    first_offset = current_offset;
                    first_size = sample_size;
                }

                ++valid_samples;

                if (sample_size >
                    std::numeric_limits<uint64_t>::max() - current_offset)
                    return false;

                current_offset += sample_size;
                ++sample_index;
            }
        }

        if (sample_index != sizes.size())
            return false;

        const double seconds =
            total_duration == 0 ? 0.0 :
            static_cast<double>(total_duration);

        out << "\nJRTE SAMPLE READER - PISTE VIDEO\n";
        out << "Echantillons : " << valid_samples << "\n";
        out << "Blocs (chunks) : " << chunks.size() << "\n";
        out << "Images cles : ";

        if (has_stss)
            out << sync.size() << "\n";
        else
            out << "table stss absente (tous les echantillons sont consideres cles)\n";

        out << "Duree en unites de temps : " << total_duration << "\n";
        out << "Premier echantillon : position " << first_offset
            << ", taille " << first_size << " octets\n";

        if (!sizes.empty())
        {
            out << "5 premiers echantillons :\n";

            uint64_t offset = first_offset;

            for (size_t i = 0; i < std::min<size_t>(5, sizes.size()); ++i)
            {
                out << "  #" << (i + 1)
                    << " : position " << offset
                    << ", taille " << sizes[i] << " octets\n";

                if (sizes[i] >
                    std::numeric_limits<uint64_t>::max() - offset)
                    break;

                offset += sizes[i];
            }
        }

        out << "Validation des tables : REUSSIE\n";

        return true;
    }

    bool scan_tracks(
        std::ifstream& file,
        uint64_t start,
        uint64_t end,
        unsigned depth,
        uint64_t file_size,
        std::ostringstream& out,
        uint32_t& video_tracks)
    {
        if (depth > 16)
            return false;

        uint64_t pos = start;

        while (pos < end)
        {
            Box box{};

            if (!read_box(file, pos, end, box))
                return false;

            if (box.type == "trak")
            {
                const auto before = out.tellp();

                if (!read_video_table_report(
                        file, box, file_size, out))
                    return false;

                if (out.tellp() != before)
                    ++video_tracks;
            }
            else if (box.type == "moov")
            {
                if (!scan_tracks(
                        file,
                        box.offset + box.header,
                        box.offset + box.size,
                        depth + 1,
                        file_size,
                        out,
                        video_tracks))
                    return false;
            }

            pos += box.size;
        }

        return pos == end;
    }
}

std::string jrte_sample_reader_report(
    const std::filesystem::path& path)
{
    std::ostringstream out;

    std::ifstream file(path, std::ios::binary);

    if (!file)
        return "\nJRTE SAMPLE READER : ouverture impossible\n";

    file.seekg(0, std::ios::end);
    const auto end = file.tellg();

    if (end < 0)
        return "\nJRTE SAMPLE READER : taille de fichier invalide\n";

    const uint64_t file_size = static_cast<uint64_t>(end);

    uint32_t video_tracks = 0;

    if (!scan_tracks(
            file, 0, file_size, 0,
            file_size, out, video_tracks))
        return "\nJRTE SAMPLE READER : tables absentes ou invalides\n";

    if (video_tracks == 0)
        return "\nJRTE SAMPLE READER : aucune piste video exploitable\n";

    return out.str();
}
