find_package(pybind11 QUIET)

if(NOT pybind11_FOUND)
    if(DEFINED pybind11_DIR AND EXISTS "${pybind11_DIR}")
        find_package(pybind11 CONFIG REQUIRED PATHS "${pybind11_DIR}" NO_DEFAULT_PATH)
    elseif(DEFINED ENV{pybind11_DIR} AND EXISTS "$ENV{pybind11_DIR}")
        find_package(pybind11 CONFIG REQUIRED PATHS "$ENV{pybind11_DIR}" NO_DEFAULT_PATH)
    else()
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c "import pybind11; print(pybind11.get_cmake_dir())"
            OUTPUT_VARIABLE pybind11_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _pybind11_rc
        )
        if(NOT _pybind11_rc EQUAL 0)
            message(FATAL_ERROR
                "pybind11 not found. Install it with `pip install pybind11` or set pybind11_DIR.")
        endif()
        find_package(pybind11 CONFIG REQUIRED PATHS "${pybind11_DIR}" NO_DEFAULT_PATH)
    endif()
endif()

pybind11_add_module(kerong MODULE py_kerong.cpp)

target_link_libraries(kerong PRIVATE kerong_core)
target_compile_features(kerong PUBLIC cxx_std_17)

set_target_properties(kerong PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bindings/python"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bindings/python"
)
