cmake_minimum_required(VERSION 3.20)
project(towerfall VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# raylib 5.5 — vendored, built as part of this project.
#
# The official prebuilt MinGW package is linked against the UCRT runtime while this
# toolchain targets msvcrt, which fails at link time with "undefined reference to
# stat64i32". Building from the vendored source gives a matching ABI.
#
# add_subdirectory() (rather than an IMPORTED target pointing at a prebuilt .a) is
# what makes a FRESH CLONE build. An IMPORTED target only works if the library
# already exists on disk, and build directories are not in version control — so a
# clean checkout would configure fine and then fail at link time with
# "No rule to make target ... libraylib.a". With add_subdirectory,
# `git clone && cmake && make` simply works.
# ---------------------------------------------------------------------------
set(RAYLIB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib-5.5")

if(EXISTS "${RAYLIB_SRC_DIR}/CMakeLists.txt")
    # These must be set BEFORE the subdirectory is added.
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    set(SUPPORT_MODULE_RAUDIO ON CACHE BOOL "" FORCE)

    add_subdirectory("${RAYLIB_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/raylib-build" EXCLUDE_FROM_ALL)
    set(RAYLIB_TARGET raylib)
    message(STATUS "towerfall: using the vendored raylib 5.5 (built from source)")
else()
    find_package(raylib 4.5 REQUIRED)
    set(RAYLIB_TARGET raylib)
    message(STATUS "towerfall: using the system raylib")
endif()

# raylib needs these Windows system libraries.
if(WIN32)
    set(RAYLIB_EXTRA_LIBS winmm gdi32 opengl32 user32 shell32 ole32 ws2_32)
else()
    set(RAYLIB_EXTRA_LIBS "")
endif()

# ---------------------------------------------------------------------------
# Lua 5.4 — vendored source. Only the library is built; the standalone
# interpreter (lua.c) and compiler (luac.c) are excluded.
# ---------------------------------------------------------------------------
set(LUA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/src")
if(NOT EXISTS "${LUA_SRC_DIR}/lua.h")
    message(FATAL_ERROR "vendored Lua not found at ${LUA_SRC_DIR}")
endif()

file(GLOB LUA_CORE_SOURCES "${LUA_SRC_DIR}/*.c")
list(FILTER LUA_CORE_SOURCES EXCLUDE REGEX ".*/(lua|luac)\\.c$")

add_library(lua_vendored STATIC ${LUA_CORE_SOURCES})
target_include_directories(lua_vendored PUBLIC "${LUA_SRC_DIR}")
# LUA_USE_POSIX pulls in <sys/wait.h> and <sys/times.h>, which do not exist on
# Windows. Lua's own Makefile only defines it for POSIX hosts, so mirror that.
if(NOT WIN32)
    target_compile_definitions(lua_vendored PUBLIC LUA_USE_POSIX)
    target_link_libraries(lua_vendored PUBLIC m dl)
endif()
if(WIN32)
    # Lua needs the MSVC CRT compatibility shims on MinGW.
    target_compile_definitions(lua_vendored PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
# Core game logic — pure C++, no graphics and no audio.
#
# Kept separate from the presentation layer so the simulation can be tested
# headlessly: the same code that runs in the game runs in the test binary.
# ---------------------------------------------------------------------------
file(GLOB TF_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp")
add_library(tfcore STATIC ${TF_CORE_SOURCES})
target_include_directories(tfcore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(tfcore PUBLIC lua_vendored)
target_compile_definitions(tfcore PUBLIC
    TF_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# ---------------------------------------------------------------------------
# Presentation layer — raylib rendering and procedural sprite generation.
# ---------------------------------------------------------------------------
file(GLOB TF_RENDER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/render/*.cpp")
add_library(tfrender STATIC ${TF_RENDER_SOURCES})
target_include_directories(tfrender PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(tfrender PUBLIC tfcore ${RAYLIB_TARGET})

# ---- the game executable
add_executable(towerfall "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
target_link_libraries(towerfall PRIVATE tfrender tfcore ${RAYLIB_TARGET} ${RAYLIB_EXTRA_LIBS})
if(NOT WIN32)
    target_link_libraries(towerfall PRIVATE m dl pthread)
endif()

# ---- headless test binary: proves the simulation without a window
add_executable(towerfall_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.cpp")
target_link_libraries(towerfall_tests PRIVATE tfcore)
if(NOT WIN32)
    target_link_libraries(towerfall_tests PRIVATE m dl pthread)
endif()

enable_testing()
add_test(NAME towerfall_tests COMMAND towerfall_tests)

# ---- sprite sheet previewer: renders every sprite to one PNG so the artwork
#      can be reviewed without launching the game.
add_executable(towerfall_sprites "${CMAKE_CURRENT_SOURCE_DIR}/tools/sprite_sheet.cpp")
target_link_libraries(towerfall_sprites PRIVATE tfrender tfcore ${RAYLIB_TARGET} ${RAYLIB_EXTRA_LIBS})
if(NOT WIN32)
    target_link_libraries(towerfall_sprites PRIVATE m dl pthread)
endif()
