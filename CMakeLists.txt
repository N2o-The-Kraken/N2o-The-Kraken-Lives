cmake_minimum_required(VERSION 3.16)
project(KrakenLiveSequencer LANGUAGES CXX)

include(GNUInstallDirs)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KRAKEN_BUILD_TESTS "Build Kraken unit tests" OFF)
option(KRAKEN_ALLOW_FETCHCONTENT "Allow CMake to download optional third-party dependencies" OFF)
option(KRAKEN_ENABLE_ZIP "Enable ZIP stem bundle support" ON)
option(KRAKEN_ENABLE_MP3 "Enable MP3 decoding support" ON)

add_library(kraken_core
    src/ai/AIAssistant.cpp
    src/audio/ClipSource.cpp
    src/audio/ClipLoader.cpp
    src/audio/Clip.cpp
    src/audio/EffectChain.cpp
    src/audio/Mixer.cpp
    src/audio/Timeline.cpp
    src/audio/StemResolver.cpp
    src/audio/Minimp3Decoder.cpp
    src/controllers/ControllerManager.cpp
    src/platform/Platform.cpp
)

target_include_directories(kraken_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(KRAKEN_HAS_SNDFILE OFF)
set(KRAKEN_HAS_MINIZIP OFF)
set(KRAKEN_HAS_MINIMP3 OFF)

set(KRAKEN_CORE_DEPS)

find_package(SndFile QUIET)
if(SndFile_FOUND)
    set(KRAKEN_HAS_SNDFILE ON)
    list(APPEND KRAKEN_CORE_DEPS SndFile::sndfile)
elseif(KRAKEN_ALLOW_FETCHCONTENT)
    FetchContent_Declare(libsndfile
        GIT_REPOSITORY https://github.com/libsndfile/libsndfile.git
        GIT_TAG 1.2.2)
    FetchContent_MakeAvailable(libsndfile)
    set(KRAKEN_HAS_SNDFILE ON)
    list(APPEND KRAKEN_CORE_DEPS sndfile)
else()
    message(WARNING "libsndfile not found; WAV decoding will be disabled")
endif()

if(KRAKEN_ENABLE_ZIP)
    find_package(minizip CONFIG QUIET)
    if(minizip_FOUND)
        set(KRAKEN_HAS_MINIZIP ON)
        list(APPEND KRAKEN_CORE_DEPS minizip::minizip)
    else()
        find_path(MINIZIP_INCLUDE_DIR NAMES minizip/unzip.h)
        find_library(MINIZIP_LIBRARY NAMES minizip minizip-ng)
        if(MINIZIP_INCLUDE_DIR AND MINIZIP_LIBRARY)
            set(KRAKEN_HAS_MINIZIP ON)
            target_include_directories(kraken_core PRIVATE ${MINIZIP_INCLUDE_DIR})
            list(APPEND KRAKEN_CORE_DEPS ${MINIZIP_LIBRARY})
        elseif(KRAKEN_ALLOW_FETCHCONTENT)
            FetchContent_Declare(minizip
                GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
                GIT_TAG 4.0.3)
            FetchContent_MakeAvailable(minizip)
            set(KRAKEN_HAS_MINIZIP ON)
            list(APPEND KRAKEN_CORE_DEPS minizip::minizip)
        else()
            message(WARNING "minizip not found; ZIP stem bundles will be disabled")
        endif()
    endif()
endif()

if(KRAKEN_ENABLE_MP3)
    find_path(MINIMP3_INCLUDE_DIR NAMES minimp3_ex.h PATH_SUFFIXES minimp3)
    if(MINIMP3_INCLUDE_DIR)
        set(KRAKEN_HAS_MINIMP3 ON)
        target_include_directories(kraken_core PRIVATE ${MINIMP3_INCLUDE_DIR})
    elseif(KRAKEN_ALLOW_FETCHCONTENT)
        FetchContent_Declare(minimp3
            GIT_REPOSITORY https://github.com/lieff/minimp3.git
            GIT_TAG master)
        FetchContent_MakeAvailable(minimp3)
        set(KRAKEN_HAS_MINIMP3 ON)
        target_include_directories(kraken_core PRIVATE ${minimp3_SOURCE_DIR})
    else()
        message(WARNING "minimp3 not found; MP3 decoding will be disabled")
    endif()
endif()

if(KRAKEN_CORE_DEPS)
    target_link_libraries(kraken_core PUBLIC ${KRAKEN_CORE_DEPS})
endif()

if(KRAKEN_HAS_SNDFILE)
    target_compile_definitions(kraken_core PRIVATE KRAKEN_HAS_SNDFILE=1)
endif()

if(KRAKEN_HAS_MINIZIP)
    target_compile_definitions(kraken_core PRIVATE KRAKEN_HAS_MINIZIP=1)
endif()

if(KRAKEN_HAS_MINIMP3)
    target_compile_definitions(kraken_core PRIVATE KRAKEN_HAS_MINIMP3=1)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set_source_files_properties(src/audio/Minimp3Decoder.cpp PROPERTIES COMPILE_OPTIONS "-w")
    elseif(MSVC)
        set_source_files_properties(src/audio/Minimp3Decoder.cpp PROPERTIES COMPILE_OPTIONS "/w")
    endif()
endif()

add_executable(kraken_cli src/main.cpp)
target_link_libraries(kraken_cli PRIVATE kraken_core)

if(KRAKEN_BUILD_TESTS)
    enable_testing()
    add_executable(kraken_tests tests/AIAssistantTests.cpp)
    target_link_libraries(kraken_tests PRIVATE kraken_core)
    add_test(NAME KrakenTests COMMAND kraken_tests)
endif()

install(TARGETS kraken_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS kraken_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY assets/stems/ DESTINATION ${CMAKE_INSTALL_DATADIR}/kraken/stems)
