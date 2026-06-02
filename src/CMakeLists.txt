add_library(kerong_core STATIC
    kerong_client.cpp
)

set_target_properties(kerong_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

add_library(kerong::core ALIAS kerong_core)

target_include_directories(kerong_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(kerong_core PUBLIC cxx_std_17)

if(WIN32)
    target_link_libraries(kerong_core PUBLIC ws2_32)
    target_compile_definitions(kerong_core PRIVATE _WIN32_WINNT=0x0601)
endif()
