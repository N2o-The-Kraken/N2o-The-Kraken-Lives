cmake_minimum_required(VERSION 3.16)
project(KrakenLiveSequencer LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KRAKEN_BUILD_TESTS "Build Kraken unit tests" OFF)

add_library(kraken_core
    src/ai/AIAssistant.cpp
    src/audio/ClipSource.cpp
    src/audio/Clip.cpp
    src/audio/EffectChain.cpp
    src/audio/Mixer.cpp
    src/audio/Timeline.cpp
    src/audio/StemResolver.cpp
    src/controllers/ControllerManager.cpp
    src/platform/Platform.cpp
)

target_include_directories(kraken_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

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
