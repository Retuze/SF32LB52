cmake_minimum_required(VERSION 3.24)
project(sf32lb52-watch C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_WARNINGS "Enable extra compiler warnings" ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/firmware.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/libraries.cmake)

# ---------------------------------------------------------------------------
# Discover and build firmware projects
#
# Each projects/<name>/ directory should contain:
#   project.cmake  — set(PROJECT_LIBS  hal bsp ...)
#   CMakeLists.txt — define the executable target, call add_firmware()
#
# Libraries declared in PROJECT_LIBS (and their transitive dependencies) are
# pulled into the build automatically.
# ---------------------------------------------------------------------------
file(GLOB _project_dirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/projects
     ${CMAKE_CURRENT_SOURCE_DIR}/projects/*)

foreach(_proj ${_project_dirs})
    set(_proj_dir    ${CMAKE_CURRENT_SOURCE_DIR}/projects/${_proj})
    set(_proj_config ${_proj_dir}/project.cmake)
    set(_proj_cmake  ${_proj_dir}/CMakeLists.txt)

    if(NOT IS_DIRECTORY "${_proj_dir}")
        continue()
    endif()
    if(NOT EXISTS "${_proj_cmake}")
        continue()
    endif()

    if(EXISTS "${_proj_config}")
        set(PROJECT_LIBS "")
        include(${_proj_config})

        foreach(_lib ${PROJECT_LIBS})
            sdk_require_library(${_lib})
        endforeach()
    endif()

    add_subdirectory(projects/${_proj})
endforeach()
