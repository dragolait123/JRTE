#include "jrte_api.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;

static void u32(Bytes& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

static void u64(Bytes& out, uint64_t value)
{
    u32(out, static_cast<uint32_t>(value >> 32));
    u32(out, static_cast<uint32_t>(value));
}

static Bytes make_box(const char* type, const Bytes& payload)
{
    Bytes result;
    u32(result, static_cast<uint32_t>(payload.size() + 8));

    for (int i = 0; i < 4; ++i)
        result.push_back(static_cast<uint8_t>(type[i]));

    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

static Bytes join(const std::vector<Bytes>& boxes)
{
    Bytes result;

    for (const auto& box : boxes)
        result.insert(result.end(), box.begin(), box.end());

    return result;
}

static Bytes make_ftyp()
{
    Bytes payload = {
        'i', 's', 'o', 'm', 0, 0, 0, 0,
        'i', 's', 'o', 'm', 'm', 'p', '4', '2'
    };

    return make_box("ftyp", payload);
}

static Bytes make_mdhd(bool version1)
{
    Bytes payload;

    if (!version1)
    {
        // FullBox version 0
        u32(payload, 0);
        u32(payload, 0);       // creation_time
        u32(payload, 0);       // modification_time
        u32(payload, 1000);    // timescale
        u32(payload, 80);      // duration
        payload.push_back(0x55);
        payload.push_back(0xC4);
        payload.push_back(0);
        payload.push_back(0);
    }
    else
    {
        // FullBox version 1
        u32(payload, 0x01000000);
        u64(payload, 0);       // creation_time
        u64(payload, 0);       // modification_time
        u32(payload, 1000);    // timescale
        u64(payload, 80);      // duration
        payload.push_back(0x55);
        payload.push_back(0xC4);
        payload.push_back(0);
        payload.push_back(0);
    }

    return make_box("mdhd", payload);
}

static Bytes make_stts()
{
    Bytes payload = {0, 0, 0, 0};

    u32(payload, 1);  // entry_count
    u32(payload, 2);  // sample_count
    u32(payload, 40); // sample_delta

    return make_box("stts", payload);
}

static Bytes make_ctts(bool version1, uint32_t positive_offset = 40)
{
    Bytes payload;

    payload.push_back(version1 ? 1 : 0);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);

    u32(payload, 2);

    u32(payload, 1);
    u32(payload, 0);

    u32(payload, 1);

    if (version1)
        u32(payload, 0xFFFFFFF6); // -10, signed offset
    else
        u32(payload, positive_offset); // unsigned offset

    return make_box("ctts", payload);
}

static Bytes make_stss()
{
    Bytes payload = {0, 0, 0, 0};

    u32(payload, 1);
    u32(payload, 1); // sample 1 is a keyframe

    return make_box("stss", payload);
}

static Bytes make_moov(
    uint64_t first_offset,
    uint64_t second_offset,
    bool use_co64,
    bool mdhd_version1,
    bool ctts_version1, uint32_t positive_offset = 40)
{
    Bytes hdlr_payload = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        'v', 'i', 'd', 'e',
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0
    };

    Bytes stsz_payload = {0, 0, 0, 0};
    u32(stsz_payload, 0);
    u32(stsz_payload, 2);
    u32(stsz_payload, 3);
    u32(stsz_payload, 2);

    Bytes stsc_payload = {0, 0, 0, 0};
    u32(stsc_payload, 1);
    u32(stsc_payload, 1);
    u32(stsc_payload, 1);
    u32(stsc_payload, 1);

    Bytes offsets_payload = {0, 0, 0, 0};
    u32(offsets_payload, 2);

    if (use_co64)
    {
        u64(offsets_payload, first_offset);
        u64(offsets_payload, second_offset);
    }
    else
    {
        assert(first_offset <= UINT32_MAX);
        assert(second_offset <= UINT32_MAX);

        u32(offsets_payload, static_cast<uint32_t>(first_offset));
        u32(offsets_payload, static_cast<uint32_t>(second_offset));
    }

    const Bytes stsz = make_box("stsz", stsz_payload);
    const Bytes stsc = make_box("stsc", stsc_payload);

    const Bytes offsets = make_box(
        use_co64 ? "co64" : "stco",
        offsets_payload
    );

    const Bytes stts = make_stts();
    const Bytes ctts = make_ctts(ctts_version1, positive_offset);
    const Bytes stss = make_stss();

    const Bytes stbl = make_box(
        "stbl",
        join({stts, ctts, stss, stsz, stsc, offsets})
    );

    const Bytes minf = make_box("minf", stbl);
    const Bytes hdlr = make_box("hdlr", hdlr_payload);
    const Bytes mdhd = make_mdhd(mdhd_version1);

    const Bytes mdia = make_box(
        "mdia",
        join({mdhd, hdlr, minf})
    );

    const Bytes trak = make_box("trak", mdia);

    return make_box("moov", trak);
}

static Bytes make_video_mp4(
    bool use_co64,
    bool mdhd_version1,
    bool ctts_version1, uint32_t positive_offset = 40)
{
    const Bytes ftyp = make_ftyp();

    const Bytes mdat_payload = {
        0x11, 0x22, 0x33,
        0xEE,
        0xAA, 0xBB
    };

    const Bytes mdat = make_box("mdat", mdat_payload);

    const Bytes temporary_moov = make_moov(
        0, 0, use_co64, mdhd_version1, ctts_version1, positive_offset
    );

    const uint64_t first_offset =
        ftyp.size() + temporary_moov.size() + 8;

    const uint64_t second_offset = first_offset + 4;

    const Bytes moov = make_moov(
        first_offset,
        second_offset,
        use_co64,
        mdhd_version1,
        ctts_version1,
        positive_offset
    );

    return join({ftyp, moov, mdat});
}

static void run_fixture(
    const std::filesystem::path& path,
    bool use_co64,
    bool mdhd_version1,
    bool ctts_version1, uint32_t positive_offset = 40)
{
    const Bytes fixture = make_video_mp4(
        use_co64, mdhd_version1, ctts_version1, positive_offset
    );

    {
        std::ofstream file(path, std::ios::binary);
        assert(file.good());

        file.write(
            reinterpret_cast<const char*>(fixture.data()),
            static_cast<std::streamsize>(fixture.size())
        );

        assert(file.good());
    }

    const std::string file_path = path.string();

    uint32_t sample_count = 0;

    const int32_t count_status =
        jrte_mp4_get_video_sample_count(
            file_path.c_str(),
            &sample_count
        );

    assert(count_status == 0);
    assert(sample_count == 2);

    unsigned char output[16] = {};
    uint32_t bytes_written = 0;

    const int32_t first_status =
        jrte_mp4_extract_video_sample(
            file_path.c_str(),
            0,
            output,
            sizeof(output),
            &bytes_written
        );

    assert(first_status == 0);
    assert(bytes_written == 3);
    assert(output[0] == 0x11);
    assert(output[1] == 0x22);
    assert(output[2] == 0x33);

    bytes_written = 0;

    const int32_t second_status =
        jrte_mp4_extract_video_sample(
            file_path.c_str(),
            1,
            output,
            sizeof(output),
            &bytes_written
        );

    assert(second_status == 0);
    assert(bytes_written == 2);
    assert(output[0] == 0xAA);
    assert(output[1] == 0xBB);

    bytes_written = 0;

    const int32_t small_buffer_status =
        jrte_mp4_extract_video_sample(
            file_path.c_str(),
            0,
            output,
            2,
            &bytes_written
        );

    assert(small_buffer_status == -6);
    assert(bytes_written == 0);

    bytes_written = 0;

    const int32_t invalid_index_status =
        jrte_mp4_extract_video_sample(
            file_path.c_str(),
            2,
            output,
            sizeof(output),
            &bytes_written
        );

    assert(invalid_index_status == -5);

    // Verifier le rapport Sample Reader via l'API publique.
    char report[8192] = {};

    const int32_t inspect_status =
        jrte_mp4_inspect(
            file_path.c_str(),
            report,
            sizeof(report)
        );

    assert(inspect_status == 0);

    const std::string text(report);

    assert(text.find("Timescale : 1000") != std::string::npos);
    assert(text.find("DTS : 0") != std::string::npos);
    assert(text.find("DTS : 40") != std::string::npos);
    assert(text.find("Keyframe : oui") != std::string::npos);
    assert(text.find("Keyframe : non") != std::string::npos);

    if (ctts_version1)
    {
        assert(text.find("PTS : 30") != std::string::npos);
    }
    else
    {
        const int64_t expected_pts =
            40 + static_cast<int64_t>(positive_offset);
        assert(text.find("PTS : " + std::to_string(expected_pts)) != std::string::npos);
    }

    std::filesystem::remove(path);

    std::cout
        << "[OK] "
        << (use_co64 ? "co64" : "stco")
        << " | mdhd v" << (mdhd_version1 ? 1 : 0)
        << " | ctts v" << (ctts_version1 ? 1 : 0)
        << " | DTS/PTS/keyframes/extraction\n";
}

int main()
{
    const auto directory =
        std::filesystem::temp_directory_path();

    run_fixture(
        directory / "jrte_mp4_timestamp_1.mp4",
        false, false, false
    );

    run_fixture(
        directory / "jrte_mp4_timestamp_2.mp4",
        false, true, true
    );

    run_fixture(
        directory / "jrte_mp4_timestamp_3.mp4",
        true, false, true
    );

    run_fixture(
        directory / "jrte_mp4_timestamp_4.mp4",
        true, true, false
    );

    run_fixture(
        directory / "jrte_mp4_timestamp_5.mp4",
        false, false, false, 0x80000000u
    );

    std::cout << "[SUCCESS] All MP4 timestamp tests passed\n";
    return 0;
}