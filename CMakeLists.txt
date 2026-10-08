cmake_minimum_required(VERSION 3.21)
project(tree_md VERSION 1.0.3 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network)
add_library(tree_md_core STATIC
    core/markdown/MarkdownAnalysis.cpp
    core/markdown/LanguageCatalog.cpp
    core/markdown/MarkdownParser.cpp
    core/markdown/MarkdownImages.cpp
    core/library/MarkdownManager.cpp
    core/library/MarkdownDocumentOperations.cpp
    core/library/LibraryPathPolicy.cpp
    core/library/LibraryImport.cpp
    core/knowledge/MarkdownManagerRelations.cpp
    core/library/MarkdownNode.cpp
    core/library/MarkdownFileIO.cpp
    core/library/FileWatcher.cpp
    core/library/DefaultLibrary.cpp
    core/knowledge/MarkdownRelation.cpp
    core/knowledge/KnowledgeIndex.cpp
    core/knowledge/KnowledgeTopology.cpp
    core/images/ImageLoader.cpp
    core/images/ImageStorage.cpp
    core/images/ImageInsertJob.cpp
    core/images/ImageUploadJob.cpp
    core/images/MarkdownExportJob.cpp
    core/settings/MarkdownSettings.cpp
    core/settings/KnowledgeViewSettings.cpp
    core/settings/MarkdownSettingsCodec.cpp
    core/settings/MarkdownSettingsStore.cpp
    core/platform/LocalCredentials.cpp
    core/markdown/MarkdownSourceMap.cpp
)
target_link_libraries(tree_md_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
if(WIN32)
    target_link_libraries(tree_md_core PRIVATE crypt32)
endif()
target_include_directories(tree_md_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_library(tree_md_ui STATIC
    ui/document/MarkdownDocumentView.cpp
    ui/document/MarkdownEditor.cpp
    ui/document/MarkdownTypography.cpp
    ui/document/CodeSyntaxHighlighter.cpp
    ui/document/MarkdownFenceCompleter.cpp
    ui/document/ImageInput.cpp
    ui/library/DocumentSidebar.cpp
    ui/library/DocumentTreeBuilder.cpp
    ui/library/OutlineSidebar.cpp
    ui/knowledge/KnowledgeTreeCanvas.cpp
    ui/knowledge/KnowledgeTreeLayout.cpp
    ui/knowledge/KnowledgeTreeRouter.cpp
    ui/knowledge/KnowledgeNodeWidget.cpp
    ui/knowledge/KnowledgeGraphWidget.cpp
    ui/knowledge/RelationDialog.cpp
    ui/settings/MarkdownSettingsDialog.cpp
    ui/settings/ImageSettingsDialog.cpp
    ui/common/Theme.cpp
    ui/common/AppIcon.cpp
    ui/MainWindow.cpp
    resources/branding.qrc
    resources/typography.qrc
)
target_link_libraries(tree_md_ui PUBLIC tree_md_core Qt6::Widgets)
qt_add_executable(tree_md main.cpp)
if(WIN32)
    enable_language(RC)
    configure_file(resources/tree_md.rc ${CMAKE_CURRENT_BINARY_DIR}/tree_md-version.rc @ONLY)
    target_sources(tree_md PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tree_md-version.rc)
    target_include_directories(tree_md PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources)
endif()
target_link_libraries(tree_md PRIVATE tree_md_ui)
set_target_properties(tree_md PROPERTIES WIN32_EXECUTABLE TRUE)
target_compile_definitions(tree_md PRIVATE TREE_MD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" TREE_MD_VERSION="${PROJECT_VERSION}")
install(TARGETS tree_md RUNTIME DESTINATION .)
install(DIRECTORY examples/md_data DESTINATION .)
install(DIRECTORY examples/md_photo DESTINATION .)
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(tmd_markdown_tests tests/MarkdownCoreTests.cpp)
    target_link_libraries(tmd_markdown_tests PRIVATE tree_md_core Qt6::Test)
    add_test(NAME tmd_markdown_core COMMAND tmd_markdown_tests)
    add_executable(tmd_refactor_tests tests/RefactorIntegrationTests.cpp)
    target_link_libraries(tmd_refactor_tests PRIVATE tree_md_ui Qt6::Test)
    add_test(NAME tmd_incremental_refresh COMMAND tmd_refactor_tests)
    set_tests_properties(tmd_incremental_refresh PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
    add_executable(tree_md_tests tests/TreeMdTests.cpp)
    target_link_libraries(tree_md_tests PRIVATE tree_md_ui Qt6::Test)
    include(tests/RegressionGroups.cmake)
    add_executable(tmd_library_tests tests/DefaultLibraryTests.cpp)
    target_link_libraries(tmd_library_tests PRIVATE tree_md_core Qt6::Test)
    add_test(NAME tmd_library_initialization COMMAND tmd_library_tests)
endif()

