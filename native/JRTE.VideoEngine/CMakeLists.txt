cmake_minimum_required(VERSION 3.20)

project(JRTE.VideoEngine
    VERSION 1.0.0
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
    src/jrte_slice_parser.cpp
    src/jrte_cavlc_decoder.cpp
    src/jrte_macroblock_decoder.cpp
    src/jrte_block_coeff_context.cpp
    src/jrte_inverse_transform.cpp
    src/jrte_inverse_quant.cpp
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
    src/jrte_cavlc_decoder.cpp
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
    src/jrte_cavlc_decoder.cpp
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
    src/jrte_cavlc_decoder.cpp
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
    src/jrte_cavlc_decoder.cpp
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

add_executable(jrte_cavlc_decoder_test
    tests/jrte_cavlc_decoder_test.cpp
    src/jrte_cavlc_decoder.cpp
    src/jrte_bit_reader.cpp
)

target_include_directories(jrte_cavlc_decoder_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_CAVLC_Decoder_Test
    COMMAND jrte_cavlc_decoder_test
)

add_executable(jrte_mp4_demux_test
    tests/jrte_mp4_demux_test.cpp
)

target_include_directories(jrte_mp4_demux_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(jrte_mp4_demux_test
    PRIVATE JRTE.VideoEngine
)

add_test(
    NAME JRTE_MP4_Demux_Test
    COMMAND jrte_mp4_demux_test
)


add_executable(jrte_macroblock_decoder_test
    tests/jrte_macroblock_decoder_test.cpp
    src/jrte_macroblock_decoder.cpp
    src/jrte_cavlc_decoder.cpp
    src/jrte_bit_reader.cpp
)

target_include_directories(jrte_macroblock_decoder_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_Macroblock_Decoder_Test
    COMMAND jrte_macroblock_decoder_test
)


add_executable(jrte_block_coeff_context_test
    tests/jrte_block_coeff_context_test.cpp
    src/jrte_block_coeff_context.cpp
    src/jrte_inverse_transform.cpp
    src/jrte_inverse_quant.cpp
    src/jrte_macroblock_decoder.cpp
    src/jrte_cavlc_decoder.cpp
    src/jrte_bit_reader.cpp
)

target_include_directories(jrte_block_coeff_context_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_Block_Coeff_Context_Test
    COMMAND jrte_block_coeff_context_test
)



add_executable(jrte_inverse_transform_test
    tests/jrte_inverse_transform_test.cpp
    src/jrte_inverse_transform.cpp
    src/jrte_inverse_quant.cpp
)

target_include_directories(jrte_inverse_transform_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_Inverse_Transform_Test
    COMMAND jrte_inverse_transform_test
)

add_executable(jrte_inverse_quant_test
    tests/jrte_inverse_quant_test.cpp
    src/jrte_inverse_quant.cpp
)

target_include_directories(jrte_inverse_quant_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(
    NAME JRTE_Inverse_Quant_Test
    COMMAND jrte_inverse_quant_test
)

