cmake_minimum_required(VERSION 3.16)

project(BrickSuite
    VERSION 0.3.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Network
)

if(MINGW)
    # Qt's standalone MinGW packages keep standard target libraries under
    # <toolchain>/<target-triplet>. Add that compiler-owned prefix so CMake's
    # supported FindZLIB module can discover the bundled headers and library.
    get_filename_component(BRICKSUITE_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(BRICKSUITE_MINGW_ROOT "${BRICKSUITE_MINGW_BIN_DIR}" DIRECTORY)
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
        OUTPUT_VARIABLE BRICKSUITE_MINGW_TARGET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    list(PREPEND CMAKE_PREFIX_PATH
        "${BRICKSUITE_MINGW_ROOT}/${BRICKSUITE_MINGW_TARGET}"
    )
endif()

find_package(ZLIB REQUIRED)

qt_standard_project_setup()

qt_add_executable(BrickSuite
    src/main.cpp

    src/app/Application.h
    src/app/Application.cpp
    src/app/WorkspaceContext.h
    src/app/WorkspaceContext.cpp

    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/database/DatabaseSchema.h
    src/database/DatabaseSchema.cpp

    src/models/Workspace.h
    src/models/Workspace.cpp

    src/repositories/WorkspaceRepository.h
    src/repositories/WorkspaceRepository.cpp

    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/import/RebrickableReferenceImporter.cpp
    src/import/RebrickableReferenceImporter.h
    resources/resources.qrc
    src/services/ReferenceDataSeeder.cpp
    src/services/ReferenceDataSeeder.h
    src/services/database/DatabaseStatusService.h
    src/services/database/DatabaseStatusService.cpp
    src/services/database/AutomaticBackupPolicy.h
    src/services/database/AutomaticBackupPolicy.cpp
    src/services/database/AutomaticBackupService.h
    src/services/database/AutomaticBackupService.cpp
    src/services/storage/SessionStorageSelectionService.h
    src/services/storage/SessionStorageSelectionService.cpp
    src/services/database/AutomaticBackupRetention.h
    src/services/database/AutomaticBackupRetention.cpp
    src/services/Logger.h
    src/services/Updater.h
    src/services/Updater.cpp
    src/services/Logger.cpp
    src/services/CredentialStore.h
    src/services/CredentialStore.cpp
    src/models/StorageLocationType.h
    src/models/StorageLocationType.cpp
    src/models/StorageLocation.h
    src/models/StorageLocation.cpp
    src/models/CollectionItem.h
    src/models/CollectionItem.cpp
    src/models/CollectionSearchCriteria.h
    src/models/CollectionSearchResult.h
    src/repositories/StorageLocationTypeRepository.h
    src/repositories/StorageLocationTypeRepository.cpp
    src/repositories/StorageLocationRepository.h
    src/repositories/StorageLocationRepository.cpp
    src/repositories/CollectionRepository.h
    src/repositories/CollectionRepository.cpp
    src/services/collection/CollectionItemService.h
    src/services/collection/CollectionItemService.cpp
    src/ui/storage/StorageWidget.cpp
    src/ui/storage/StorageWidget.h
    src/ui/collection/MyCollectionWidget.h
    src/ui/collection/MyCollectionWidget.cpp
    src/ui/collection/CollectionItemDialog.h
    src/ui/collection/CollectionItemDialog.cpp
    src/ui/collection/CatalogCollectionDialog.h
    src/ui/collection/CatalogCollectionDialog.cpp
    src/models/Part.h
    src/models/Part.cpp
    src/models/PartReferenceEntry.h
    src/models/InventoryRecord.h
    src/models/InventoryRecord.cpp
    src/models/Manufacturer.h
    src/repositories/PartRepository.h
    src/repositories/PartRepository.cpp
    src/repositories/InventoryRecordRepository.h
    src/repositories/InventoryRecordRepository.cpp
    src/repositories/ManufacturerRepository.h
    src/repositories/ManufacturerRepository.cpp
    src/services/reference/ManufacturerManagementService.h
    src/services/reference/ManufacturerManagementService.cpp
    src/ui/reference/ManufacturerEditDialog.h
    src/ui/reference/ManufacturerEditDialog.cpp
    src/ui/reference/ReferenceDataDialog.h
    src/ui/reference/ReferenceDataDialog.cpp
    src/ui/database/DatabaseStatusDialog.h
    src/ui/database/DatabaseStatusDialog.cpp
    src/models/Color.h
    src/models/Color.cpp
    src/repositories/ColorRepository.h
    src/repositories/ColorRepository.cpp
    src/models/PartCategory.h
    src/models/PartCategory.cpp
    src/models/ExternalMappingStatus.h
    src/models/ExternalColorMapping.h
    src/models/ExternalPartMapping.h
    src/models/ExternalPartIdentifier.h
    src/models/PartRelationship.h
    src/models/PartAlias.h
    src/models/PartResolutionResult.h
    src/repositories/PartCategoryRepository.h
    src/repositories/PartCategoryRepository.cpp
    src/repositories/ExternalColorMappingRepository.h
    src/repositories/ExternalColorMappingRepository.cpp
    src/repositories/ExternalPartMappingRepository.h
    src/repositories/ExternalPartMappingRepository.cpp
    src/repositories/ExternalPartIdentifierRepository.h
    src/repositories/ExternalPartIdentifierRepository.cpp
    src/repositories/PartRelationshipRepository.h
    src/repositories/PartRelationshipRepository.cpp
    src/repositories/PartAliasRepository.h
    src/repositories/PartAliasRepository.cpp
    src/models/PartSearchCriteria.h
    src/ui/catalog/PartsCatalogWidget.cpp
    src/ui/catalog/PartsCatalogWidget.h
    src/models/PartSearchResult.h
    src/models/InventorySearchCriteria.h
    src/models/InventorySearchResult.h
    src/ui/inventory/MyInventoryWidget.cpp
    src/ui/inventory/MyInventoryWidget.h
    src/ui/inventory/AddInventoryDialog.h
    src/ui/inventory/AddInventoryDialog.cpp
    src/import/RebrickableInventoryImporter.h
    src/import/BrickOwlInventoryImporter.cpp
    src/import/RebrickableInventoryImporter.cpp
    src/ui/inventory/ImportInventoryDialog.h
    src/ui/inventory/ImportInventoryDialog.cpp
    src/ui/inventory/EditInventoryDialog.h
    src/ui/inventory/EditInventoryDialog.cpp
    src/ui/inventory/CorrectInventoryDialog.h
    src/ui/inventory/CorrectInventoryDialog.cpp
    src/ui/inventory/RemoveInventoryDialog.h
    src/ui/inventory/RemoveInventoryDialog.cpp
    src/models/InventoryMovement.h
    src/models/InventoryMovement.cpp
    src/repositories/InventoryMovementRepository.h
    src/repositories/InventoryMovementRepository.cpp
    src/ui/inventory/MoveInventoryDialog.h
    src/ui/inventory/MoveInventoryDialog.cpp
    src/models/InventoryHistoryResult.h
    src/ui/inventory/InventoryHistoryDialog.h
    src/ui/inventory/InventoryHistoryDialog.cpp
    src/settings/UserSettings.cpp src/settings/UserSettings.h
    src/settings/ThemeManager.h
    src/settings/ThemeManager.cpp
    src/ui/settings/SettingsDialog.cpp src/ui/settings/SettingsDialog.h
    src/ui/logging/LogViewerDialog.h
    src/ui/logging/LogViewerDialog.cpp
    src/ui/about/AboutDialog.h
    src/ui/about/AboutDialog.cpp
    src/ui/help/HelpTopic.h
    src/ui/help/HelpManager.h
    src/ui/help/HelpManager.cpp
    src/ui/help/HelpDialog.h
    src/ui/help/HelpDialog.cpp
    resources/help/help.qrc
    src/import/InventoryCsvOperation.h
    src/import/BrickOwlInventoryImporter.h
    src/import/InventoryImportTypes.h
    src/services/inventory/RebrickableInventoryDiffCsvWriter.h
    src/services/inventory/RebrickableInventoryDiffCsvWriter.cpp
    src/ui/inventory/InventoryImportPreviewDialog.h
    src/ui/inventory/InventoryImportPreviewDialog.cpp
    src/api/ApiProvider.h
    src/api/ApiConnectionStatus.h
    src/api/ApiProviderStatusRegistry.h
    src/api/ApiProviderStatusRegistry.cpp
    src/api/ApiError.h
    src/api/ApiResult.h
    src/api/ApiRequestContext.h
    src/api/ApiNetworkService.h
    src/api/ApiNetworkService.cpp
    src/api/rebrickable/RebrickableService.h
    src/api/rebrickable/RebrickableService.cpp
    src/api/brickset/BricksetService.h
    src/api/brickset/BricksetService.cpp
    src/services/RebrickableApiClient.h
    src/services/RebrickableApiClient.cpp
    src/services/images/PartImageService.cpp src/services/images/PartImageService.h
    src/services/parts/PartResolver.h
    src/services/parts/PartResolver.cpp
    src/services/parts/PartReferenceManifest.h
    src/services/parts/PartReferenceManifest.cpp
    src/services/parts/RebrickablePartAliasLearner.h
    src/services/parts/RebrickablePartAliasLearner.cpp
    src/ui/parts/PartDetailsDialog.cpp src/ui/parts/PartDetailsDialog.h
    src/ui/parts/PartReferenceDialog.h
    src/ui/parts/PartReferenceDialog.cpp
    src/ui/parts/PartResolverTestDialog.h
    src/ui/parts/PartResolverTestDialog.cpp
    src/ui/helpers/ColorComboHelper.cpp src/ui/helpers/ColorComboHelper.h
    src/models/Build.h
    src/models/Build.cpp
    src/repositories/BuildRepository.h
    src/repositories/BuildRepository.cpp
    src/ui/builds/BuildsWidget.cpp src/ui/builds/BuildsWidget.h
    src/models/BuildRequirement.h
    src/models/BuildRequirement.cpp
    src/repositories/BuildRequirementRepository.h
    src/repositories/BuildRequirementRepository.cpp
    src/ui/builds/EditBuildRequirementDialog.h
    src/ui/builds/EditBuildRequirementDialog.cpp
    src/models/BuildAllocation.h
    src/models/BuildAllocation.cpp
    src/repositories/BuildAllocationRepository.h
    src/repositories/BuildAllocationRepository.cpp
    src/ui/builds/SetImportPreviewDialog.h
    src/ui/builds/SetImportPreviewDialog.cpp
    src/ui/builds/AllocateBuildRequirementDialog.h
    src/ui/builds/AllocateBuildRequirementDialog.cpp
    src/ui/builds/ImportPullListDialog.h
    src/ui/builds/ImportPullListDialog.cpp
    src/ui/builds/InteractiveBuildPullingDialog.h
    src/ui/builds/InteractiveBuildPullingDialog.cpp
    src/models/SetCatalogItem.h
    src/models/SetCatalogItem.cpp
    src/models/SetCatalogSearchCriteria.h
    src/models/MinifigCatalogItem.h
    src/models/MinifigCatalogItem.cpp
    src/models/MinifigCatalogSearchCriteria.h
    src/models/MinifigCatalogSearchResult.h
    src/models/MinifigExternalIdentifier.h
    src/models/MinifigCatalogPart.h
    src/repositories/SetCatalogRepository.h
    src/repositories/SetCatalogRepository.cpp
    src/repositories/MinifigCatalogRepository.h
    src/repositories/MinifigCatalogRepository.cpp
    src/repositories/MinifigCatalogPartRepository.h
    src/repositories/MinifigCatalogPartRepository.cpp
    src/import/RebrickableSetCatalogImporter.h
    src/import/RebrickableSetCatalogImporter.cpp
    src/import/RebrickableMinifigCatalogImporter.h
    src/import/RebrickableMinifigCatalogImporter.cpp
    src/import/RebrickableMinifigPartsImporter.h
    src/import/RebrickableMinifigPartsImporter.cpp
    src/import/RebrickableMinifigThemeImporter.h
    src/import/RebrickableMinifigThemeImporter.cpp
    src/models/ThemeCatalogItem.h
    src/repositories/ThemeCatalogRepository.h
    src/repositories/ThemeCatalogRepository.cpp
    src/import/RebrickableCsvInputResolver.h
    src/import/RebrickableCsvInputResolver.cpp
    src/ui/catalog/SetsCatalogWidget.h
    src/ui/catalog/SetsCatalogWidget.cpp
    src/ui/builds/EditBuildDialog.h
    src/ui/builds/EditBuildDialog.cpp
    src/services/images/SetImageService.h
    src/services/images/SetImageService.cpp
    src/services/images/ImageUnavailableCache.h
    src/services/images/ImageUnavailableCache.cpp
    src/services/images/MinifigImageService.h
    src/services/images/MinifigImageService.cpp
    src/ui/catalog/SetDetailsDialog.h
    src/ui/catalog/SetDetailsDialog.cpp
    src/ui/catalog/MinifigsCatalogWidget.h
    src/ui/catalog/MinifigsCatalogWidget.cpp
    src/ui/catalog/MinifigDetailsDialog.h
    src/ui/catalog/MinifigDetailsDialog.cpp
    src/ui/catalog/BricksetInstructionsDialog.h
    src/ui/catalog/BricksetInstructionsDialog.cpp
    src/import/RebrickablePartCatalogImporter.h
    src/import/RebrickablePartCatalogImporter.cpp
    src/import/RebrickablePartRelationshipImporter.h
    src/import/RebrickablePartRelationshipImporter.cpp
    src/ui/builds/DisassembleSetDialog.h
    src/ui/builds/DisassembleSetDialog.cpp
    src/services/builds/MissingPartsService.cpp src/services/builds/MissingPartsService.h
    src/models/procurement/ProcurementDraft.h
    src/models/procurement/BrickLinkWantedListOptions.h
    src/services/procurement/ProcurementDraftService.h
    src/services/procurement/ProcurementDraftService.cpp
    src/services/procurement/BrickLinkWantedListXmlWriter.h
    src/services/procurement/BrickLinkWantedListXmlWriter.cpp
    src/ui/procurement/ProcurementPreviewDialog.h
    src/ui/procurement/ProcurementPreviewDialog.cpp
    src/ui/procurement/BrickLinkWantedListResultDialog.h
    src/ui/procurement/BrickLinkWantedListResultDialog.cpp
    src/services/sets/SetDetailsProviderService.h
    src/services/sets/SetDetailsProviderService.cpp
    src/services/mappings/BrickLinkMappingService.h
    src/services/mappings/BrickLinkMappingService.cpp
    src/services/mappings/BrickLinkPartResolver.h
    src/services/mappings/BrickLinkPartResolver.cpp
    src/ui/mappings/BrickLinkColorMappingStatusDialog.h
    src/ui/mappings/BrickLinkColorMappingStatusDialog.cpp
    src/ui/mappings/BrickLinkPartMappingTestDialog.h
    src/ui/mappings/BrickLinkPartMappingTestDialog.cpp
    src/import/RebrickableMocCsvImporter.h
    src/import/RebrickableMocCsvImporter.cpp
    src/services/images/BackgroundPartColorImageCacheService.h
    src/services/images/BackgroundPartColorImageCacheService.cpp
    src/ui/inventory/MarkLostInventoryDialog.h
    src/ui/inventory/MarkLostInventoryDialog.cpp
    src/models/LostInventoryItem.h
    src/repositories/LostInventoryRepository.h
    src/repositories/LostInventoryRepository.cpp
    src/ui/inventory/FoundInventoryDialog.h
    src/ui/inventory/FoundInventoryDialog.cpp
    src/ui/inventory/LostInventoryDialog.h
    src/ui/inventory/LostInventoryDialog.cpp
    src/services/builds/BuildPullingService.h
    src/services/builds/BuildPullingService.cpp
    src/services/builds/MinifigBuildCreationService.h
    src/services/builds/MinifigBuildCreationService.cpp
    src/services/builds/SetBuildCreationService.h
    src/services/builds/SetBuildCreationService.cpp
    src/services/minifigs/MinifigCompositionReplacementService.h
    src/services/minifigs/MinifigCompositionReplacementService.cpp
    src/services/minifigs/RebrickableMinifigPartsService.h
    src/services/minifigs/RebrickableMinifigPartsService.cpp
    src/models/SetCatalogPart.h
    src/repositories/SetCatalogPartRepository.h
    src/repositories/SetCatalogPartRepository.cpp
    src/services/sets/SetCompositionReplacementService.h
    src/services/sets/SetCompositionReplacementService.cpp
    src/services/sets/RebrickableSetPartsService.h
    src/services/sets/RebrickableSetPartsService.cpp
    src/import/RebrickableSetPartsImporter.h
    src/import/RebrickableSetPartsImporter.cpp
    README.md
    src/core/AppVersion.h
    src/core/AppConstants.h
    CONTRIBUTING.md
    SECURITY.md
    deployment/update/update.json

)
target_link_libraries(BrickSuite PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    ZLIB::ZLIB
)

include(CTest)

if(BUILD_TESTING)
    qt_add_executable(CatalogToCollectionTest EXCLUDE_FROM_ALL
        tests/CatalogToCollectionTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/models/Build.h src/models/Build.cpp
        src/models/SetCatalogItem.h src/models/SetCatalogItem.cpp
        src/models/SetCatalogSearchCriteria.h
        src/models/MinifigCatalogItem.h src/models/MinifigCatalogItem.cpp
        src/models/MinifigCatalogSearchCriteria.h src/models/MinifigCatalogSearchResult.h
        src/models/MinifigExternalIdentifier.h src/models/Manufacturer.h
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/CollectionRepository.h src/repositories/CollectionRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/SetCatalogRepository.h src/repositories/SetCatalogRepository.cpp
        src/repositories/MinifigCatalogRepository.h src/repositories/MinifigCatalogRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/services/collection/CollectionItemService.h src/services/collection/CollectionItemService.cpp
        src/services/database/AutomaticBackupPolicy.h src/services/database/AutomaticBackupPolicy.cpp
    )
    target_link_libraries(CatalogToCollectionTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME CatalogToCollection COMMAND CatalogToCollectionTest)

    qt_add_executable(MyCollectionTest EXCLUDE_FROM_ALL
        tests/MyCollectionTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/models/CollectionSearchCriteria.h src/models/CollectionSearchResult.h
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/CollectionRepository.h src/repositories/CollectionRepository.cpp
        src/services/database/AutomaticBackupPolicy.h src/services/database/AutomaticBackupPolicy.cpp
    )
    target_link_libraries(MyCollectionTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME MyCollection COMMAND MyCollectionTest)

    qt_add_executable(CollectionFoundationTest EXCLUDE_FROM_ALL
        tests/CollectionFoundationTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/models/CollectionSearchCriteria.h src/models/CollectionSearchResult.h
        src/models/Build.h src/models/Build.cpp
        src/models/SetCatalogItem.h src/models/SetCatalogItem.cpp
        src/models/SetCatalogSearchCriteria.h
        src/models/MinifigCatalogItem.h src/models/MinifigCatalogItem.cpp
        src/models/MinifigCatalogSearchCriteria.h src/models/MinifigCatalogSearchResult.h
        src/models/MinifigExternalIdentifier.h
        src/models/Manufacturer.h
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/CollectionRepository.h src/repositories/CollectionRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/SetCatalogRepository.h src/repositories/SetCatalogRepository.cpp
        src/repositories/MinifigCatalogRepository.h src/repositories/MinifigCatalogRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/services/collection/CollectionItemService.h
        src/services/collection/CollectionItemService.cpp
        src/services/database/AutomaticBackupPolicy.h
        src/services/database/AutomaticBackupPolicy.cpp
    )
    target_link_libraries(CollectionFoundationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME CollectionFoundation COMMAND CollectionFoundationTest)

    qt_add_executable(SessionStorageSelectionServiceTest EXCLUDE_FROM_ALL
        tests/SessionStorageSelectionServiceTest.cpp
        src/services/storage/SessionStorageSelectionService.h
        src/services/storage/SessionStorageSelectionService.cpp
        src/repositories/StorageLocationRepository.h
        src/repositories/StorageLocationRepository.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
    )
    target_link_libraries(SessionStorageSelectionServiceTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME SessionStorageSelectionService COMMAND SessionStorageSelectionServiceTest)

    qt_add_executable(AutomaticBackupPolicyTest EXCLUDE_FROM_ALL
        tests/AutomaticBackupPolicyTest.cpp
        src/services/database/AutomaticBackupPolicy.h
        src/services/database/AutomaticBackupPolicy.cpp
        src/services/database/AutomaticBackupRetention.h
        src/services/database/AutomaticBackupRetention.cpp
    )
    target_link_libraries(AutomaticBackupPolicyTest PRIVATE Qt6::Core)
    add_test(NAME AutomaticBackupPolicy COMMAND AutomaticBackupPolicyTest)

    qt_add_executable(AutomaticBackupServiceTest EXCLUDE_FROM_ALL
        tests/AutomaticBackupServiceTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/settings/UserSettings.h src/settings/UserSettings.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
        src/services/database/AutomaticBackupPolicy.h
        src/services/database/AutomaticBackupPolicy.cpp
        src/services/database/AutomaticBackupService.h
        src/services/database/AutomaticBackupService.cpp
        src/services/database/DatabaseStatusService.h
        src/services/database/DatabaseStatusService.cpp
        src/services/database/AutomaticBackupRetention.h
        src/services/database/AutomaticBackupRetention.cpp
    )
    target_link_libraries(AutomaticBackupServiceTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME AutomaticBackupService COMMAND AutomaticBackupServiceTest)

    qt_add_executable(DatabaseStatusServiceTest EXCLUDE_FROM_ALL
        tests/DatabaseStatusServiceTest.cpp
        src/services/database/DatabaseStatusService.h
        src/services/database/DatabaseStatusService.cpp
    )
    target_link_libraries(DatabaseStatusServiceTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME DatabaseStatusService COMMAND DatabaseStatusServiceTest)

    qt_add_executable(DatabaseRecoveryGuidanceTest EXCLUDE_FROM_ALL
        tests/DatabaseRecoveryGuidanceTest.cpp
        src/ui/database/DatabaseStatusDialog.h
        src/ui/database/DatabaseStatusDialog.cpp
        src/services/database/DatabaseStatusService.h
        src/services/database/DatabaseStatusService.cpp
        src/services/Logger.h src/services/Logger.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
        src/settings/UserSettings.h src/settings/UserSettings.cpp
        src/ui/help/HelpTopic.h
        src/ui/help/HelpManager.h src/ui/help/HelpManager.cpp
        src/ui/help/HelpDialog.h src/ui/help/HelpDialog.cpp
        resources/help/help.qrc
    )
    target_link_libraries(DatabaseRecoveryGuidanceTest PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql)
    target_compile_definitions(DatabaseRecoveryGuidanceTest PRIVATE
        BRICKSUITE_VERSION="${PROJECT_VERSION}")
    add_test(NAME DatabaseRecoveryGuidance COMMAND DatabaseRecoveryGuidanceTest)
    set_tests_properties(DatabaseRecoveryGuidance PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(ManufacturerManagementTest EXCLUDE_FROM_ALL
        tests/ManufacturerManagementTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Manufacturer.h
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/services/reference/ManufacturerManagementService.h
        src/services/reference/ManufacturerManagementService.cpp
    )
    target_link_libraries(ManufacturerManagementTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME ManufacturerManagement COMMAND ManufacturerManagementTest)

    qt_add_executable(MinifigCatalogFoundationTest EXCLUDE_FROM_ALL
        tests/MinifigCatalogFoundationTest.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h
        src/database/DatabaseSchema.cpp
        src/models/MinifigCatalogItem.h
        src/models/MinifigCatalogItem.cpp
        src/models/MinifigCatalogSearchCriteria.h
        src/models/MinifigCatalogSearchResult.h
        src/models/MinifigExternalIdentifier.h
        src/repositories/MinifigCatalogRepository.h
        src/repositories/MinifigCatalogRepository.cpp
        src/import/RebrickableCsvInputResolver.h
        src/import/RebrickableCsvInputResolver.cpp
        src/import/RebrickableMinifigCatalogImporter.h
        src/import/RebrickableMinifigCatalogImporter.cpp
        src/import/RebrickableMinifigThemeImporter.h
        src/import/RebrickableMinifigThemeImporter.cpp
        src/models/ThemeCatalogItem.h
        src/repositories/ThemeCatalogRepository.h
        src/repositories/ThemeCatalogRepository.cpp
    )

    target_link_libraries(MinifigCatalogFoundationTest PRIVATE
        Qt6::Core
        Qt6::Sql
        ZLIB::ZLIB
    )

    add_test(NAME MinifigCatalogFoundation COMMAND MinifigCatalogFoundationTest)

    qt_add_executable(MinifigImageServiceTest EXCLUDE_FROM_ALL
        tests/MinifigImageServiceTest.cpp
        src/services/images/ImageUnavailableCache.h
        src/services/images/ImageUnavailableCache.cpp
        src/services/images/MinifigImageService.h
        src/services/images/MinifigImageService.cpp
    )

    target_link_libraries(MinifigImageServiceTest PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
    )

    add_test(NAME MinifigImageService COMMAND MinifigImageServiceTest)

    qt_add_executable(CatalogImageNegativeCacheTest EXCLUDE_FROM_ALL
        tests/CatalogImageNegativeCacheTest.cpp
        src/services/images/ImageUnavailableCache.h
        src/services/images/ImageUnavailableCache.cpp
        src/services/images/MinifigImageService.h
        src/services/images/MinifigImageService.cpp
        src/services/images/SetImageService.h
        src/services/images/SetImageService.cpp
    )

    target_link_libraries(CatalogImageNegativeCacheTest PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
    )

    add_test(NAME CatalogImageNegativeCache COMMAND CatalogImageNegativeCacheTest)

    qt_add_executable(MinifigPartsImportTest EXCLUDE_FROM_ALL
        tests/MinifigPartsImportTest.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h
        src/database/DatabaseSchema.cpp
        src/models/MinifigCatalogPart.h
        src/repositories/MinifigCatalogPartRepository.h
        src/repositories/MinifigCatalogPartRepository.cpp
        src/import/RebrickableCsvInputResolver.h
        src/import/RebrickableCsvInputResolver.cpp
        src/import/RebrickableMinifigPartsImporter.h
        src/import/RebrickableMinifigPartsImporter.cpp
        src/services/minifigs/MinifigCompositionReplacementService.h
        src/services/minifigs/MinifigCompositionReplacementService.cpp
    )

    target_link_libraries(MinifigPartsImportTest PRIVATE
        Qt6::Core
        Qt6::Sql
        ZLIB::ZLIB
    )

    add_test(NAME MinifigPartsImport COMMAND MinifigPartsImportTest)

    qt_add_executable(MinifigPartsApiTest EXCLUDE_FROM_ALL
        tests/MinifigPartsApiTest.cpp
        src/api/ApiNetworkService.h src/api/ApiNetworkService.cpp
        src/api/rebrickable/RebrickableService.h src/api/rebrickable/RebrickableService.cpp
        src/settings/UserSettings.h src/settings/UserSettings.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
    )
    target_link_libraries(MinifigPartsApiTest PRIVATE
        Qt6::Core Qt6::Network
    )
    if(WIN32)
        target_link_libraries(MinifigPartsApiTest PRIVATE Advapi32)
    endif()
    add_test(NAME MinifigPartsApi COMMAND MinifigPartsApiTest)

    qt_add_executable(MinifigBuildCreationTest EXCLUDE_FROM_ALL
        tests/MinifigBuildCreationTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/Build.h src/models/Build.cpp
        src/models/BuildRequirement.h src/models/BuildRequirement.cpp
        src/models/MinifigCatalogItem.h src/models/MinifigCatalogItem.cpp
        src/models/MinifigCatalogSearchCriteria.h
        src/models/MinifigCatalogSearchResult.h
        src/models/MinifigExternalIdentifier.h
        src/models/MinifigCatalogPart.h
        src/models/Manufacturer.h
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/BuildRequirementRepository.h src/repositories/BuildRequirementRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/MinifigCatalogRepository.h src/repositories/MinifigCatalogRepository.cpp
        src/repositories/MinifigCatalogPartRepository.h src/repositories/MinifigCatalogPartRepository.cpp
        src/services/builds/MinifigBuildCreationService.h
        src/services/builds/MinifigBuildCreationService.cpp
    )
    target_link_libraries(MinifigBuildCreationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME MinifigBuildCreation COMMAND MinifigBuildCreationTest)

    qt_add_executable(SetCompositionFoundationTest EXCLUDE_FROM_ALL
        tests/SetCompositionFoundationTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/SetCatalogPart.h
        src/models/Build.h src/models/Build.cpp
        src/models/Manufacturer.h
        src/repositories/SetCatalogPartRepository.h src/repositories/SetCatalogPartRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/import/RebrickableCsvInputResolver.h src/import/RebrickableCsvInputResolver.cpp
        src/import/RebrickableSetPartsImporter.h src/import/RebrickableSetPartsImporter.cpp
        src/services/sets/SetCompositionReplacementService.h
        src/services/sets/SetCompositionReplacementService.cpp
    )
    target_link_libraries(SetCompositionFoundationTest PRIVATE Qt6::Core Qt6::Sql ZLIB::ZLIB)
    add_test(NAME SetCompositionFoundation COMMAND SetCompositionFoundationTest)

    qt_add_executable(SetPartsApiTest EXCLUDE_FROM_ALL
        tests/SetPartsApiTest.cpp
        src/api/ApiNetworkService.h src/api/ApiNetworkService.cpp
        src/api/rebrickable/RebrickableService.h src/api/rebrickable/RebrickableService.cpp
        src/settings/UserSettings.h src/settings/UserSettings.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
    )
    target_link_libraries(SetPartsApiTest PRIVATE Qt6::Core Qt6::Network)
    if(WIN32)
        target_link_libraries(SetPartsApiTest PRIVATE Advapi32)
    endif()
    add_test(NAME SetPartsApi COMMAND SetPartsApiTest)

    qt_add_executable(SetBuildCreationTest EXCLUDE_FROM_ALL
        tests/SetBuildCreationTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/Build.h src/models/Build.cpp
        src/models/BuildRequirement.h src/models/BuildRequirement.cpp
        src/models/SetCatalogItem.h src/models/SetCatalogItem.cpp
        src/models/SetCatalogSearchCriteria.h
        src/models/SetCatalogPart.h
        src/models/Manufacturer.h
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/BuildRequirementRepository.h src/repositories/BuildRequirementRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/SetCatalogRepository.h src/repositories/SetCatalogRepository.cpp
        src/repositories/SetCatalogPartRepository.h src/repositories/SetCatalogPartRepository.cpp
        src/services/builds/SetBuildCreationService.h src/services/builds/SetBuildCreationService.cpp
        src/services/sets/SetCompositionReplacementService.h src/services/sets/SetCompositionReplacementService.cpp
    )
    target_link_libraries(SetBuildCreationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME SetBuildCreation COMMAND SetBuildCreationTest)

    qt_add_executable(V020ToV030MigrationTest EXCLUDE_FROM_ALL
        tests/V020ToV030MigrationTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
    )
    target_link_libraries(V020ToV030MigrationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME V020ToV030Migration COMMAND V020ToV030MigrationTest)

    qt_add_executable(PartRelationshipImportTest EXCLUDE_FROM_ALL
        tests/PartRelationshipImportTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/import/RebrickableCsvInputResolver.h src/import/RebrickableCsvInputResolver.cpp
        src/import/RebrickablePartRelationshipImporter.h src/import/RebrickablePartRelationshipImporter.cpp
    )
    target_link_libraries(PartRelationshipImportTest PRIVATE Qt6::Core Qt6::Sql ZLIB::ZLIB)
    add_test(NAME PartRelationshipImport COMMAND PartRelationshipImportTest)
endif()

if(WIN32)
    set_target_properties(BrickSuite PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    target_link_libraries(BrickSuite PRIVATE
        Advapi32
    )
endif()

if(APPLE)
    set_target_properties(BrickSuite PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "BrickSuite"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.rfstateside.bricksuite"
    )

    target_link_libraries(BrickSuite PRIVATE
        "-framework Security"
    )
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS BrickSuite
        RUNTIME DESTINATION bin
    )
endif()



# --------------------------------------------------------------------
# App constants
# --------------------------------------------------------------------
set(APP_NAME              "BrickSuite")
set(APP_COMPANY           "RF StateSide, LLC")
set(APP_ORGANIZATION_NAME "RFStateSide")
set(APP_DOMAIN            "rfstateside.com")
set(APP_COPYRIGHT_YEAR    "2026")
set(APP_RELEASE           "Development")
set(APP_RELEASE_DATE      "2026-08-17")
set(APP_UPDATE_MANIFEST_URL "https://raw.githubusercontent.com/RayJr2/BrickSuite/master/deployment/update/update.json")

# --------------------------------------------------------------------
# Windows deployment metadata
# --------------------------------------------------------------------
if(WIN32)
    file(MAKE_DIRECTORY
        "${CMAKE_BINARY_DIR}/deployment/windows"
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/deployment/windows/BrickSuiteVersion.iss.in"
        "${CMAKE_BINARY_DIR}/deployment/windows/BrickSuiteVersion.iss"
        @ONLY
    )
endif()

target_compile_definitions(BrickSuite PRIVATE
    BRICKSUITE_VERSION="${PROJECT_VERSION}"
    APP_NAME="${APP_NAME}"
    APP_COMPANY="${APP_COMPANY}"
    APP_ORGANIZATION_NAME="${APP_ORGANIZATION_NAME}"
    APP_DOMAIN="${APP_DOMAIN}"
    APP_COPYRIGHT_YEAR="${APP_COPYRIGHT_YEAR}"
    APP_RELEASE="${APP_RELEASE}"
    APP_RELEASE_DATE="${APP_RELEASE_DATE}"
    APP_UPDATE_MANIFEST_URL="${APP_UPDATE_MANIFEST_URL}"
)

set(BRICKSUITE_REBRICKABLE_API_BLOCKED OFF)

if (BRICKSUITE_REBRICKABLE_API_BLOCKED)
    target_compile_definitions(BrickSuite PRIVATE
        BRICKSUITE_REBRICKABLE_API_BLOCKED=1
    )
else()
    target_compile_definitions(BrickSuite PRIVATE
        BRICKSUITE_REBRICKABLE_API_BLOCKED=0
    )
endif()
