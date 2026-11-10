#pragma once

#include <stdint.h>

#ifdef _WIN32
    #ifdef JRTE_ENGINE_EXPORTS
        #define JRTE_API __declspec(dllexport)
    #else
        #define JRTE_API __declspec(dllimport)
    #endif
#else
    #define JRTE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

JRTE_API int32_t jrte_mp4_inspect(
    const char* utf8_path,
    char* output,
    uint32_t output_capacity
);

JRTE_API int32_t jrte_mp4_extract_video_sample(
    const char* utf8_path,
    uint32_t sample_index,
    unsigned char* output,
    uint32_t output_capacity,
    uint32_t* bytes_written
);

JRTE_API int32_t jrte_mp4_get_video_sample_count(
    const char* utf8_path,
    uint32_t* sample_count
);
JRTE_API int32_t jrte_mp4_get_avc_configuration(
    const char* utf8_path,
    uint32_t sample_index,
    char* output,
    uint32_t output_capacity
);

JRTE_API int32_t jrte_mp4_analyze_video_sample(
    const char* utf8_path,
    uint32_t sample_index,
    char* output,
    uint32_t output_capacity
);
JRTE_API uint32_t jrte_get_api_version();

JRTE_API const char* jrte_get_engine_name();

JRTE_API int32_t jrte_initialize();

JRTE_API void jrte_shutdown();

JRTE_API int32_t jrte_mp4_analyze_all_video_samples(
    const char* utf8_path,
    char* output,
    uint32_t output_capacity
);
#ifdef __cplusplus
}
#endif


