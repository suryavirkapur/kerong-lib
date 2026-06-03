find_path(NODE_ADDON_API_INCLUDE_DIR
    NAMES napi.h
    PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api
        ${CMAKE_CURRENT_SOURCE_DIR}/../node_modules/node-addon-api
    NO_DEFAULT_PATH
)

if(NOT NODE_ADDON_API_INCLUDE_DIR)
    message(FATAL_ERROR
        "node-addon-api headers not found. Run `npm install` in the node/ "
        "directory first.")
endif()

if(DEFINED NODE_INCLUDE_DIRS)
    set(_node_include_dirs ${NODE_INCLUDE_DIRS})
else()
    find_path(NODE_INCLUDE_DIR
        NAMES node_api.h
        PATHS
            ${NODE_ROOT}/include/node
            /usr/include/node
            /usr/local/include/node
    )
    if(NOT NODE_INCLUDE_DIR)
        execute_process(
            COMMAND node -p "require('node-addon-api').include"
            OUTPUT_VARIABLE NODE_INCLUDE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _naa_rc
        )
        if(NOT _naa_rc EQUAL 0 OR NOT NODE_INCLUDE_DIR)
            execute_process(
                COMMAND node -e "console.log(require('node-gyp').include_dir || '')"
                OUTPUT_VARIABLE NODE_INCLUDE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        endif()
    endif()
    if(NOT NODE_INCLUDE_DIR OR NOT EXISTS "${NODE_INCLUDE_DIR}/node_api.h")
        message(FATAL_ERROR
            "Could not locate Node.js headers (node_api.h). "
            "Set NODE_INCLUDE_DIR or NODE_ROOT to point at the Node.js install.")
    endif()
    set(_node_include_dirs ${NODE_INCLUDE_DIR})
endif()

add_library(kerong_node SHARED kerong_addon.cpp)

set_target_properties(kerong_node PROPERTIES
    OUTPUT_NAME "kerong"
)

target_include_directories(kerong_node PRIVATE
    ${NODE_ADDON_API_INCLUDE_DIR}
    ${_node_include_dirs}
)

target_link_libraries(kerong_node PRIVATE kerong_core)

target_compile_features(kerong_node PUBLIC cxx_std_17)
target_compile_definitions(kerong_node PRIVATE
    NAPI_CPP_EXCEPTIONS
    NAPI_VERSION=8
)

set_target_properties(kerong_node PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/node"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/node"
)

if(WIN32)
    set_target_properties(kerong_node PROPERTIES
        SUFFIX ".node"
    )
elseif(APPLE)
    set_target_properties(kerong_node PROPERTIES
        SUFFIX ".node"
        LINK_FLAGS "-undefined dynamic_lookup"
    )
else()
    set_target_properties(kerong_node PROPERTIES
        SUFFIX ".node"
    )
endif()
