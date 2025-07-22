# Minimum required CMake version
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# Project definition
project(LogFold VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add optimization and static linking flags
if (NOT MSVC)
  add_compile_options(-O3 -static)
else()
  add_compile_options(/O2 /MT)
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Find required dependencies
find_package(PCRE2 REQUIRED 8BIT)

# Fetch Abseil from Git
include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON)
FetchContent_Declare(
  abseil
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20250512.1
)
FetchContent_MakeAvailable(abseil)

# Source files
file(GLOB_RECURSE SOURCES "src/**/*.cpp")
set(SOURCES ${SOURCES} src/main.cpp)

# Create the executable target
add_executable(${PROJECT_NAME} ${SOURCES})

# Link dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
  absl::flat_hash_map
  absl::flat_hash_set
  PCRE2::8BIT
)