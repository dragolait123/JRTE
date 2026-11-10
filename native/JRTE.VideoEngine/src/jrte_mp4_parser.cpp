#include "jrte_api.h"
#include "jrte_sample_reader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

namespace
{
    constexpr int32_t OK = 0;
    constexpr int32_t INVALID_ARGUMENT = -1;
    constexpr int32_t FILE_OPEN_ERROR = -2;
    constexpr int32_t NOT_MP4 = -3;
    constexpr int32_t MALFORMED_FILE = -4;
    constexpr int32_t OUTPUT_TOO_SMALL = -5;

    constexpr uint64_t MAX_BOXES = 100000;
    constexpr unsigned MAX_DEPTH = 16;

    struct Box
    {
        uint64_t offset = 0;
        uint64_t size = 0;
        uint64_t header_size = 0;
        std::string type;
    };

    struct TrackInfo
    {
        std::string handler = "inconnu";
        std::string codec = "inconnu";
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t timescale = 0;
        uint64_t duration = 0;
        uint32_t sample_rate = 0;
        uint16_t channels = 0;
    };

    struct ParserState
    {
        uint64_t box_count = 0;
        uint64_t track_count = 0;
        uint64_t mdat_count = 0;
        uint64_t moov_count = 0;
        uint64_t ftyp_count = 0;
        std::string major_brand = "inconnue";
        std::vector<TrackInfo> tracks;
    };

    uint32_t read_u32(const unsigned char* p)
    {
        return (uint32_t(p[0]) << 24) |
               (uint32_t(p[1]) << 16) |
               (uint32_t(p[2]) << 8) |
               uint32_t(p[3]);
    }

    uint64_t read_u64(const unsigned char* p)
    {
        return (uint64_t(read_u32(p)) << 32) |
               uint64_t(read_u32(p + 4));
    }

    uint16_t read_u16(const unsigned char* p)
    {
        return uint16_t((uint16_t(p[0]) << 8) | p[1]);
    }

    bool read_at(
        std::ifstream& file,
        uint64_t offset,
        unsigned char* buffer,
        size_t length)
    {
        if (offset > uint64_t(std::numeric_limits<std::streamoff>::max()))
            return false;

        file.clear();
        file.seekg(static_cast<std::streamoff>(offset));

        if (!file)
            return false;

        file.read(reinterpret_cast<char*>(buffer),
                  static_cast<std::streamsize>(length));

        return file.gcount() == static_cast<std::streamsize>(length);
    }

    bool read_box(
        std::ifstream& file,
        uint64_t offset,
        uint64_t parent_end,
        Box& box)
    {
        if (offset > parent_end || parent_end - offset < 8)
            return false;

        unsigned char header[16]{};

        if (!read_at(file, offset, header, 8))
            return false;

        uint64_t size = read_u32(header);
        uint64_t header_size = 8;

        box.type.assign(reinterpret_cast<const char*>(header + 4), 4);

        if (size == 1)
        {
            if (parent_end - offset < 16 ||
                !read_at(file, offset, header, 16))
                return false;

            size = read_u64(header + 8);
            header_size = 16;
        }
        else if (size == 0)
        {
            size = parent_end - offset;
        }

        if (size < header_size || size > parent_end - offset)
            return false;

        box.offset = offset;
        box.size = size;
        box.header_size = header_size;

        return true;
    }

    bool find_child(
        std::ifstream& file,
        const Box& parent,
        const std::string& wanted,
        Box& result)
    {
        uint64_t position = parent.offset + parent.header_size;
        const uint64_t end = parent.offset + parent.size;

        while (position < end)
        {
            Box child{};

            if (!read_box(file, position, end, child))
                return false;

            if (child.type == wanted)
            {
                result = child;
                return true;
            }

            position += child.size;
        }

        return false;
    }

    bool read_track_metadata(
        std::ifstream& file,
        const Box& trak,
        TrackInfo& track)
    {
        Box mdia{}, hdlr{}, mdhd{}, minf{}, stbl{}, stsd{};

        if (!find_child(file, trak, "mdia", mdia))
            return false;

        if (find_child(file, mdia, "hdlr", hdlr))
        {
            unsigned char data[12]{};

            if (hdlr.size >= hdlr.header_size + 12 &&
                read_at(file, hdlr.offset + hdlr.header_size, data, 12))
            {
                track.handler.assign(
                    reinterpret_cast<const char*>(data + 8), 4);
            }
        }

        if (find_child(file, mdia, "mdhd", mdhd))
        {
            unsigned char version_data[1]{};

            if (!read_at(file, mdhd.offset + mdhd.header_size,
                         version_data, 1))
                return false;

            if (version_data[0] == 0)
            {
                unsigned char data[20]{};

                if (mdhd.size < mdhd.header_size + 20 ||
                    !read_at(file, mdhd.offset + mdhd.header_size,
                             data, 20))
                    return false;

                track.timescale = read_u32(data + 12);
                track.duration = read_u32(data + 16);
            }
            else if (version_data[0] == 1)
            {
                unsigned char data[32]{};

                if (mdhd.size < mdhd.header_size + 32 ||
                    !read_at(file, mdhd.offset + mdhd.header_size,
                             data, 32))
                    return false;

                track.timescale = read_u32(data + 20);
                track.duration = read_u64(data + 24);
            }
        }

        if (!find_child(file, mdia, "minf", minf) ||
            !find_child(file, minf, "stbl", stbl) ||
            !find_child(file, stbl, "stsd", stsd))
            return true;

        const uint64_t payload = stsd.offset + stsd.header_size;

        unsigned char stsd_header[8]{};

        if (stsd.size < stsd.header_size + 8 ||
            !read_at(file, payload, stsd_header, 8))
            return false;

        const uint32_t entry_count = read_u32(stsd_header + 4);

        if (entry_count == 0)
            return true;

        Box entry{};

        if (!read_box(file, payload + 8,
                      stsd.offset + stsd.size, entry))
            return false;

        track.codec = entry.type;

        const uint64_t sample = entry.offset + entry.header_size;

        if (track.handler == "vide" &&
            entry.size >= entry.header_size + 28)
        {
            unsigned char video_data[28]{};

            if (!read_at(file, sample, video_data, 28))
                return false;

            track.width = read_u16(video_data + 24);
            track.height = read_u16(video_data + 26);
        }
        else if (track.handler == "soun" &&
                 entry.size >= entry.header_size + 28)
        {
            unsigned char audio_data[28]{};

            if (!read_at(file, sample, audio_data, 28))
                return false;

            track.channels = read_u16(audio_data + 16);

            const uint32_t fixed_rate = read_u32(audio_data + 24);
            track.sample_rate = fixed_rate >> 16;
        }

        return true;
    }

    bool is_container(const std::string& type)
    {
        return type == "moov" || type == "trak" ||
               type == "mdia" || type == "minf" ||
               type == "stbl" || type == "edts" ||
               type == "dinf" || type == "mvex" ||
               type == "moof" || type == "traf" ||
               type == "mfra" || type == "udta";
    }

    bool parse_boxes(
        std::ifstream& file,
        uint64_t start,
        uint64_t end,
        unsigned depth,
        ParserState& state)
    {
        if (depth > MAX_DEPTH || start > end)
            return false;

        uint64_t position = start;

        while (position < end)
        {
            if (++state.box_count > MAX_BOXES)
                return false;

            Box box{};

            if (!read_box(file, position, end, box))
                return false;

            if (box.type == "ftyp")
            {
                if (box.size < box.header_size + 8)
                    return false;

                unsigned char brand[4]{};

                if (!read_at(file, box.offset + box.header_size,
                             brand, 4))
                    return false;

                state.major_brand.assign(
                    reinterpret_cast<const char*>(brand), 4);

                ++state.ftyp_count;
            }
            else if (box.type == "moov")
            {
                ++state.moov_count;
            }
            else if (box.type == "trak")
            {
                ++state.track_count;

                TrackInfo track{};

                if (!read_track_metadata(file, box, track))
                    return false;

                state.tracks.push_back(track);
            }
            else if (box.type == "mdat")
            {
                ++state.mdat_count;
            }

            if (is_container(box.type))
            {
                const uint64_t child_start =
                    box.offset + box.header_size;

                const uint64_t child_end =
                    box.offset + box.size;

                if (!parse_boxes(file, child_start, child_end,
                                 depth + 1, state))
                    return false;
            }

            position = box.offset + box.size;
        }

        return position == end;
    }

    std::string format_duration(const TrackInfo& track)
    {
        if (track.timescale == 0)
            return "inconnue";

        const double seconds =
            static_cast<double>(track.duration) /
            static_cast<double>(track.timescale);

        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << seconds << " s";
        return out.str();
    }
}

extern "C"
{
    JRTE_API int32_t jrte_mp4_inspect(
        const char* utf8_path,
        char* output,
        uint32_t output_capacity)
    {
        if (!utf8_path || utf8_path[0] == '\0' ||
            !output || output_capacity == 0)
            return INVALID_ARGUMENT;

        output[0] = '\0';

        try
        {
            auto path = std::filesystem::u8path(utf8_path);
            std::ifstream file(path, std::ios::binary);

            if (!file.is_open())
                return FILE_OPEN_ERROR;

            file.seekg(0, std::ios::end);
            const auto end_position = file.tellg();

            if (end_position < 0)
                return MALFORMED_FILE;

            const uint64_t file_size =
                static_cast<uint64_t>(end_position);

            if (file_size < 16)
                return NOT_MP4;

            ParserState state;

            if (!parse_boxes(file, 0, file_size, 0, state))
                return MALFORMED_FILE;

            if (state.ftyp_count == 0 || state.moov_count == 0)
                return NOT_MP4;

            std::ostringstream report;

            report << "JRTE VIDEO ENGINE - MEDIA REPORT\n";
            report << "Taille : " << file_size << " octets\n";
            report << "Marque : " << state.major_brand << "\n";
            report << "Boites analysees : " << state.box_count << "\n";
            report << "Pistes detectees : " << state.track_count << "\n\n";

            for (size_t i = 0; i < state.tracks.size(); ++i)
            {
                const auto& track = state.tracks[i];

                report << "PISTE " << (i + 1) << "\n";

                if (track.handler == "vide")
                    report << "Type : VIDEO\n";
                else if (track.handler == "soun")
                    report << "Type : AUDIO\n";
                else
                    report << "Type : " << track.handler << "\n";

                report << "Codec declare : " << track.codec << "\n";
                report << "Duree : " << format_duration(track) << "\n";

                if (track.handler == "vide")
                {
                    report << "Resolution : "
                           << track.width << " x "
                           << track.height << "\n";
                }
                else if (track.handler == "soun")
                {
                    report << "Frequence : "
                           << track.sample_rate << " Hz\n";
                    report << "Canaux : " << track.channels << "\n";
                }

                report << "\n";
            }

            report << "Structure MP4 : reconnue\n";
            report << "Analyse des pistes : terminee\n";
            report << jrte_sample_reader_report(path);

            const std::string result = report.str();

            if (result.size() + 1 > output_capacity)
                return OUTPUT_TOO_SMALL;

            std::memcpy(output, result.c_str(), result.size() + 1);

            return OK;
        }
        catch (...)
        {
            return MALFORMED_FILE;
        }
    }
}

