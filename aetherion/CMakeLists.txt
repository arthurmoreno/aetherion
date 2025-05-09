cmake_minimum_required(VERSION 3.12)

# Intelligent conda environment detection
set(_conda_prefix "")
if(DEFINED ENV{CONDA_PREFIX})
    set(_conda_prefix "$ENV{CONDA_PREFIX}")
elseif(EXISTS "$ENV{HOME}/anaconda3/envs/lifesimcore-312")
    set(_conda_prefix "$ENV{HOME}/anaconda3/envs/lifesimcore-312")
endif()

if(_conda_prefix)
    list(APPEND CMAKE_PREFIX_PATH "${_conda_prefix}")
elseif(WIN32)
    message(WARNING "TODO: set CMAKE_PREFIX_PATH for Windows conda environment")
else()
    message(FATAL_ERROR "Could not detect a conda environment. Please set CONDA_PREFIX or ensure one of the default env paths exists.")
endif()

message(STATUS "▶ Forcing legacy FindPython to use Conda: $ENV{CONDA_PREFIX}/python.exe")

# Configure Python interpreter for FindPython and FindPython3
if(WIN32)
    set(Python_ROOT_DIR    "${_conda_prefix}" CACHE PATH     "Conda env root for Python" FORCE)
    set(Python_EXECUTABLE  "${_conda_prefix}/python.exe" CACHE FILEPATH "Path to Python executable" FORCE)
else()
    set(Python_ROOT_DIR    "${_conda_prefix}" CACHE PATH     "Conda env root for Python" FORCE)
    set(Python_EXECUTABLE  "${_conda_prefix}/bin/python" CACHE FILEPATH "Path to Python executable" FORCE)
endif()

# Align Python3 variables
set(Python3_ROOT_DIR     "${_conda_prefix}" CACHE PATH     "" FORCE)
set(Python3_EXECUTABLE   "${Python_EXECUTABLE}" CACHE FILEPATH "" FORCE)
message(STATUS "Using Python interpreter: ${Python_EXECUTABLE}")

set(NANOBIND_DIR "${CMAKE_SOURCE_DIR}/libs/nanobind")
set(nanobind_DIR "${CMAKE_SOURCE_DIR}/libs/nanobind/cmake")
include_directories(${NANOBIND_DIR}/include)
add_subdirectory(${NANOBIND_DIR})

project(AetherionProject LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

option(BUILD_WORLD_TEST "Build the world_test executable" ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

# Find packages
find_package(Python3 COMPONENTS Interpreter Development NumPy REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(nanobind CONFIG REQUIRED)
find_package(TBB REQUIRED)
find_package(msgpack REQUIRED)
find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LMDB REQUIRED lmdb)
find_package(SQLite3 REQUIRED)

# message(STATUS "SDL2 include dirs: ${SDL2_INCLUDE_DIRS}")
# message(STATUS "SDL2 libraries: ${SDL2_LIBRARIES}")
# message(STATUS "SDL2_image include dirs: ${SDL2_IMAGE_INCLUDE_DIRS}")
# message(STATUS "SDL2_image libraries: ${SDL2_IMAGE_LIBRARIES}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto -ffast-math")

# Enable colored output for GCC and Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fdiagnostics-color=always)
endif()
add_compile_definitions(ENTT_ENTITY_TYPE=int)
add_definitions(-DMDB_MAXKEYSIZE=0)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always -fvisibility=hidden")

# Manually set OpenVDB paths if CMake can't find it automatically
set(OPENVDB_INCLUDE_DIR /usr/local/include/openvdb)
set(OPENVDB_LIBRARIES /usr/local/lib/libopenvdb.so)

message(STATUS "Using OpenVDB include directory: ${OPENVDB_INCLUDE_DIR}")
message(STATUS "Using OpenVDB library: ${OPENVDB_LIBRARIES}")

# Add FlatBuffers directly to our build
set(FLATBUFFERS_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/flatbuffers)
add_subdirectory(${FLATBUFFERS_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/flatbuffers-build EXCLUDE_FROM_ALL)

# Include directories
include_directories(
    ${Python3_INCLUDE_DIRS}
    ${Python3_NumPy_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${OPENVDB_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${NANOBIND_DIR}/include
    ${MSGPACK_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/lmdb++
)

# Add FlatBuffers schema compilation
find_program(FLATC_COMPILER flatc REQUIRED)
if(NOT FLATC_COMPILER)
    message(FATAL_ERROR "flatc compiler not found. Please ensure it is installed and in your PATH.")
endif()

set(FLATBUFFERS_SCHEMAS
    ${CMAKE_SOURCE_DIR}/schemas/Components.fbs
    ${CMAKE_SOURCE_DIR}/schemas/Health.fbs
    ${CMAKE_SOURCE_DIR}/schemas/GridData.fbs
    ${CMAKE_SOURCE_DIR}/schemas/VoxelGridView.fbs
    ${CMAKE_SOURCE_DIR}/schemas/PhysicsComponents.fbs
    ${CMAKE_SOURCE_DIR}/schemas/PerceptionComponent.fbs
    ${CMAKE_SOURCE_DIR}/schemas/EntityTypeComponent.fbs
    ${CMAKE_SOURCE_DIR}/schemas/EntityInterface.fbs
    ${CMAKE_SOURCE_DIR}/schemas/WorldView.fbs
    ${CMAKE_SOURCE_DIR}/schemas/PerceptionResponse.fbs
)

set(FLATBUFFERS_GENERATED_CPP_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${FLATBUFFERS_GENERATED_CPP_DIR})

set(FLATBUFFERS_GENERATED_CPP_HEADERS)

foreach(schema ${FLATBUFFERS_SCHEMAS})
    get_filename_component(schema_name ${schema} NAME_WE)
    add_custom_command(
        OUTPUT ${FLATBUFFERS_GENERATED_CPP_DIR}/${schema_name}.h
        COMMAND ${FLATC_COMPILER} --cpp --gen-mutable -o ${FLATBUFFERS_GENERATED_CPP_DIR} ${schema}
        DEPENDS ${schema}
        COMMENT "Compiling FlatBuffers schema ${schema} to C++"
    )
    list(APPEND FLATBUFFERS_GENERATED_CPP_HEADERS ${FLATBUFFERS_GENERATED_CPP_DIR}/${schema_name}.h)
endforeach()

add_custom_target(GenerateFlatBuffers ALL DEPENDS ${FLATBUFFERS_GENERATED_CPP_HEADERS})

include_directories(${FLATBUFFERS_GENERATED_CPP_DIR})

# Dynamically search for all .cpp files except bindings.cpp and my_module.cpp
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/aetherion.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/my_module.cpp")


# Combine C++ and CUDA source files
list(
    APPEND SOURCES
)    

# Add subdirectories - project source files
add_subdirectory(src/components)
add_subdirectory(src/LowLevelRenderer)
# add_subdirectory(src/GameDB)
add_subdirectory(src/Gui)
add_subdirectory(src/neat)

# ------------------------------------------------------------------------------
# Build the Python Module (nanobind)
# ------------------------------------------------------------------------------

# Create the Python module using Nanobind
nanobind_add_module(_aetherion MODULE ${SOURCES} src/aetherion.cpp)

# Link libraries for the Python module
target_link_libraries(_aetherion PRIVATE
    components
    LowLevelRenderer
    Gui
    neat
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${OPENVDB_LIBRARIES}
    ${TBB_LIBRARIES}
    ${MSGPACK_LIBRARIES}
    ${Python3_LIBRARIES}
    tbb
    flatbuffers
    spdlog::spdlog
    ${LMDB_LIBRARIES}
    ${LMDBPP_LIBRARIES}
    SQLite::SQLite3
)

# Ensure that your targets depend on the generated code
add_dependencies(_aetherion GenerateFlatBuffers)

# Conditionally build world_test if the option is enabled
if(BUILD_WORLD_TEST)
    add_executable(world_test ${SOURCES} src/aetherion.cpp)
    target_link_libraries(world_test PRIVATE
        components
        LowLevelRenderer
        Gui
        ${SDL2_LIBRARIES}
        ${SDL2_IMAGE_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
        ${OPENVDB_LIBRARIES}
        ${TBB_LIBRARIES}
        ${MSGPACK_LIBRARIES}
        ${Python3_LIBRARIES}
        Eigen3::Eigen
        tbb
        flatbuffers
    )
    add_dependencies(world_test GenerateFlatBuffers)
    message(STATUS "world_test executable will be built")
else()
    message(STATUS "Skipping world_test executable")
endif()

# ------------------------------------------------------------------------------
# Install directives
# ------------------------------------------------------------------------------
# Install the compiled Python extension module into the neat3p package directory
install(TARGETS _aetherion LIBRARY DESTINATION neat3p)

# Install the umbrella header for C++ users.
# Adjust the path to neat3p.hpp if it's located somewhere else.
install(FILES ${CMAKE_SOURCE_DIR}/src/aetherion.hpp DESTINATION include/aetherion)

# ------------------------------------------------------------------------------
# Final Message
# ------------------------------------------------------------------------------
message(STATUS "Build Configuration for '_aetherion' Complete")