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
option(KRAKEN_ENABLE_JUCE "Enable JUCE-based runtime and UI" OFF)

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
    src/runtime/ModuleOrchestrator.cpp
    src/platform/Platform.cpp
)

target_include_directories(kraken_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(KRAKEN_ENABLE_JUCE)
    if(NOT TARGET juce::juce_gui_extra)
        if(KRAKEN_ALLOW_FETCHCONTENT)
            FetchContent_Declare(
                juce
                GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                GIT_TAG 8.0.1)
            FetchContent_MakeAvailable(juce)
        else()
            find_package(JUCE 8 CONFIG REQUIRED COMPONENTS
                juce_gui_extra
                juce_audio_utils
                juce_audio_devices
                juce_dsp)
        endif()
    endif()
endif()

if(KRAKEN_ENABLE_JUCE)
    target_compile_definitions(kraken_core PUBLIC KRAKEN_HAS_JUCE=1)
else()
    target_compile_definitions(kraken_core PUBLIC KRAKEN_HAS_JUCE=0)
endif()

# Optional codecs must not force network access or block a non-JUCE build.
set(KRAKEN_HAS_MINIMP3 0)
set(KRAKEN_HAS_MINIZIP 0)
set(KRAKEN_HAS_SNDFILE 0)

if(KRAKEN_ENABLE_MP3)
    find_path(KRAKEN_MINIMP3_INCLUDE_DIR NAMES minimp3/minimp3_ex.h
        HINTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
              "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
              "${CMAKE_CURRENT_SOURCE_DIR}/external")
    if(NOT KRAKEN_MINIMP3_INCLUDE_DIR AND KRAKEN_ALLOW_FETCHCONTENT)
        FetchContent_Declare(minimp3
            GIT_REPOSITORY https://github.com/lieff/minimp3.git
            GIT_TAG master)
        FetchContent_GetProperties(minimp3)
        if(NOT minimp3_POPULATED)
            FetchContent_Populate(minimp3)
        endif()
        # Source includes <minimp3/minimp3_ex.h>; fetched header is at repo root.
        get_filename_component(KRAKEN_MINIMP3_INCLUDE_DIR
            "${minimp3_SOURCE_DIR}" DIRECTORY)
    endif()
    if(KRAKEN_MINIMP3_INCLUDE_DIR)
        set(KRAKEN_HAS_MINIMP3 1)
        target_include_directories(kraken_core PRIVATE "${KRAKEN_MINIMP3_INCLUDE_DIR}")
    else()
        message(STATUS "minimp3 unavailable: MP3 decoding disabled")
    endif()
endif()

if(KRAKEN_ENABLE_ZIP)
    if(NOT TARGET minizip::minizip AND NOT TARGET MINIZIP::minizip)
        find_package(minizip CONFIG QUIET)
    endif()
    if(NOT TARGET minizip::minizip AND NOT TARGET MINIZIP::minizip
       AND KRAKEN_ALLOW_FETCHCONTENT)
        FetchContent_Declare(minizip
            GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
            GIT_TAG 4.0.3)
        FetchContent_MakeAvailable(minizip)
    endif()
    if(TARGET minizip::minizip)
        target_link_libraries(kraken_core PUBLIC minizip::minizip)
        set(KRAKEN_HAS_MINIZIP 1)
    elseif(TARGET MINIZIP::minizip)
        target_link_libraries(kraken_core PUBLIC MINIZIP::minizip)
        set(KRAKEN_HAS_MINIZIP 1)
    else()
        message(STATUS "minizip unavailable: ZIP support disabled")
    endif()
endif()

find_package(SndFile QUIET)
if(TARGET SndFile::sndfile)
    set(KRAKEN_SNDFILE_TARGET SndFile::sndfile)
elseif(TARGET sndfile)
    set(KRAKEN_SNDFILE_TARGET sndfile)
else()
    # Handle installations without a CMake package config.
    find_path(KRAKEN_SNDFILE_INCLUDE_DIR NAMES sndfile.h)
    find_library(KRAKEN_SNDFILE_LIBRARY NAMES sndfile libsndfile)
    if(KRAKEN_SNDFILE_INCLUDE_DIR AND KRAKEN_SNDFILE_LIBRARY)
        add_library(kraken_system_sndfile UNKNOWN IMPORTED)
        set_target_properties(kraken_system_sndfile PROPERTIES
            IMPORTED_LOCATION "${KRAKEN_SNDFILE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${KRAKEN_SNDFILE_INCLUDE_DIR}")
        set(KRAKEN_SNDFILE_TARGET kraken_system_sndfile)
    elseif(KRAKEN_ALLOW_FETCHCONTENT)
        FetchContent_Declare(libsndfile
            GIT_REPOSITORY https://github.com/libsndfile/libsndfile.git
            GIT_TAG 1.2.2)
        FetchContent_MakeAvailable(libsndfile)
        if(TARGET SndFile::sndfile)
            set(KRAKEN_SNDFILE_TARGET SndFile::sndfile)
        elseif(TARGET sndfile)
            set(KRAKEN_SNDFILE_TARGET sndfile)
        endif()
    endif()
endif()
if(KRAKEN_SNDFILE_TARGET)
    set(KRAKEN_HAS_SNDFILE 1)
    target_link_libraries(kraken_core PUBLIC ${KRAKEN_SNDFILE_TARGET})
else()
    message(STATUS "libsndfile unavailable: non-MP3 file decoding disabled")
endif()

target_compile_definitions(kraken_core PRIVATE
    KRAKEN_HAS_MINIMP3=${KRAKEN_HAS_MINIMP3}
    KRAKEN_HAS_MINIZIP=${KRAKEN_HAS_MINIZIP}
    KRAKEN_HAS_SNDFILE=${KRAKEN_HAS_SNDFILE})

add_executable(kraken_cli
    src/main.cpp
    src/platform/JuceApp.cpp)
target_link_libraries(kraken_cli PRIVATE kraken_core)

if(KRAKEN_ENABLE_JUCE)
    target_compile_definitions(kraken_cli PRIVATE KRAKEN_HAS_JUCE=1)
else()
    target_compile_definitions(kraken_cli PRIVATE KRAKEN_HAS_JUCE=0)
endif()

if(KRAKEN_ENABLE_JUCE)
    target_link_libraries(kraken_cli PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_dsp)
    target_compile_definitions(kraken_cli PRIVATE
        JUCE_APPLICATION_NAME_STRING="Kraken Live"
        JUCE_APPLICATION_VERSION_STRING="0.1.0"
        JUCE_STANDALONE_APPLICATION=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_REPORT_APP_USAGE=0)
endif()

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
