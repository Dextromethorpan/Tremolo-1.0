cmake_minimum_required(VERSION 3.15)
project(SmartTremolo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(smart_tremolo
    src/main.cpp
    src/Tremolo.cpp
    src/WavIO.cpp
)

target_include_directories(smart_tremolo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# # Put assets in runtime dir (helpful for IDE runs)
# add_custom_target(copy_assets ALL
#     COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#             ${CMAKE_SOURCE_DIR}/assets/input.wav
#             ${CMAKE_BINARY_DIR}/assets/input.wav
#     VERBATIM
# )
# add_dependencies(smart_tremolo copy_assets)
