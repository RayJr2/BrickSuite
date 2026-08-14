cmake_minimum_required(VERSION 3.16)

project(BrickSuite
    VERSION 1.0.0
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
    src/app/WorkspaceContext.h
    src/app/WorkspaceContext.cpp
    src/import/RebrickableReferenceImporter.cpp
    src/import/RebrickableReferenceImporter.h
    resources/resources.qrc
    src/services/ReferenceDataSeeder.cpp
    src/services/ReferenceDataSeeder.h
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
    src/models/InventoryRecord.h
    src/models/InventoryRecord.cpp
    src/repositories/PartRepository.h
    src/repositories/PartRepository.cpp
    src/repositories/InventoryRecordRepository.h
    src/repositories/InventoryRecordRepository.cpp
    src/models/Color.h
    src/models/Color.cpp
    src/repositories/ColorRepository.h
    src/repositories/ColorRepository.cpp
    src/models/PartCategory.h
    src/models/PartCategory.cpp
    src/repositories/PartCategoryRepository.h
    src/repositories/PartCategoryRepository.cpp
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
    src/import/RebrickableInventoryImportPreview.h
    src/ui/inventory/InventoryImportPreviewDialog.h
    src/ui/inventory/InventoryImportPreviewDialog.cpp
    src/services/RebrickableApiClient.h
    src/services/RebrickableApiClient.cpp
    src/services/images/PartImageService.cpp src/services/images/PartImageService.h
    src/ui/parts/PartDetailsDialog.cpp src/ui/parts/PartDetailsDialog.h
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

)
target_link_libraries(BrickSuite PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
)

set_target_properties(BrickSuite PROPERTIES
    WIN32_EXECUTABLE TRUE
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