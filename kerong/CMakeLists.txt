find_package(Python COMPONENTS Interpreter Development REQUIRED)

if(DEFINED SKBUILD_PLATLIB_DIR)
    set(_kerong_out_dir "${SKBUILD_PLATLIB_DIR}/kerong")
else()
    set(_kerong_out_dir "${CMAKE_BINARY_DIR}/kerong")
endif()

if(NOT EXISTS "${_kerong_out_dir}")
    file(MAKE_DIRECTORY "${_kerong_out_dir}")
endif()

if(NOT TARGET kerong_core)
    find_package(kerong_core QUIET)
    if(NOT kerong_core_FOUND)
        message(FATAL_ERROR
            "kerong_core target not found. Build the static C++ core first "
            "(top-level CMakeLists.txt adds it via add_subdirectory(src)).")
    endif()
endif()

find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(_kerong MODULE _native.cpp)

target_link_libraries(_kerong PRIVATE kerong_core)

set_target_properties(_kerong PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${_kerong_out_dir}"
    RUNTIME_OUTPUT_DIRECTORY "${_kerong_out_dir}"
    ARCHIVE_OUTPUT_DIRECTORY "${_kerong_out_dir}"
)

if(WIN32)
    set_target_properties(_kerong PROPERTIES
        SUFFIX ".pyd"
    )
endif()
