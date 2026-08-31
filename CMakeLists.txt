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
    src/repositories/StorageLocationTypeRepository.h
    src/repositories/StorageLocationTypeRepository.cpp
    src/repositories/StorageLocationRepository.h
    src/repositories/StorageLocationRepository.cpp
    src/ui/storage/StorageWidget.cpp
    src/ui/storage/StorageWidget.h
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
    src/models/SetCatalogItem.h
    src/models/SetCatalogItem.cpp
    src/models/SetCatalogSearchCriteria.h
    src/repositories/SetCatalogRepository.h
    src/repositories/SetCatalogRepository.cpp
    src/import/RebrickableSetCatalogImporter.h
    src/import/RebrickableSetCatalogImporter.cpp
    src/ui/catalog/SetsCatalogWidget.h
    src/ui/catalog/SetsCatalogWidget.cpp
    src/ui/builds/EditBuildDialog.h
    src/ui/builds/EditBuildDialog.cpp
    src/services/images/SetImageService.h
    src/services/images/SetImageService.cpp
    src/ui/catalog/SetDetailsDialog.h
    src/ui/catalog/SetDetailsDialog.cpp
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
    README.md
    src/core/AppVersion.h
    src/core/AppConstants.h
    CONTRIBUTING.md
    SECURITY.md

)
target_link_libraries(BrickSuite PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
)

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