# Each slot belongs to exactly one group; use tree_md_tests directly for an
# all-in-one manual run. Offscreen groups deliberately run serially by default.
add_test(NAME tmd_reference_maintenance COMMAND tmd_reference_tests -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_reference_maintenance.txt,txt")
set_tests_properties(tmd_reference_maintenance PROPERTIES TIMEOUT 180
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
add_test(NAME tmd_selection_links COMMAND tmd_selection_link_tests -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_selection_links.txt,txt")
set_tests_properties(tmd_selection_links PROPERTIES TIMEOUT 180
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
add_test(NAME tmd_document_find COMMAND tmd_document_find_tests -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_document_find.txt,txt")
set_tests_properties(tmd_document_find PROPERTIES TIMEOUT 180
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
add_test(NAME tmd_editing_headings_audit COMMAND tmd_editing_headings_tests -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_editing_headings_audit.txt,txt")
add_test(NAME tmd_editing_code_audit COMMAND tmd_editing_code_tests -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_editing_code_audit.txt,txt")
add_test(NAME tmd_editing_list_audit COMMAND tmd_editing_list_tests -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_editing_list_audit.txt,txt")
add_test(NAME tmd_editing_integration_audit COMMAND tmd_editing_integration_tests -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_editing_integration_audit.txt,txt")
set_tests_properties(tmd_editing_headings_audit tmd_editing_code_audit tmd_editing_list_audit tmd_editing_integration_audit PROPERTIES
    TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_library_checks
    movePathsPreserveImagesAndRelations
    watchedDirectoriesMoveAcrossInstances
    directoryMoveReportsLocksAndRetries
    directoryMoveWithSharedDescendantHandles
    directoryMoveStagingRollsBackOnDeniedDestination
    directoryMoveImageRewriteFailureRestoresOriginal
    movePathsRejectConflictsAndDirtyDocuments
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
add_test(NAME tmd_library_regressions COMMAND tree_md_tests ${tmd_library_checks} -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_library_regressions.txt,txt")
set_tests_properties(tmd_library_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_images_checks
    imageSyntaxPreservesCodeAndTitles
    imageImportUsesFlatAbsoluteDirectoryAndDeduplicates
    imageImportFailureRollsBack
    imageRelocationAndMetadataRollback
    imageExportLocalEmbeddedAndRemote
    imageExportFailureAndCancellation
    applicationImageDropPasteAndPathModes
    applicationExportOptions
    documentRenderInlineImagesAndSourceIntegrity
    documentRenderIgnoresStaleNetworkResults
)
add_test(NAME tmd_images_regressions COMMAND tree_md_tests ${tmd_images_checks} -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_images_regressions.txt,txt")
set_tests_properties(tmd_images_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_document_checks
    listEnterIndentAndBackspace
    editorPreservesMarkdownAndUndo
    graphShowsAllBranchesAndCardClick
    applicationEditingNavigationAndSource
    fileMenuLayoutAndModeSwitch
    renderedEditingPreservesSource
    renderedListsTablesAndUndo
    renderedParagraphsCodeAndDeletion
    headingsRequireSpaceWhenRendered
    formattedLinesPreserveTypedSpaces
    linePrefixSpaceEntersPreviewAndBackspaceRestores
    listBackspaceRestoresMarkerInSameRow
    linePrefixesRequireSpaceAndStayOnTheirLine
    activeLineMarkersSupportCharacterEditing
    standardBoundariesMatchRenderedHeadings
    inlineFormattingShowsSourceMarkers
    inlineArrowsAdvanceOneCharacter
    inlineCodeDoesNotAbsorbFollowingText
    inlineMarkerEditingKeepsExactSourcePosition
    inlineMarkersShareCodeBackgroundAndSelection
    inlineMouseSelectionHasIndependentMarkers
    inlineCodeHasRoundedCorners
    enterStartsBodyWithoutDecorationIndent
    fencedCodeIndentation
    codeHeadersCopyOnlyTheirOwnBody
    codeHighlightingTracksLanguageAndBlockBoundaries
    emptyListAndQuoteEnterReturnsToBody
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
    fenceCompletionKeepsFollowingParagraphsOutside
    fencedFirstLineBackspacePreservesDelimiters
    unpairedFencesStayLiteralAndEditable
    emptyFencedBlocksKeepTypingInside
    inlineCodeCursorCanLeaveAndReenter
    inlineCodeMouseCanTypeAfter
)
add_test(NAME tmd_document_regressions COMMAND tree_md_tests ${tmd_document_checks} -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_document_regressions.txt,txt")
set_tests_properties(tmd_document_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_knowledge_checks
    knowledgeIndexTraversesCyclesAndIncomingReferences
    treeRelationReplacementIsAtomic
    treeNodeRemovalIsAtomicAndPreservesDocuments
    knowledgeTreeSelectedNodeRemoval
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
add_test(NAME tmd_knowledge_regressions COMMAND tree_md_tests ${tmd_knowledge_checks} -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_knowledge_regressions.txt,txt")
set_tests_properties(tmd_knowledge_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_workspace_checks
    unsavedDocumentsKeepDotsAndSaveIndividually
    closeSaveAllFailureKeepsUnsavedDocuments
    sidebarColorsDragAndExplorerTargets
    applicationMoveKeepsCurrentAndUnsaved
    responsivePanelsAndOneThirdTree
    sharedTreeZoomPersistsAcrossViews
    sidebarDragCyclesThroughAllModes
    sidebarHeaderMorphsWithoutGeometryJumps
    compactSearchOpensDocumentsAndRefreshes
    recyclePreservesOriginalNameAndContent
    directoryLifecycleAndRecycleRelations
    recycleFailurePreservesFilesAndRelations
    sidebarContextTargetsAndEmptyDirectories
    sidebarScrollBlankCreatesAtRoot
    sidebarRemembersExpansionAndDragRestores
    sidebarBoxSelectionAndTextDragging
    batchMoveDocumentsPreservesImagesAndRollsBack
    directoryRenamePreservesContentAndCurrentDocument
    editingViewsHaveHalfPageBlank
    modeSwitchAlignsCurrentThirdLine
    contextNewMarkdownNamesAndPreservesEdits
    popupMenusShareRoundedBlackTheme
    applicationContextActionsPreserveOtherEdits
    directoryDeleteProtectsUnsavedCurrentDocument
)
add_test(NAME tmd_workspace_regressions COMMAND tree_md_tests ${tmd_workspace_checks} -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_workspace_regressions.txt,txt")
set_tests_properties(tmd_workspace_regressions PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=C:/Windows/Fonts")
set(tmd_settings_checks
    settingsHexRoundTripAndValidation
    settingsMenuAndAppearanceApply
    settingsImportExportDialog
    imageConfigurationAndPaths
)
add_test(NAME tmd_settings_regressions COMMAND tree_md_tests ${tmd_settings_checks} -o "${CMAKE_CURRENT_BINARY_DIR}/tmd_settings_regressions.txt,txt")
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
