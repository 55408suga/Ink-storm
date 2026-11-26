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
target_link_libraries(ink_fluid PUBLIC ink_core)
target_link_libraries(ink_ecs PRIVATE ink_core)
target_link_libraries(ink_game PUBLIC ink_fluid ink_ecs PRIVATE ink_core)
target_link_libraries(ink_audio PRIVATE ink_core ink_game)
target_link_libraries(ink_render PUBLIC ink_game PRIVATE ink_core ink_fluid ink_ecs raylib)
target_link_libraries(inkstorm PRIVATE
    ink_core ink_fluid ink_ecs ink_game ink_audio ink_render raylib GL m pthread dl rt X11
)
target_include_directories(ink_render SYSTEM PRIVATE "${RAYLIB_DIR}/include")
target_include_directories(inkstorm SYSTEM PRIVATE "${RAYLIB_DIR}/include")

# Development builds and the extracted source archive both have assets beside the executable.
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

    add_executable(test_fluid tests/test_fluid.c)
    target_link_libraries(test_fluid PRIVATE unity ink_fluid ink_core m)
    add_test(NAME test_fluid COMMAND test_fluid)
    add_executable(bench_fluid tests/bench_fluid.c)
    target_link_libraries(bench_fluid PRIVATE ink_fluid ink_core m)
    add_test(NAME test_cli_headless_smoke COMMAND inkstorm --headless-smoke)

    add_executable(test_ecs tests/test_ecs.c)
    target_link_libraries(test_ecs PRIVATE unity ink_ecs ink_core)
    add_test(NAME test_ecs COMMAND test_ecs)
    add_executable(test_roguelike_meta tests/test_roguelike_meta.c)
    target_link_libraries(test_roguelike_meta PRIVATE unity ink_game ink_core)
    add_test(NAME test_roguelike_meta COMMAND test_roguelike_meta)

    add_executable(test_elements tests/test_elements.c)
    target_link_libraries(test_elements PRIVATE unity ink_game ink_core m)
    add_test(NAME test_elements COMMAND test_elements)
    add_executable(test_determinism_run tests/test_determinism_run.c)
    target_link_libraries(test_determinism_run PRIVATE unity ink_game ink_core m)
    add_test(NAME test_determinism_run COMMAND test_determinism_run)
    add_executable(bench_game tests/bench_game.c)
    target_link_libraries(bench_game PRIVATE ink_game ink_core m)
    add_executable(test_audio tests/test_audio.c)
    target_link_libraries(test_audio PRIVATE unity ink_audio ink_game ink_core m)
    add_test(NAME test_audio COMMAND test_audio)
    add_executable(test_app_save tests/test_app_save.c src/app/save.c src/app/paths.c)
    target_include_directories(test_app_save PRIVATE src/app)
    target_link_libraries(test_app_save PRIVATE unity ink_game ink_core)
    add_test(NAME test_app_save COMMAND test_app_save)
    add_executable(test_app_core tests/test_app_core.c src/app/audio_cb.c src/app/cli.c
        src/app/frame_clock.c src/app/input_latch.c src/app/menu_nav.c src/app/bgm_fade.c
        src/app/hint_queue.c src/app/save.c)
    target_include_directories(test_app_core PRIVATE src/app src/render)
    target_link_libraries(test_app_core PRIVATE unity ink_audio ink_game ink_core m pthread)
    add_test(NAME test_app_core COMMAND test_app_core)
    add_executable(test_text_table tests/test_text_table.c src/render/text_table.c
        src/render/text_layout.c)
    target_include_directories(test_text_table PRIVATE src/render)
    target_link_libraries(test_text_table PRIVATE unity ink_game ink_core)
    add_test(NAME test_text_table COMMAND test_text_table)
    set(expect_usage sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/expect_usage.sh" "$<TARGET_FILE:inkstorm>")
    add_test(NAME test_cli_usage_unknown COMMAND ${expect_usage} --bogus)
    add_test(NAME test_cli_usage_save_exclusive COMMAND ${expect_usage} --no-save --save-path x)
    add_test(NAME test_cli_usage_headless_exclusive COMMAND ${expect_usage} --headless-smoke --smoke-frames 1)
    add_test(NAME test_cli_usage_demo_exclusive COMMAND ${expect_usage} --demo --showcase)
    add_test(NAME test_cli_usage_snap_exclusive COMMAND ${expect_usage} --snap out --smoke-frames 1)
    add_test(NAME test_cli_usage_snap_missing_arg COMMAND ${expect_usage} --snap)
    add_executable(test_acts_enemies tests/test_acts_enemies.c)
    target_link_libraries(test_acts_enemies PRIVATE unity ink_game ink_core m)
    add_test(NAME test_acts_enemies COMMAND test_acts_enemies)
    add_executable(test_vfx_rules tests/test_vfx_rules.c src/render/vfx_rules.c)
    target_include_directories(test_vfx_rules PRIVATE src/render)
    target_link_libraries(test_vfx_rules PRIVATE unity ink_game ink_core)
    add_test(NAME test_vfx_rules COMMAND test_vfx_rules)
    add_executable(test_render_rules tests/test_render_rules.c src/render/render_rules.c)
    target_include_directories(test_render_rules PRIVATE src/render)
    target_link_libraries(test_render_rules PRIVATE unity ink_fluid ink_core m)
    add_test(NAME test_render_rules COMMAND test_render_rules)
    add_executable(test_sprite_rules tests/test_sprite_rules.c src/render/sprite_rules.c
                                     src/render/png_rgba.c)
    target_include_directories(test_sprite_rules PRIVATE src/render)
    target_link_libraries(test_sprite_rules PRIVATE unity ink_game ink_core m)
    add_test(NAME test_sprite_rules COMMAND test_sprite_rules)
    add_executable(test_gameplay_fixes tests/test_gameplay_fixes.c)
    target_link_libraries(test_gameplay_fixes PRIVATE unity ink_game ink_core m)
    add_test(NAME test_gameplay_fixes COMMAND test_gameplay_fixes)
    add_executable(qa_record tests/qa_record.c src/app/paths.c src/app/bgm_fade.c src/app/cli.c)
    target_include_directories(qa_record PRIVATE src/app)
    target_include_directories(qa_record SYSTEM PRIVATE "${RAYLIB_DIR}/include")
    target_link_libraries(qa_record PRIVATE
        ink_core ink_fluid ink_ecs ink_game ink_audio ink_render raylib GL m pthread dl rt X11
    )
    add_test(NAME test_qa_run COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qa_run.py
        $<TARGET_FILE:qa_record>)
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
    COMMAND cppcheck --enable=warning,style,performance,portability --check-level=exhaustive --error-exitcode=1
        "--project=${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json"
        "--file-filter=${CMAKE_CURRENT_SOURCE_DIR}/src/*"
    COMMAND python3 tools/check_shaders.py src
    COMMAND shellcheck ${shell_scripts}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(dist
    COMMAND "${CMAKE_COMMAND}" -E env "RAYLIB_DIR=${RAYLIB_DIR}" python3 tools/package_dist.py
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)
