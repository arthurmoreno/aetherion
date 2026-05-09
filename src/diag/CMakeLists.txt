# src/diag/CMakeLists.txt — aetherion::diag (unified diagnostic module)

file(GLOB DIAG_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/diag/*.hpp"
)

file(GLOB DIAG_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/diag/*.cpp"
)

add_library(diag STATIC ${DIAG_SOURCES} ${DIAG_HEADERS})

set_target_properties(diag PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(diag PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(diag PUBLIC
    spdlog::spdlog
    fmt::fmt
)
