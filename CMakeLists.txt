cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_C_COMPILER AND "$ENV{CC}" STREQUAL "")
    set(CMAKE_C_COMPILER /usr/bin/gcc)
endif()
project(inkstorm LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(_DEFAULT_SOURCE)
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

set(RAYLIB_DIR "" CACHE PATH "raylib 6.2 installation prefix")
if(RAYLIB_DIR STREQUAL "")
    set(RAYLIB_DIR "$ENV{RAYLIB_DIR}")
endif()
if(RAYLIB_DIR STREQUAL "")
    set(RAYLIB_DIR "$ENV{HOME}/.local/opt/raylib-6.2")
endif()
if(NOT EXISTS "${RAYLIB_DIR}/include/raylib.h" OR NOT EXISTS "${RAYLIB_DIR}/lib/libraylib.a")
    message(FATAL_ERROR "raylib 6.2 not found at '${RAYLIB_DIR}'. Set RAYLIB_DIR to its install prefix.")
endif()
add_library(raylib STATIC IMPORTED)
set_target_properties(raylib PROPERTIES IMPORTED_LOCATION "${RAYLIB_DIR}/lib/libraylib.a")

set(ink_sources)
foreach(module IN ITEMS core fluid ecs game audio render app)
    file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS "src/${module}/*.c")
    list(APPEND ink_sources ${module_sources})
    if(module STREQUAL "app")
        add_executable(inkstorm ${module_sources})
        target_include_directories(inkstorm PRIVATE src/app)
    else()
        add_library(ink_${module} STATIC ${module_sources})
        target_include_directories(ink_${module} PUBLIC src/${module})
    endif()
endforeach()

target_link_libraries(ink_core PRIVATE m)
target_link_libraries(ink_fluid PRIVATE ink_core)
target_link_libraries(ink_ecs PRIVATE ink_core)
target_link_libraries(ink_game PUBLIC ink_fluid ink_ecs PRIVATE ink_core)
target_link_libraries(ink_audio PRIVATE ink_core ink_game)
target_link_libraries(ink_render PUBLIC ink_game PRIVATE ink_core ink_fluid ink_ecs raylib)
target_link_libraries(inkstorm PRIVATE
    ink_core ink_fluid ink_ecs ink_game ink_audio ink_render raylib GL m pthread dl rt X11
)
target_include_directories(ink_render SYSTEM PRIVATE "${RAYLIB_DIR}/include")
target_include_directories(inkstorm SYSTEM PRIVATE "${RAYLIB_DIR}/include")

# Development builds and the extracted submission both have assets beside the executable.
if(NOT CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_CURRENT_BINARY_DIR}/assets"
        SYMBOLIC)
endif()

include(CTest)
if(BUILD_TESTING)
    add_library(unity STATIC third_party/unity/unity.c)
    target_include_directories(unity SYSTEM PUBLIC third_party/unity)
    target_compile_options(unity PRIVATE -w)

    add_executable(test_core_smoke tests/test_core_smoke.c)
    target_link_libraries(test_core_smoke PRIVATE unity ink_core ink_fluid ink_ecs ink_game ink_audio)
    add_test(NAME test_core_smoke COMMAND test_core_smoke)
    add_test(NAME test_cli_headless_smoke COMMAND inkstorm --headless-smoke)
endif()

file(GLOB_RECURSE format_sources CONFIGURE_DEPENDS src/*.c src/*.h tests/*.c tests/*.h)
file(GLOB shell_scripts CONFIGURE_DEPENDS tools/*.sh)
add_custom_target(format-check
    COMMAND clang-format --dry-run --Werror ${format_sources}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(lint
    COMMAND clang-tidy -p "${CMAKE_CURRENT_BINARY_DIR}" ${ink_sources}
    COMMAND cppcheck --enable=warning,style,performance,portability --error-exitcode=1
        "--project=${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json"
        "--file-filter=${CMAKE_CURRENT_SOURCE_DIR}/src/*"
    COMMAND python3 tools/check_shaders.py src
    COMMAND shellcheck ${shell_scripts}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(dist
    COMMAND "${CMAKE_COMMAND}" -E env "RAYLIB_DIR=${RAYLIB_DIR}" python3 tools/package_submission.py
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)
