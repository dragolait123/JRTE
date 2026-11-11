cmake_minimum_required(VERSION 3.20)

project(JRTE.VideoEngine
    VERSION 4.5.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(JRTE.VideoEngine SHARED
    src/jrte_engine.cpp
    src/jrte_mp4_parser.cpp
    src/jrte_sample_reader.cpp
    src/jrte_sample_extract.cpp
    src/jrte_h264_analyzer.cpp
    src/jrte_bit_reader.cpp
    src/jrte_sps_parser.cpp
    src/jrte_pps_parser.cpp
    src/jrte_cabac_decoder.cpp
    src/jrte_slice_parser.cpp
)

target_include_directories(JRTE.VideoEngine
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(JRTE.VideoEngine
    PRIVATE JRTE_ENGINE_EXPORTS
)

set_target_properties(JRTE.VideoEngine PROPERTIES
    OUTPUT_NAME "JRTE.VideoEngine"
    PREFIX ""
)




enable_testing()

add_executable(jrte_sps_parser_test
    tests/jrte_sps_parser_test.cpp
    src/jrte_sps_parser.cpp
    src/jrte_pps_parser.cpp
    src/jrte_slice_parser.cpp
    src/jrte_bit_reader.cpp
)

target_include_directories(jrte_sps_parser_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_SPS_Parser_Test
    COMMAND jrte_sps_parser_test
)

add_executable(jrte_h264_integration_test
    tests/jrte_h264_integration_test.cpp
    src/jrte_h264_analyzer.cpp
    src/jrte_sps_parser.cpp
    src/jrte_pps_parser.cpp
    src/jrte_slice_parser.cpp
    src/jrte_bit_reader.cpp
)

target_include_directories(jrte_h264_integration_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_H264_Integration_Test
    COMMAND jrte_h264_integration_test
)

add_executable(jrte_pps_parser_test
    tests/jrte_pps_parser_test.cpp
    src/jrte_pps_parser.cpp
    src/jrte_slice_parser.cpp
    src/jrte_bit_reader.cpp
)

target_include_directories(jrte_pps_parser_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_PPS_Parser_Test
    COMMAND jrte_pps_parser_test
)


add_executable(jrte_slice_parser_test
    tests/jrte_slice_parser_test.cpp
    src/jrte_slice_parser.cpp
    src/jrte_bit_reader.cpp
)

target_include_directories(jrte_slice_parser_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_Slice_Parser_Test
    COMMAND jrte_slice_parser_test
)

add_executable(jrte_bit_reader_test
    tests/jrte_bit_reader_test.cpp
    src/jrte_bit_reader.cpp
)

target_include_directories(jrte_bit_reader_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_Bit_Reader_Test
    COMMAND jrte_bit_reader_test
)



add_executable(jrte_cabac_decoder_test
    tests/jrte_cabac_decoder_test.cpp
    src/jrte_cabac_decoder.cpp
)

target_include_directories(jrte_cabac_decoder_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_CABAC_Decoder_Test
    COMMAND jrte_cabac_decoder_test
)
