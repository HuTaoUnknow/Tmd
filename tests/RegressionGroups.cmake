# Each slot belongs to exactly one group; use tree_md_tests directly for an
# all-in-one manual run. Offscreen groups deliberately run serially by default.
set(tmd_library_checks
    utf8ChineseAndAtomicIO
    transientWindowsLockKeepsAtomicSave
    outlineFencesAndSetext
    emptyAndLargeDocuments
    documentLifecycleAndPortableRelations
    uniqueRolesArePerPair
    externalChangeProtectsEdits
    externalDeletePrunesReferences
    externalRenameKeepsEditsAndRelations
    metadataErrorsPreserveState
    externalMetadataConflictRollsBack
    invalidPathsAndWindowsAliases
    watcherRecognizesNestedChanges
)
add_test(NAME tmd_library_regressions COMMAND tree_md_tests ${tmd_library_checks})
set_tests_properties(tmd_library_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_images_checks
    imageSyntaxPreservesCodeAndTitles
    imageImportMirrorsDirectoriesAndDeduplicates
    imageImportFailureRollsBack
    imageRelocationAndMetadataRollback
    imageExportLocalEmbeddedAndRemote
    imageExportFailureAndCancellation
    applicationImageDropPasteAndPathModes
    applicationExportOptions
    documentRenderInlineImagesAndSourceIntegrity
    documentRenderIgnoresStaleNetworkResults
)
add_test(NAME tmd_images_regressions COMMAND tree_md_tests ${tmd_images_checks})
set_tests_properties(tmd_images_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_document_checks
    editorPreservesMarkdownAndUndo
    graphShowsAllBranchesAndCardClick
    applicationEditingNavigationAndSource
    fileMenuLayoutAndModeSwitch
    renderedEditingPreservesSource
    renderedListsTablesAndUndo
    renderedParagraphsCodeAndDeletion
    headingsRequireSpaceWhenRendered
    headingBackspaceRestoresMarker
    emptyHeadingsKeepEditablePositions
    headingEditingUndoAndParagraphIntegrity
    markdownStylesAndOutlinePosition
    imagesHaveNoExcessSpacing
    fontZoomBoundsAndSourceIntegrity
    fenceLanguageCompletionInSource
    fenceLanguageCompletionInDocument
    fencePopupKeyboardSelection
    fenceCompletionRespectsCodeBoundaries
    fenceCompletionPreservesExistingBlocks
    emptyFencedBlocksKeepTypingInside
    inlineCodeCursorCanLeaveAndReenter
    inlineCodeMouseCanTypeAfter
)
add_test(NAME tmd_document_regressions COMMAND tree_md_tests ${tmd_document_checks})
set_tests_properties(tmd_document_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_knowledge_checks
    knowledgeIndexTraversesCyclesAndIncomingReferences
    treeRelationReplacementIsAtomic
    knowledgeTreeDropPreviewAndPersistence
    knowledgeTreeCardConnectionsAndRemoval
    knowledgeTreeDirectorySearchAndReindex
    reciprocalRolesPersistAndRemoveFromEitherSide
    legacyReciprocalMetadataAndConflictProtection
    reciprocalRolesUpdateBothInterfaces
    knowledgeIndexExpandsFiveLayersInBothDirections
    mainAndManagementTreesShareRangeAndLayout
    mainTreeNavigationDoesNotEditRelations
    nestedCardsFollowHierarchyAndSharedNodes
    nestedDropPreviewAndSmallCardPorts
    alternativeRoutesStayInSync
    legacyAlternativesAndDetailsStaySeparate
    learningRouteCardsAndConnectors
    referenceLayoutHasAlignedRouteAndParallelDetails
    detailBranchesReserveSpaceBesideRoutes
    skippedAndSharedConnectionsAvoidCards
    attachedDetailStaysBelowItsAlternativeAfterNewLinks
    treeLayoutRemainsStableWhenCenterChanges
)
add_test(NAME tmd_knowledge_regressions COMMAND tree_md_tests ${tmd_knowledge_checks})
set_tests_properties(tmd_knowledge_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_workspace_checks
    responsivePanelsAndOneThirdTree
    sharedTreeZoomPersistsAcrossViews
    sidebarDragCyclesThroughAllModes
    sidebarHeaderMorphsWithoutGeometryJumps
    compactSearchOpensDocumentsAndRefreshes
    recyclePreservesOriginalNameAndContent
    directoryLifecycleAndRecycleRelations
    recycleFailurePreservesFilesAndRelations
    sidebarContextTargetsAndEmptyDirectories
    contextNewMarkdownNamesAndPreservesEdits
    popupMenusShareRoundedBlackTheme
    applicationContextActionsPreserveOtherEdits
    directoryDeleteProtectsUnsavedCurrentDocument
)
add_test(NAME tmd_workspace_regressions COMMAND tree_md_tests ${tmd_workspace_checks})
set_tests_properties(tmd_workspace_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_settings_checks
    settingsHexRoundTripAndValidation
    settingsMenuAndAppearanceApply
    settingsImportExportDialog
    imageConfigurationAndPaths
)
add_test(NAME tmd_settings_regressions COMMAND tree_md_tests ${tmd_settings_checks})
set_tests_properties(tmd_settings_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_registered_checks ${tmd_library_checks} ${tmd_images_checks} ${tmd_document_checks} ${tmd_knowledge_checks} ${tmd_workspace_checks} ${tmd_settings_checks})
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tests/TreeMdTests.cpp" tmd_test_source)
string(REGEX MATCHALL "void TreeMdTests::[A-Za-z0-9_]+\\(" tmd_test_definitions "${tmd_test_source}")
foreach(definition IN LISTS tmd_test_definitions)
    string(REGEX REPLACE "void TreeMdTests::([A-Za-z0-9_]+)\\(" "\\1" slot "${definition}")
    if(NOT slot MATCHES "^(init|initTestCase)$|_data$" AND NOT slot IN_LIST tmd_registered_checks)
        message(FATAL_ERROR "Register the new test ${slot} in tests/RegressionGroups.cmake")
    endif()
endforeach()
list(LENGTH tmd_registered_checks tmd_registered_count)
list(REMOVE_DUPLICATES tmd_registered_checks)
list(LENGTH tmd_registered_checks tmd_unique_count)
if(NOT tmd_registered_count EQUAL tmd_unique_count)
    message(FATAL_ERROR "A regression slot is registered more than once")
endif()
