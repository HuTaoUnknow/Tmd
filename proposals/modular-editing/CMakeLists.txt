cmake_minimum_required(VERSION 3.21)
project(tmd_modular_editing_proposal LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
get_filename_component(TMD_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Test)
# This isolated build reuses the real core/UI, without editing their source list.
set(BUILD_TESTING OFF CACHE BOOL "Disable baseline test registration in this isolated build" FORCE)
add_subdirectory("${TMD_PROJECT_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/baseline" EXCLUDE_FROM_ALL)
add_library(tmd_proposal_core STATIC
    core/markdown/editing/TextPatch.cpp
    core/markdown/analysis/MarkdownSnapshot.cpp
    application/editing/EditCommandRouter.cpp
)
target_include_directories(tmd_proposal_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tmd_proposal_core PUBLIC tree_md_core)
add_library(tmd_proposal_qt STATIC ui/document/QtTextEditAdapter.cpp)
target_include_directories(tmd_proposal_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tmd_proposal_qt PUBLIC tmd_proposal_core Qt6::Gui)
add_executable(tmd_proposal_tests tests/ModularEditingTests.cpp)
target_link_libraries(tmd_proposal_tests PRIVATE tmd_proposal_qt tree_md_ui Qt6::Test)
enable_testing()
add_test(NAME tmd_modular_editing_proposal COMMAND tmd_proposal_tests -o "${CMAKE_CURRENT_BINARY_DIR}/proposal-tests.txt,txt")
set_tests_properties(tmd_modular_editing_proposal PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts"
    TIMEOUT 180)
