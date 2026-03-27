# src/items/CMakeLists.txt

# Define items sources and headers

file(GLOB ITEMS_HEADERS
    "${CMAKE_SOURCE_DIR}/src/items/*.hpp"
)

file(GLOB ITEMS_SOURCES "${CMAKE_SOURCE_DIR}/src/items/*.cpp")

# Create library for items
add_library(items STATIC ${ITEMS_SOURCES} ${ITEMS_HEADERS})

# Ensure the library is built with PIC
set_target_properties(items PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Specify include directories for items
target_include_directories(items PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Link dependencies for items
target_link_libraries(items PUBLIC
    components
)