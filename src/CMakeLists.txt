cmake_minimum_required(VERSION 2.8)

add_definitions(-fPIC)

# Source files for ti_features plugin
set(SRC
    ti_features.cpp
    ti_features.h
    feature_config.cpp
    feature_config.h
    feature_stats.h
    calc_basic.cpp
    calc_basic.h
    calc_iat.cpp
    calc_iat.h
    calc_burst.cpp
    calc_burst.h
    calc_freq_domain.cpp
    calc_freq_domain.h
    calc_sequence.cpp
    calc_sequence.h
    calc_payload.cpp
    calc_payload.h
    calc_protocol.cpp
    calc_protocol.h
    calc_behavior.cpp
    calc_behavior.h
    calc_window.cpp
    calc_window.h
    utils/circular_buffer.h
    utils/circular_buffer.cpp
    utils/fft.h
    utils/fft.cpp
    utils/stats_utils.h
    KafkaProducer.cpp
    KafkaProducer.h
)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/inc)
include_directories(${CMAKE_SOURCE_DIR}/inc_for_refer)
include_directories(/opt/MESA/include/)
include_directories(/opt/MESA/include/MESA)

# 链接 zlib 用于压缩计算
set(DEPEND_DYN_LIB MESA_handle_logger MESA_prof_load cjson rdkafka z)

add_library(${plugin_name} SHARED ${SRC})
target_link_libraries(${plugin_name} ${DEPEND_DYN_LIB})
set_target_properties(${plugin_name} PROPERTIES PREFIX "")

install(TARGETS ${plugin_name} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plug/business/${plugin_name} COMPONENT LIBRARIES)
install(FILES ../bin/${plugin_name}.inf DESTINATION ${CMAKE_INSTALL_PREFIX}/plug/business/${plugin_name} COMPONENT PROFILE)
install(FILES ../bin/${plugin_name}.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/ticonf COMPONENT PROFILE)
