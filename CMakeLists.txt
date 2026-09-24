cmake_minimum_required(VERSION 3.16)

project(BrickSuite
    VERSION 0.4.0
    LANGUAGES CXX
)

include(cmake/BrickSuiteMcut.cmake)
include(cmake/BrickSuiteLib3mf.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Network
    WebSockets
    Concurrent
    OpenGL
    OpenGLWidgets
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
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

qt_standard_project_setup()

qt_add_executable(BrickSuite
    src/main.cpp

    src/app/Application.h
    src/app/Application.cpp
    src/app/WorkspaceContext.h
    src/app/WorkspaceContext.cpp

    src/models/WhatCanIBuildPartSelection.h

    src/network/BrickSuiteProtocol.h
    src/network/BrickSuiteProtocol.cpp
    src/network/BrickSuiteAuthentication.h
    src/network/BrickSuiteAuthentication.cpp
    src/network/HostAuthenticationThrottle.h
    src/network/HostAuthenticationThrottle.cpp
    src/network/BrickSuiteHostIdentity.h
    src/network/BrickSuiteHostIdentity.cpp
    src/network/PairedDeviceRegistry.h
    src/network/PairedDeviceRegistry.cpp
    src/network/BrickSuitePairingService.h
    src/network/BrickSuitePairingService.cpp
    src/network/PairedDeviceAdministrationService.h
    src/network/PairedDeviceAdministrationService.cpp
    src/network/BrickSuiteConnectionState.h
    src/network/OperationalInvalidation.h
    src/network/OperationalInvalidation.cpp
    src/network/OperationalInvalidationPublisher.h
    src/network/OperationalInvalidationPublisher.cpp
    src/network/BrickSuiteOperationDispatcher.h
    src/network/BrickSuiteOperationDispatcher.cpp
    src/network/HostRequestContext.h
    src/network/HostRequestAdmissionController.h
    src/network/HostRequestAdmissionController.cpp
    src/network/HostOutboundPolicy.h
    src/network/HostOutboundPolicy.cpp
    src/network/BrickSuiteWebSocketServer.h
    src/network/BrickSuiteWebSocketServer.cpp
    src/network/BrickSuiteWebSocketClient.h
    src/network/BrickSuiteWebSocketClient.cpp
    src/network/BrickSuiteNetworkManager.h
    src/network/BrickSuiteNetworkManager.cpp
    src/network/RemoteSessionState.h
    src/network/RemoteSessionState.cpp
    src/services/application/RemoteRefreshCoordinator.h
    src/services/application/RemoteRefreshCoordinator.cpp
    src/services/application/RebrickableCoordinationService.h
    src/services/application/RebrickableCoordinationService.cpp
    src/services/application/HostMaintenanceCoordinator.h
    src/services/application/HostMaintenanceCoordinator.cpp
    src/services/application/HostOperationalGate.h
    src/services/application/HostMutationPublicationService.h
    src/services/application/HostMutationPublicationService.cpp
    src/services/application/HostWriteExecutor.h
    src/services/application/HostWriteExecutor.cpp
    src/services/application/HostMutationProtocolService.h
    src/services/application/HostMutationProtocolService.cpp
    src/services/application/RemoteMutationApplicationServices.h
    src/services/application/RemoteMutationApplicationServices.cpp
    src/services/application/dto/RemoteMutationDtos.h
    src/services/application/dto/RemoteMutationDtos.cpp
    src/services/application/dto/RemotePullingMutationDtos.h
    src/services/application/dto/RemotePullingMutationDtos.cpp
    src/services/application/dto/RemoteInventoryMutationDtos.h
    src/services/application/dto/RemoteInventoryMutationDtos.cpp
    src/services/application/dto/RemoteStorageMutationDtos.h
    src/services/application/dto/RemoteStorageMutationDtos.cpp
    src/services/application/dto/RemotePartReferenceMutationDtos.h
    src/services/application/dto/RemotePartReferenceMutationDtos.cpp
    src/services/application/dto/RemoteCollectionMutationDtos.h
    src/services/application/dto/RemoteCollectionMutationDtos.cpp
    src/services/application/dto/RemoteBuildabilityDtos.h
    src/services/application/dto/RemoteBuildabilityDtos.cpp
    src/services/application/dto/RemoteBuildMutationDtos.h
    src/services/application/dto/RemoteBuildMutationDtos.cpp
    src/services/application/HostPullingMutationService.h
    src/services/application/HostPullingMutationService.cpp
    src/services/application/HostInventoryMutationService.h
    src/services/application/HostInventoryMutationService.cpp
    src/services/application/HostStorageProtocolMutationService.h
    src/services/application/HostStorageProtocolMutationService.cpp
    src/services/application/HostStorageMutationService.h
    src/services/application/HostStorageMutationService.cpp
    src/services/application/HostPartReferenceMutationService.h
    src/services/application/HostPartReferenceMutationService.cpp
    src/services/application/HostCollectionMutationService.h
    src/services/application/HostCollectionMutationService.cpp
    src/services/application/CollectionDisassemblyPlanService.h
    src/services/application/CollectionDisassemblyPlanService.cpp
    src/services/application/HostBuildMutationService.h
    src/services/application/HostBuildMutationService.cpp
    src/services/application/RemotePullingApplicationService.h
    src/services/application/RemotePullingApplicationService.cpp
    src/services/application/RemoteInventoryMutationApplicationService.h
    src/services/application/RemoteInventoryMutationApplicationService.cpp
    src/services/application/RemoteStorageMutationApplicationService.h
    src/services/application/RemoteStorageMutationApplicationService.cpp
    src/services/application/RemotePartReferenceMutationApplicationService.h
    src/services/application/RemotePartReferenceMutationApplicationService.cpp
    src/services/application/RemoteCollectionMutationApplicationService.h
    src/services/application/RemoteCollectionMutationApplicationService.cpp
    src/services/application/RemoteBuildMutationApplicationService.h
    src/services/application/RemoteBuildMutationApplicationService.cpp
    src/ui/collection/RemoteCollectionMutationDialog.h
    src/ui/collection/RemoteCollectionMutationDialog.cpp
    src/ui/collection/RemoteCollectionStorageFilter.h

    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/database/DatabaseSchema.h
    src/database/DatabaseSchema.cpp

    src/network/HostDataEpoch.h
    src/network/HostDataEpoch.cpp

    src/models/Workspace.h
    src/models/Workspace.cpp

    src/repositories/WorkspaceRepository.h
    src/repositories/WorkspaceRepository.cpp
    src/repositories/RepositoryConnection.h
    src/models/RemoteMutationReceipt.h
    src/repositories/RemoteMutationReceiptRepository.h
    src/repositories/RemoteMutationReceiptRepository.cpp

    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/common/SessionFileDialogDirectoryService.h
    src/ui/common/SessionFileDialogDirectoryService.cpp
    src/ui/inventory/RemoteInventoryMutationDialog.h
    src/ui/inventory/RemoteInventoryMutationDialog.cpp
    src/import/RebrickableReferenceImporter.cpp
    src/import/RebrickableReferenceImporter.h
    src/import/global/RebrickableImportTypes.h
    src/import/global/RebrickableImportTypes.cpp
    src/import/global/RebrickableDatasetRegistry.h
    src/import/global/RebrickableDatasetRegistry.cpp
    src/import/global/RebrickableImportDiscoveryService.h
    src/import/global/RebrickableImportDiscoveryService.cpp
    src/import/global/RebrickableImportCancellation.h
    src/import/global/RebrickableImportPlanController.h
    src/import/global/RebrickableImportPlanController.cpp
    src/import/global/RebrickableWorkerDatabaseSession.h
    src/import/global/RebrickableWorkerDatabaseSession.cpp
    src/import/global/RebrickableGlobalImportService.h
    src/import/global/RebrickableGlobalImportService.cpp
    src/import/RebrickableThemeCatalogImporter.h
    src/import/RebrickableThemeCatalogImporter.cpp
    src/import/RebrickableInventoryCompositionImporter.h
    src/import/RebrickableInventoryCompositionImporter.cpp
    src/import/RebrickableElementImporter.h
    src/import/RebrickableElementImporter.cpp
    src/repositories/SetInventoryRevisionRepository.h
    src/repositories/SetInventoryRevisionRepository.cpp
    src/repositories/PartElementIdentifierRepository.h
    src/repositories/PartElementIdentifierRepository.cpp
    src/services/parts/ElementIdentityService.h
    src/services/parts/ElementIdentityService.cpp
    src/repositories/EffectiveSetCompositionRepository.h
    src/repositories/EffectiveSetCompositionRepository.cpp
    src/ui/import/GlobalRebrickableImportDialog.h
    src/ui/import/GlobalRebrickableImportDialog.cpp
    resources/resources.qrc
    src/services/ReferenceDataSeeder.cpp
    src/services/ReferenceDataSeeder.h
    src/services/database/DatabaseStatusService.h
    src/services/database/DatabaseStatusService.cpp
    src/services/database/DatabaseFileValidator.h
    src/services/database/DatabaseFileValidator.cpp
    src/services/database/DatabaseRestoreTransaction.h
    src/services/database/DatabaseRestoreTransaction.cpp
    src/services/database/DatabaseRestoreCoordinator.h
    src/services/database/DatabaseRestoreCoordinator.cpp
    src/services/database/AutomaticBackupPolicy.h
    src/services/database/AutomaticBackupPolicy.cpp
    src/services/database/AutomaticBackupService.h
    src/services/database/AutomaticBackupService.cpp
    src/services/storage/SessionStorageSelectionService.h
    src/services/storage/SessionStorageSelectionService.cpp
    src/services/storage/RemoteStorageDestination.h
    src/services/application/ApplicationServices.h
    src/services/application/ApplicationServices.cpp
    src/services/application/ApplicationServicesCore.cpp
    src/services/application/UnavailableApplicationServices.cpp
    src/services/application/HostReadExecutor.h
    src/services/application/HostReadExecutor.cpp
    src/services/application/AsyncReadResult.h
    src/services/application/dto/RemoteReadDtos.h
    src/services/application/dto/RemoteReadJson.h
    src/services/application/dto/RemoteReadJson.cpp
    src/services/application/RemoteReadApplicationServices.h
    src/services/application/RemoteReadApplicationServices.cpp
    src/services/application/HostReadProtocolService.h
    src/services/application/HostReadProtocolService.cpp
    src/services/application/LocalReferenceDecoration.h
    src/services/application/SharedDataSource.h
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
    src/services/collection/CollectionDisassemblyService.h
    src/services/collection/CollectionDisassemblyService.cpp
    src/ui/storage/StorageWidget.cpp
    src/ui/storage/StorageTreeVisibility.h
    src/ui/storage/StorageLocationDialog.h
    src/ui/storage/StorageLocationDialog.cpp
    src/ui/storage/RemoteStorageActionEligibility.h
    src/ui/storage/StorageWidget.h
    src/ui/collection/MyCollectionWidget.h
    src/ui/collection/MyCollectionWidget.cpp
    src/ui/collection/CollectionExportDialog.h
    src/ui/collection/CollectionExportDialog.cpp
    src/ui/collection/CollectionItemDialog.h
    src/ui/collection/CollectionItemDialog.cpp
    src/ui/collection/CatalogCollectionDialog.h
    src/ui/collection/CatalogCollectionDialog.cpp
    src/models/Part.h
    src/models/Part.cpp
    src/models/PartReferenceEntry.h
    src/models/UserPartReferenceEntry.h
    src/models/InventoryRecord.h
    src/models/InventoryRecord.cpp
    src/models/Manufacturer.h
    src/repositories/PartRepository.h
    src/repositories/PartRepository.cpp
    src/repositories/UserPartReferenceRepository.h
    src/repositories/UserPartReferenceRepository.cpp
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
    src/ui/inventory/InventoryExportDialog.cpp
    src/ui/inventory/InventoryExportDialog.h
    src/ui/inventory/AddInventoryDialog.h
    src/ui/inventory/AddInventoryDialog.cpp
    src/ui/inventory/AddInventoryColorSelection.h
    src/ui/inventory/AddInventoryDialogButtonState.h
    src/import/RebrickableInventoryImporter.h
    src/import/BrickOwlInventoryImporter.cpp
    src/import/RebrickableInventoryImporter.cpp
    src/ui/inventory/ImportInventoryDialog.h
    src/ui/inventory/ImportInventoryDialog.cpp
    src/ui/inventory/EditInventoryDialog.h
    src/ui/inventory/EditInventoryDialog.cpp
    src/ui/inventory/EditInventorySaveState.h
    src/ui/inventory/InventoryColorAuditReviewDialog.h
    src/ui/inventory/InventoryColorAuditReviewDialog.cpp
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
    src/services/inventory/InventoryColorAuditCsv.h
    src/services/inventory/InventoryColorAuditCsv.cpp
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
    src/api/brickset/BricksetUsagePolicy.h
    src/api/brickset/BricksetUsagePolicy.cpp
    src/services/RebrickableApiClient.h
    src/services/RebrickableApiClient.cpp
    src/services/images/PartImageService.cpp src/services/images/PartImageService.h
    src/services/parts/PartResolver.h
    src/services/parts/PartResolver.cpp
    src/services/parts/PartReferenceManifest.h
    src/services/parts/PartReferenceManifest.cpp
    src/services/parts/PartReferenceCustomizationService.h
    src/services/parts/PartReferenceCustomizationService.cpp
    src/services/parts/RebrickablePartAliasLearner.h
    src/services/parts/RebrickablePartAliasLearner.cpp
    src/services/parts/PartExternalIdEnrichmentService.h
    src/services/parts/PartExternalIdEnrichmentService.cpp
    src/services/parts/BrickLinkCandidateDiscoveryService.h
    src/services/parts/BrickLinkCandidateDiscoveryService.cpp
    src/ui/parts/PartDetailsDialog.cpp src/ui/parts/PartDetailsDialog.h
    src/ui/parts/LDrawModelViewerManager.cpp src/ui/parts/LDrawModelViewerManager.h
    src/ui/parts/LDrawModelViewerWindow.cpp src/ui/parts/LDrawModelViewerWindow.h
    src/ui/parts/ManufacturingMeshDiagnosticDialog.cpp src/ui/parts/ManufacturingMeshDiagnosticDialog.h
    src/ui/parts/LDrawViewportWidget.cpp src/ui/parts/LDrawViewportWidget.h
    src/ui/parts/PrintPreparationCoordinator.cpp src/ui/parts/PrintPreparationCoordinator.h
    src/ui/parts/PreparedMeshRenderAdapter.cpp src/ui/parts/PreparedMeshRenderAdapter.h
    src/ui/parts/FitCalibrationDialog.cpp src/ui/parts/FitCalibrationDialog.h
    src/ui/parts/FitCalibrationFamilyCatalog.cpp src/ui/parts/FitCalibrationFamilyCatalog.h
    src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
    src/services/geometry/LDrawLibraryService.cpp src/services/geometry/LDrawLibraryService.h
    src/services/geometry/LDrawIdentityService.cpp src/services/geometry/LDrawIdentityService.h
    src/services/geometry/LDrawIdentitySelection.h
    src/services/geometry/LDrawObjWriter.cpp src/services/geometry/LDrawObjWriter.h
    src/services/geometry/BinaryStlWriter.cpp src/services/geometry/BinaryStlWriter.h
    src/services/geometry/ThreeMfWriter.cpp src/services/geometry/ThreeMfWriter.h
    src/services/geometry/ModelExportPolicy.h
    src/services/geometry/print/PrintMesh.h src/services/geometry/print/PreparedMesh.h
    src/services/geometry/print/SemanticOperand.h src/services/geometry/print/LDrawPrintPreparationProfile.h
    src/services/geometry/print/FunctionalOperandRegenerator.cpp src/services/geometry/print/FunctionalOperandRegenerator.h
    src/services/geometry/print/FrictionlessTechnicPinSemantic.cpp src/services/geometry/print/FrictionlessTechnicPinSemantic.h
    src/services/geometry/print/FrictionTechnicPinSemantic.cpp src/services/geometry/print/FrictionTechnicPinSemantic.h
    src/services/geometry/print/TechnicAxleSemantic.cpp src/services/geometry/print/TechnicAxleSemantic.h
    src/services/geometry/print/RoundTechnicPassageSemantic.cpp src/services/geometry/print/RoundTechnicPassageSemantic.h
    src/services/geometry/print/StudReceivingPostSemantic.cpp src/services/geometry/print/StudReceivingPostSemantic.h
    src/services/geometry/print/StudReceivingWallPocketSemantic.cpp src/services/geometry/print/StudReceivingWallPocketSemantic.h
    src/services/geometry/print/StudReceivingAntiStudSemantic.cpp src/services/geometry/print/StudReceivingAntiStudSemantic.h
    src/services/geometry/print/StandardBarSemantic.cpp src/services/geometry/print/StandardBarSemantic.h
    src/services/geometry/print/CClipBarReceiverSemantic.cpp src/services/geometry/print/CClipBarReceiverSemantic.h
    src/services/geometry/print/BallJointSemantic.cpp src/services/geometry/print/BallJointSemantic.h
    src/services/geometry/print/BallSocketSemantic.cpp src/services/geometry/print/BallSocketSemantic.h
    src/services/geometry/print/PinBarrelHingeSemantic.cpp src/services/geometry/print/PinBarrelHingeSemantic.h
    src/services/geometry/print/InterleavedFingerHingeSemantic.cpp src/services/geometry/print/InterleavedFingerHingeSemantic.h
    src/services/geometry/print/ClickHingeSemantic.cpp src/services/geometry/print/ClickHingeSemantic.h
    src/services/geometry/print/RetainedRotatingWheelSemantic.cpp src/services/geometry/print/RetainedRotatingWheelSemantic.h
    src/services/geometry/fit/FitCalibrationExperiment.cpp src/services/geometry/fit/FitCalibrationExperiment.h
    src/services/geometry/fit/FitCalibrationLibrary.cpp src/services/geometry/fit/FitCalibrationLibrary.h
    src/services/geometry/fit/FitCalibrationPackage.cpp src/services/geometry/fit/FitCalibrationPackage.h
    src/services/geometry/fit/FitCalibrationFixtureLabel.cpp src/services/geometry/fit/FitCalibrationFixtureLabel.h
    src/services/geometry/fit/RoundTechnicCalibrationArtifact.cpp src/services/geometry/fit/RoundTechnicCalibrationArtifact.h
    src/services/geometry/fit/StandardStudCalibrationArtifact.cpp src/services/geometry/fit/StandardStudCalibrationArtifact.h
    src/services/geometry/fit/StudReceivingCalibrationArtifact.cpp src/services/geometry/fit/StudReceivingCalibrationArtifact.h
    src/services/geometry/fit/StandardBarCalibrationArtifact.cpp src/services/geometry/fit/StandardBarCalibrationArtifact.h
    src/services/geometry/fit/CClipBarReceiverCalibrationArtifact.cpp src/services/geometry/fit/CClipBarReceiverCalibrationArtifact.h
    src/services/geometry/fit/BallJointCalibrationArtifact.cpp src/services/geometry/fit/BallJointCalibrationArtifact.h
    src/services/geometry/fit/BallSocketCalibrationArtifact.cpp src/services/geometry/fit/BallSocketCalibrationArtifact.h
    src/services/geometry/fit/PinBarrelHingeCalibrationArtifact.cpp src/services/geometry/fit/PinBarrelHingeCalibrationArtifact.h
    src/services/geometry/fit/InterleavedFingerHingeCalibrationArtifact.cpp src/services/geometry/fit/InterleavedFingerHingeCalibrationArtifact.h
    src/services/geometry/fit/ClickHingeCalibrationArtifact.cpp src/services/geometry/fit/ClickHingeCalibrationArtifact.h
    src/services/geometry/fit/RetainedRotatingWheelCalibrationArtifact.cpp src/services/geometry/fit/RetainedRotatingWheelCalibrationArtifact.h
    src/services/geometry/fit/FrictionlessTechnicPinCalibrationArtifact.cpp src/services/geometry/fit/FrictionlessTechnicPinCalibrationArtifact.h
    src/services/geometry/fit/FrictionTechnicPinCalibrationArtifact.cpp src/services/geometry/fit/FrictionTechnicPinCalibrationArtifact.h
    src/services/geometry/fit/TechnicAxleCalibrationArtifact.cpp src/services/geometry/fit/TechnicAxleCalibrationArtifact.h
    src/services/geometry/fit/TechnicAxleHoleCalibrationArtifact.cpp src/services/geometry/fit/TechnicAxleHoleCalibrationArtifact.h
    src/services/geometry/print/PrintMeshConversion.cpp src/services/geometry/print/PrintMeshConversion.h
    src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
    src/services/geometry/print/McutMeshBooleanService.cpp src/services/geometry/print/McutMeshBooleanService.h
    src/services/geometry/print/MeshBooleanService.h src/services/geometry/print/PrintPreparation.h
    src/services/geometry/print/PrintPreparationCache.cpp src/services/geometry/print/PrintPreparationCache.h
    src/services/geometry/print/LDrawPrintPreparationService.cpp src/services/geometry/print/LDrawPrintPreparationService.h
    src/services/geometry/print/LDrawPrintGeometryBuilder.cpp src/services/geometry/print/LDrawPrintGeometryBuilder.h
    src/services/geometry/print/LDrawCertifiedInterfaceStitcher.cpp src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h
    src/services/geometry/print/SourceSurfaceSolidifier.cpp src/services/geometry/print/SourceSurfaceSolidifier.h
    src/services/geometry/print/PreparedObjProofWriter.cpp src/services/geometry/print/PreparedObjProofWriter.h
    src/services/geometry/print/ManufacturingMesh.h
    src/services/geometry/print/ManufacturingMeshService.cpp src/services/geometry/print/ManufacturingMeshService.h
    src/services/geometry/print/ManufacturingMeshDiagnosticExporter.cpp src/services/geometry/print/ManufacturingMeshDiagnosticExporter.h
    src/services/geometry/print/AutoFitProfileResolver.cpp src/services/geometry/print/AutoFitProfileResolver.h
    src/services/geometry/ConditionalEdgeVisibility.cpp src/services/geometry/ConditionalEdgeVisibility.h
    src/services/geometry/LDrawColorResolver.cpp src/services/geometry/LDrawColorResolver.h
    src/services/geometry/PartViewerCamera.cpp src/services/geometry/PartViewerCamera.h
    src/services/geometry/PartViewerState.h
    src/services/geometry/PrintOrientation.h
    src/ui/parts/PartReferenceDialog.h
    src/ui/parts/PartReferenceDialog.cpp
    src/ui/parts/AddPartReferenceDialog.h
    src/ui/parts/AddPartReferenceDialog.cpp
    src/ui/parts/PartResolverTestDialog.h
    src/ui/parts/PartResolverTestDialog.cpp
    src/ui/helpers/ColorComboHelper.cpp src/ui/helpers/ColorComboHelper.h
    src/ui/helpers/PartSearchCompleterHelper.cpp src/ui/helpers/PartSearchCompleterHelper.h
    src/ui/helpers/LargeViewLoadingGuard.h
    src/models/Build.h
    src/models/Build.cpp
    src/repositories/BuildRepository.h
    src/repositories/BuildRepository.cpp
    src/ui/builds/BuildsWidget.cpp src/ui/builds/BuildsWidget.h
    src/ui/builds/MissingPartsExportPreparationState.h
    src/ui/builds/MissingPartsExportDialog.cpp src/ui/builds/MissingPartsExportDialog.h
    src/ui/builds/WhatCanIBuildWidget.cpp src/ui/builds/WhatCanIBuildWidget.h
    src/models/PartUsageDiscovery.h
    src/repositories/PartUsageDiscoveryRepository.cpp src/repositories/PartUsageDiscoveryRepository.h
    src/services/builds/PartUsageDiscoveryService.cpp src/services/builds/PartUsageDiscoveryService.h
    src/models/InventoryBuildability.h src/models/InventoryBuildability.cpp
    src/repositories/InventoryBuildabilityRepository.h src/repositories/InventoryBuildabilityRepository.cpp
    src/services/builds/InventoryBuildabilityService.h src/services/builds/InventoryBuildabilityService.cpp
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
    src/models/export/MissingPartsExportTypes.h
    src/services/builds/MissingPartsExportService.cpp src/services/builds/MissingPartsExportService.h
    src/models/export/InventoryExportTypes.h
    src/services/inventory/InventoryExportService.cpp src/services/inventory/InventoryExportService.h
    src/services/inventory/InventoryCsvWriter.cpp src/services/inventory/InventoryCsvWriter.h
    src/models/export/CollectionExportTypes.h
    src/services/collection/CollectionExportService.cpp src/services/collection/CollectionExportService.h
    src/services/collection/CollectionCsvWriter.cpp src/services/collection/CollectionCsvWriter.h
    src/services/builds/MissingPartsCsvWriter.cpp src/services/builds/MissingPartsCsvWriter.h
    src/services/builds/PickABrickExportService.cpp src/services/builds/PickABrickExportService.h
    src/services/builds/PickABrickCsvWriter.cpp src/services/builds/PickABrickCsvWriter.h
    src/services/builds/PickABrickPartResolutionService.cpp src/services/builds/PickABrickPartResolutionService.h
    src/models/procurement/ProcurementDraft.h
    src/models/procurement/BrickLinkWantedListOptions.h
    src/services/procurement/ProcurementDraftService.h
    src/services/procurement/ProcurementDraftService.cpp
    src/services/procurement/ProcurementPartEnrichmentSession.h
    src/services/procurement/ProcurementPartEnrichmentSession.cpp
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
    src/services/builds/BuildMutationService.h
    src/services/builds/BuildMutationService.cpp
    src/services/builds/BuildRequirementMutationService.h
    src/services/builds/BuildRequirementMutationService.cpp
    src/services/builds/BuildAllocationMutationService.h
    src/services/builds/BuildAllocationMutationService.cpp
    src/services/builds/BuildLifecycleService.h
    src/services/builds/BuildLifecycleService.cpp
    src/services/builds/BuildRequirementAvailabilityService.h
    src/services/builds/BuildRequirementAvailabilityService.cpp
    src/services/builds/MinifigBuildCreationService.h
    src/services/builds/MinifigBuildCreationService.cpp
    src/services/builds/SetBuildCreationService.h
    src/services/builds/SetBuildCreationService.cpp
    src/services/minifigs/MinifigCompositionReplacementService.h
    src/services/minifigs/MinifigCompositionReplacementService.cpp
    src/services/minifigs/RebrickableMinifigPartsService.h
    src/services/minifigs/RebrickableMinifigPartsService.cpp
    src/services/minifigs/RebrickableMinifigThemeDerivationService.h
    src/services/minifigs/RebrickableMinifigThemeDerivationService.cpp
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
    docs/BRICKSUITE_PROTOCOL_1_1.md
    docs/BRICKSUITE_PROTOCOL_1_2.md

)
target_link_libraries(BrickSuite PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::WebSockets
    Qt6::Concurrent
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    ZLIB::ZLIB
    OpenSSL::Crypto
    BrickSuite::Mcut BrickSuite::Lib3mf
)
bricksuite_stage_mcut_runtime(BrickSuite)

include(CTest)

if(BUILD_TESTING)
    qt_add_executable(PrintMeshAnalysisTest EXCLUDE_FROM_ALL tests/PrintMeshAnalysisTest.cpp
        src/services/geometry/print/PrintMesh.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h)
    target_link_libraries(PrintMeshAnalysisTest PRIVATE Qt6::Core)
    add_test(NAME PrintMeshAnalysis COMMAND PrintMeshAnalysisTest)

    qt_add_executable(McutMeshBooleanTest EXCLUDE_FROM_ALL tests/McutMeshBooleanTest.cpp
        src/services/geometry/print/PrintMesh.h src/services/geometry/print/LDrawPrintPreparationProfile.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
        src/services/geometry/print/McutMeshBooleanService.cpp src/services/geometry/print/McutMeshBooleanService.h)
    target_link_libraries(McutMeshBooleanTest PRIVATE Qt6::Core BrickSuite::Mcut)
    bricksuite_stage_mcut_runtime(McutMeshBooleanTest)
    add_test(NAME McutMeshBoolean COMMAND McutMeshBooleanTest)

    qt_add_executable(LDrawCertifiedInterfaceStitcherTest EXCLUDE_FROM_ALL
        tests/LDrawCertifiedInterfaceStitcherTest.cpp
        src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/print/LDrawPrintPreparationProfile.h
        src/services/geometry/print/LDrawCertifiedInterfaceStitcher.cpp src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h)
    target_link_libraries(LDrawCertifiedInterfaceStitcherTest PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME LDrawCertifiedInterfaceStitcher COMMAND LDrawCertifiedInterfaceStitcherTest)

    qt_add_executable(SourceSurfaceSolidifierTest EXCLUDE_FROM_ALL
        tests/SourceSurfaceSolidifierTest.cpp
        src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/LDrawLibraryService.cpp src/services/geometry/LDrawLibraryService.h
        src/services/geometry/print/PrintMesh.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
        src/services/geometry/print/SourceSurfaceSolidifier.cpp src/services/geometry/print/SourceSurfaceSolidifier.h)
    target_link_libraries(SourceSurfaceSolidifierTest PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME SourceSurfaceSolidifier COMMAND SourceSurfaceSolidifierTest)

    qt_add_executable(LDrawSemanticOperandTest EXCLUDE_FROM_ALL tests/LDrawSemanticOperandTest.cpp
        src/ui/parts/PreparedMeshRenderAdapter.cpp src/ui/parts/PreparedMeshRenderAdapter.h
        src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/LDrawLibraryService.cpp src/services/geometry/LDrawLibraryService.h
        src/services/geometry/print/PrintMesh.h src/services/geometry/print/SemanticOperand.h
        src/services/geometry/print/LDrawPrintPreparationProfile.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
        src/services/geometry/print/McutMeshBooleanService.cpp src/services/geometry/print/McutMeshBooleanService.h
        src/services/geometry/print/MeshBooleanService.h src/services/geometry/print/PrintPreparation.h
        src/services/geometry/print/PrintPreparationCache.cpp src/services/geometry/print/PrintPreparationCache.h
        src/services/geometry/print/LDrawPrintPreparationService.cpp src/services/geometry/print/LDrawPrintPreparationService.h
        src/services/geometry/print/SourceSurfaceSolidifier.cpp src/services/geometry/print/SourceSurfaceSolidifier.h
        src/services/geometry/print/LDrawPrintGeometryBuilder.cpp src/services/geometry/print/LDrawPrintGeometryBuilder.h
        src/services/geometry/print/LDrawCertifiedInterfaceStitcher.cpp src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h
        src/services/geometry/print/PreparedObjProofWriter.cpp src/services/geometry/print/PreparedObjProofWriter.h)
    target_sources(LDrawSemanticOperandTest PRIVATE src/services/geometry/print/StudReceivingWallPocketSemantic.cpp src/services/geometry/print/StudReceivingAntiStudSemantic.cpp src/services/geometry/print/StandardBarSemantic.cpp src/services/geometry/print/CClipBarReceiverSemantic.cpp src/services/geometry/print/BallJointSemantic.cpp src/services/geometry/print/BallSocketSemantic.cpp src/services/geometry/print/PinBarrelHingeSemantic.cpp src/services/geometry/print/InterleavedFingerHingeSemantic.cpp src/services/geometry/print/ClickHingeSemantic.cpp src/services/geometry/print/RetainedRotatingWheelSemantic.cpp)
    target_link_libraries(LDrawSemanticOperandTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut)
    bricksuite_stage_mcut_runtime(LDrawSemanticOperandTest)
    add_test(NAME LDrawSemanticOperand COMMAND LDrawSemanticOperandTest)

    qt_add_executable(FunctionalOperandRegeneratorTest EXCLUDE_FROM_ALL
        tests/FunctionalOperandRegeneratorTest.cpp
        src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/LDrawLibraryService.cpp src/services/geometry/LDrawLibraryService.h
        src/services/geometry/print/PrintMesh.h src/services/geometry/print/SemanticOperand.h
        src/services/geometry/print/LDrawPrintPreparationProfile.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
        src/services/geometry/print/MeshBooleanService.h
        src/services/geometry/print/McutMeshBooleanService.cpp src/services/geometry/print/McutMeshBooleanService.h
        src/services/geometry/print/LDrawCertifiedInterfaceStitcher.cpp src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h
        src/services/geometry/print/LDrawPrintGeometryBuilder.cpp src/services/geometry/print/LDrawPrintGeometryBuilder.h
        src/services/geometry/print/FunctionalOperandRegenerator.cpp src/services/geometry/print/FunctionalOperandRegenerator.h
        src/services/geometry/print/FrictionlessTechnicPinSemantic.cpp src/services/geometry/print/FrictionlessTechnicPinSemantic.h
        src/services/geometry/print/FrictionTechnicPinSemantic.cpp src/services/geometry/print/FrictionTechnicPinSemantic.h
        src/services/geometry/print/TechnicAxleSemantic.cpp src/services/geometry/print/TechnicAxleSemantic.h
        src/services/geometry/print/StudReceivingPostSemantic.cpp src/services/geometry/print/StudReceivingPostSemantic.h
        src/services/geometry/print/StudReceivingWallPocketSemantic.cpp src/services/geometry/print/StudReceivingWallPocketSemantic.h
        src/services/geometry/print/StudReceivingAntiStudSemantic.cpp src/services/geometry/print/StudReceivingAntiStudSemantic.h)
    target_sources(FunctionalOperandRegeneratorTest PRIVATE src/services/geometry/print/StandardBarSemantic.cpp src/services/geometry/print/CClipBarReceiverSemantic.cpp src/services/geometry/print/BallJointSemantic.cpp src/services/geometry/print/BallSocketSemantic.cpp)

    target_link_libraries(FunctionalOperandRegeneratorTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut)
    bricksuite_stage_mcut_runtime(FunctionalOperandRegeneratorTest)
    add_test(NAME FunctionalOperandRegenerator COMMAND FunctionalOperandRegeneratorTest)

    qt_add_executable(FitCalibrationArtifactTest EXCLUDE_FROM_ALL
        tests/FitCalibrationArtifactTest.cpp
        src/services/geometry/LDrawLoadResult.h src/services/geometry/LDrawSourceModel.h src/services/geometry/PartMesh.h
        src/services/geometry/LDrawLibraryService.cpp src/services/geometry/LDrawLibraryService.h
        src/services/geometry/ThreeMfWriter.cpp src/services/geometry/ThreeMfWriter.h
        src/services/geometry/fit/FitCalibrationExperiment.cpp src/services/geometry/fit/FitCalibrationExperiment.h
        src/services/geometry/fit/FitCalibrationLibrary.cpp src/services/geometry/fit/FitCalibrationLibrary.h
        src/services/geometry/fit/FitCalibrationPackage.cpp src/services/geometry/fit/FitCalibrationPackage.h
        src/services/geometry/fit/FitCalibrationFixtureLabel.cpp src/services/geometry/fit/FitCalibrationFixtureLabel.h
        src/services/geometry/fit/RoundTechnicCalibrationArtifact.cpp src/services/geometry/fit/RoundTechnicCalibrationArtifact.h
        src/services/geometry/fit/StandardStudCalibrationArtifact.cpp src/services/geometry/fit/StandardStudCalibrationArtifact.h
        src/services/geometry/fit/StudReceivingCalibrationArtifact.cpp src/services/geometry/fit/StudReceivingCalibrationArtifact.h
        src/services/geometry/fit/StandardBarCalibrationArtifact.cpp src/services/geometry/fit/StandardBarCalibrationArtifact.h
        src/services/geometry/fit/CClipBarReceiverCalibrationArtifact.cpp src/services/geometry/fit/CClipBarReceiverCalibrationArtifact.h
        src/services/geometry/fit/BallJointCalibrationArtifact.cpp src/services/geometry/fit/BallJointCalibrationArtifact.h
        src/services/geometry/fit/BallSocketCalibrationArtifact.cpp src/services/geometry/fit/BallSocketCalibrationArtifact.h
        src/services/geometry/fit/PinBarrelHingeCalibrationArtifact.cpp src/services/geometry/fit/PinBarrelHingeCalibrationArtifact.h
        src/services/geometry/fit/InterleavedFingerHingeCalibrationArtifact.cpp src/services/geometry/fit/InterleavedFingerHingeCalibrationArtifact.h
        src/services/geometry/fit/ClickHingeCalibrationArtifact.cpp src/services/geometry/fit/ClickHingeCalibrationArtifact.h
        src/services/geometry/fit/RetainedRotatingWheelCalibrationArtifact.cpp src/services/geometry/fit/RetainedRotatingWheelCalibrationArtifact.h
        src/services/geometry/fit/FrictionlessTechnicPinCalibrationArtifact.cpp src/services/geometry/fit/FrictionlessTechnicPinCalibrationArtifact.h
        src/services/geometry/fit/FrictionTechnicPinCalibrationArtifact.cpp src/services/geometry/fit/FrictionTechnicPinCalibrationArtifact.h
        src/services/geometry/fit/TechnicAxleCalibrationArtifact.cpp src/services/geometry/fit/TechnicAxleCalibrationArtifact.h
        src/services/geometry/fit/TechnicAxleHoleCalibrationArtifact.cpp src/services/geometry/fit/TechnicAxleHoleCalibrationArtifact.h
        src/services/geometry/print/PrintMesh.h src/services/geometry/print/SemanticOperand.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
        src/services/geometry/print/SourceSurfaceSolidifier.cpp src/services/geometry/print/SourceSurfaceSolidifier.h
        src/services/geometry/print/FunctionalOperandRegenerator.cpp src/services/geometry/print/FunctionalOperandRegenerator.h
        src/services/geometry/print/FrictionlessTechnicPinSemantic.cpp src/services/geometry/print/FrictionlessTechnicPinSemantic.h
        src/services/geometry/print/FrictionTechnicPinSemantic.cpp src/services/geometry/print/FrictionTechnicPinSemantic.h
        src/services/geometry/print/TechnicAxleSemantic.cpp src/services/geometry/print/TechnicAxleSemantic.h
        src/services/geometry/print/StudReceivingPostSemantic.cpp src/services/geometry/print/StudReceivingPostSemantic.h
        src/services/geometry/print/McutMeshBooleanService.cpp src/services/geometry/print/McutMeshBooleanService.h
        src/services/geometry/print/LDrawCertifiedInterfaceStitcher.cpp src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h
        src/services/geometry/print/LDrawPrintGeometryBuilder.cpp src/services/geometry/print/LDrawPrintGeometryBuilder.h)
    target_sources(FitCalibrationArtifactTest PRIVATE src/services/geometry/print/StudReceivingWallPocketSemantic.cpp src/services/geometry/print/StudReceivingAntiStudSemantic.cpp src/services/geometry/print/StandardBarSemantic.cpp src/services/geometry/print/CClipBarReceiverSemantic.cpp src/services/geometry/print/BallJointSemantic.cpp src/services/geometry/print/BallSocketSemantic.cpp src/services/geometry/print/PinBarrelHingeSemantic.cpp src/services/geometry/print/InterleavedFingerHingeSemantic.cpp src/services/geometry/print/ClickHingeSemantic.cpp src/services/geometry/print/RetainedRotatingWheelSemantic.cpp)
    target_link_libraries(FitCalibrationArtifactTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut BrickSuite::Lib3mf)
    bricksuite_stage_mcut_runtime(FitCalibrationArtifactTest)
    add_test(NAME FitCalibrationArtifact COMMAND FitCalibrationArtifactTest)

    get_target_property(_fitArtifactSources FitCalibrationArtifactTest SOURCES)
    list(REMOVE_ITEM _fitArtifactSources tests/FitCalibrationArtifactTest.cpp)
    qt_add_executable(PinBarrelHingeCalibrationTest EXCLUDE_FROM_ALL
        tests/PinBarrelHingeCalibrationTest.cpp ${_fitArtifactSources})
    target_link_libraries(PinBarrelHingeCalibrationTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut BrickSuite::Lib3mf)
    bricksuite_stage_mcut_runtime(PinBarrelHingeCalibrationTest)
    add_test(NAME PinBarrelHingeCalibration COMMAND PinBarrelHingeCalibrationTest)
    qt_add_executable(InterleavedFingerHingeCalibrationTest EXCLUDE_FROM_ALL
        tests/InterleavedFingerHingeCalibrationTest.cpp ${_fitArtifactSources}
        src/services/geometry/print/InterleavedFingerHingeSemantic.h)
    target_link_libraries(InterleavedFingerHingeCalibrationTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut BrickSuite::Lib3mf)
    bricksuite_stage_mcut_runtime(InterleavedFingerHingeCalibrationTest)
    add_test(NAME InterleavedFingerHingeCalibration COMMAND InterleavedFingerHingeCalibrationTest)
    qt_add_executable(ClickHingeCalibrationTest EXCLUDE_FROM_ALL
        tests/ClickHingeCalibrationTest.cpp ${_fitArtifactSources})
    target_link_libraries(ClickHingeCalibrationTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut BrickSuite::Lib3mf)
    bricksuite_stage_mcut_runtime(ClickHingeCalibrationTest)
    add_test(NAME ClickHingeCalibration COMMAND ClickHingeCalibrationTest)
    qt_add_executable(RetainedRotatingWheelCalibrationTest EXCLUDE_FROM_ALL
        tests/RetainedRotatingWheelCalibrationTest.cpp ${_fitArtifactSources})
    target_link_libraries(RetainedRotatingWheelCalibrationTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut BrickSuite::Lib3mf)
    bricksuite_stage_mcut_runtime(RetainedRotatingWheelCalibrationTest)
    add_test(NAME RetainedRotatingWheelCalibration COMMAND RetainedRotatingWheelCalibrationTest)
    qt_add_executable(FitCalibrationPackageTest EXCLUDE_FROM_ALL
        tests/FitCalibrationPackageTest.cpp ${_fitArtifactSources})
    target_link_libraries(FitCalibrationPackageTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut BrickSuite::Lib3mf)
    bricksuite_stage_mcut_runtime(FitCalibrationPackageTest)
    add_test(NAME FitCalibrationPackage COMMAND FitCalibrationPackageTest)

    qt_add_executable(FitCalibrationLibraryTest EXCLUDE_FROM_ALL
        tests/FitCalibrationLibraryTest.cpp
        src/services/geometry/fit/FitCalibrationExperiment.cpp src/services/geometry/fit/FitCalibrationExperiment.h
        src/services/geometry/fit/FitCalibrationLibrary.cpp src/services/geometry/fit/FitCalibrationLibrary.h)
    target_link_libraries(FitCalibrationLibraryTest PRIVATE Qt6::Core)
    add_test(NAME FitCalibrationLibrary COMMAND FitCalibrationLibraryTest)

    qt_add_executable(FitCalibrationFamilyCatalogTest EXCLUDE_FROM_ALL
        tests/FitCalibrationFamilyCatalogTest.cpp
        src/ui/parts/FitCalibrationFamilyCatalog.cpp
        src/ui/parts/FitCalibrationFamilyCatalog.h)
    target_link_libraries(FitCalibrationFamilyCatalogTest PRIVATE Qt6::Core)
    add_test(NAME FitCalibrationFamilyCatalog COMMAND FitCalibrationFamilyCatalogTest)

    qt_add_executable(ManufacturingMeshServiceTest EXCLUDE_FROM_ALL
        tests/ManufacturingMeshServiceTest.cpp
        src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/LDrawLibraryService.cpp src/services/geometry/LDrawLibraryService.h
        src/services/geometry/fit/FitCalibrationExperiment.cpp src/services/geometry/fit/FitCalibrationExperiment.h
        src/services/geometry/fit/FitCalibrationLibrary.cpp src/services/geometry/fit/FitCalibrationLibrary.h
        src/services/geometry/print/ManufacturingMesh.h
        src/services/geometry/print/ManufacturingMeshService.cpp src/services/geometry/print/ManufacturingMeshService.h
        src/services/geometry/print/LDrawPrintPreparationService.cpp src/services/geometry/print/LDrawPrintPreparationService.h
        src/services/geometry/print/PrintPreparationCache.cpp src/services/geometry/print/PrintPreparationCache.h
        src/services/geometry/print/FrictionlessTechnicPinSemantic.cpp src/services/geometry/print/FrictionlessTechnicPinSemantic.h
        src/services/geometry/print/FrictionTechnicPinSemantic.cpp src/services/geometry/print/FrictionTechnicPinSemantic.h
        src/services/geometry/print/TechnicAxleSemantic.cpp src/services/geometry/print/TechnicAxleSemantic.h
        src/services/geometry/print/RoundTechnicPassageSemantic.cpp src/services/geometry/print/RoundTechnicPassageSemantic.h
        src/services/geometry/print/StudReceivingPostSemantic.cpp src/services/geometry/print/StudReceivingPostSemantic.h
        src/services/geometry/print/ManufacturingMeshDiagnosticExporter.cpp src/services/geometry/print/ManufacturingMeshDiagnosticExporter.h
        src/services/geometry/print/AutoFitProfileResolver.cpp src/services/geometry/print/AutoFitProfileResolver.h
        src/services/geometry/ThreeMfWriter.cpp src/services/geometry/ThreeMfWriter.h
        src/services/geometry/print/FunctionalOperandRegenerator.cpp src/services/geometry/print/FunctionalOperandRegenerator.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
        src/services/geometry/print/SourceSurfaceSolidifier.cpp src/services/geometry/print/SourceSurfaceSolidifier.h
        src/services/geometry/print/McutMeshBooleanService.cpp src/services/geometry/print/McutMeshBooleanService.h
        src/services/geometry/print/LDrawPrintGeometryBuilder.cpp src/services/geometry/print/LDrawPrintGeometryBuilder.h
        src/services/geometry/print/LDrawCertifiedInterfaceStitcher.cpp src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h)
    target_sources(ManufacturingMeshServiceTest PRIVATE src/services/geometry/print/StudReceivingWallPocketSemantic.cpp src/services/geometry/print/StudReceivingAntiStudSemantic.cpp src/services/geometry/print/StandardBarSemantic.cpp src/services/geometry/print/CClipBarReceiverSemantic.cpp src/services/geometry/print/BallJointSemantic.cpp src/services/geometry/print/BallSocketSemantic.cpp src/services/geometry/print/PinBarrelHingeSemantic.cpp src/services/geometry/print/InterleavedFingerHingeSemantic.cpp src/services/geometry/print/ClickHingeSemantic.cpp src/services/geometry/print/RetainedRotatingWheelSemantic.cpp)
    target_link_libraries(ManufacturingMeshServiceTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut BrickSuite::Lib3mf)
    bricksuite_stage_mcut_runtime(ManufacturingMeshServiceTest)
    add_test(NAME ManufacturingMeshService COMMAND ManufacturingMeshServiceTest)

    qt_add_executable(LDrawPrintPreparationServiceTest EXCLUDE_FROM_ALL
        tests/LDrawPrintPreparationServiceTest.cpp
        src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/LDrawLibraryService.cpp src/services/geometry/LDrawLibraryService.h
        src/services/geometry/print/PrintMesh.h src/services/geometry/print/PreparedMesh.h
        src/services/geometry/print/SemanticOperand.h src/services/geometry/print/LDrawPrintPreparationProfile.h
        src/services/geometry/print/PrintPreparation.h src/services/geometry/print/MeshBooleanService.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
        src/services/geometry/print/McutMeshBooleanService.cpp src/services/geometry/print/McutMeshBooleanService.h
        src/services/geometry/print/LDrawPrintGeometryBuilder.cpp src/services/geometry/print/LDrawPrintGeometryBuilder.h
        src/services/geometry/print/LDrawCertifiedInterfaceStitcher.cpp src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h
        src/services/geometry/print/PrintPreparationCache.cpp src/services/geometry/print/PrintPreparationCache.h
        src/services/geometry/print/LDrawPrintPreparationService.cpp src/services/geometry/print/LDrawPrintPreparationService.h
        src/services/geometry/print/SourceSurfaceSolidifier.cpp src/services/geometry/print/SourceSurfaceSolidifier.h)
    target_sources(LDrawPrintPreparationServiceTest PRIVATE src/services/geometry/print/StudReceivingWallPocketSemantic.cpp src/services/geometry/print/StudReceivingAntiStudSemantic.cpp src/services/geometry/print/StandardBarSemantic.cpp src/services/geometry/print/CClipBarReceiverSemantic.cpp src/services/geometry/print/BallJointSemantic.cpp src/services/geometry/print/BallSocketSemantic.cpp src/services/geometry/print/PinBarrelHingeSemantic.cpp src/services/geometry/print/InterleavedFingerHingeSemantic.cpp src/services/geometry/print/ClickHingeSemantic.cpp src/services/geometry/print/RetainedRotatingWheelSemantic.cpp)
    target_link_libraries(LDrawPrintPreparationServiceTest PRIVATE Qt6::Core Qt6::Gui BrickSuite::Mcut)
    bricksuite_stage_mcut_runtime(LDrawPrintPreparationServiceTest)
    add_test(NAME LDrawPrintPreparationService COMMAND LDrawPrintPreparationServiceTest)

    qt_add_executable(PreparedMeshTest EXCLUDE_FROM_ALL tests/PreparedMeshTest.cpp
        src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/print/PrintMesh.h src/services/geometry/print/PreparedMesh.h
        src/services/geometry/print/LDrawPrintPreparationProfile.h)
    target_link_libraries(PreparedMeshTest PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME PreparedMesh COMMAND PreparedMeshTest)

    qt_add_executable(PreparedMeshRenderAdapterTest EXCLUDE_FROM_ALL
        tests/PreparedMeshRenderAdapterTest.cpp
        src/ui/parts/PreparedMeshRenderAdapter.cpp src/ui/parts/PreparedMeshRenderAdapter.h
        src/services/geometry/print/PrintMesh.h src/services/geometry/print/PreparedMesh.h
        src/services/geometry/print/SemanticOperand.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/LDrawSourceModel.h src/services/geometry/PartMesh.h)
    target_link_libraries(PreparedMeshRenderAdapterTest PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME PreparedMeshRenderAdapter COMMAND PreparedMeshRenderAdapterTest)

    qt_add_executable(PrintPreparationCoordinatorTest EXCLUDE_FROM_ALL
        tests/PrintPreparationCoordinatorTest.cpp
        src/ui/parts/PrintPreparationCoordinator.cpp src/ui/parts/PrintPreparationCoordinator.h
        src/services/geometry/PartMesh.h src/services/geometry/LDrawSourceModel.h src/services/geometry/LDrawLoadResult.h
        src/services/geometry/print/PrintMesh.h src/services/geometry/print/PreparedMesh.h
        src/services/geometry/print/SemanticOperand.h src/services/geometry/print/LDrawPrintPreparationProfile.h
        src/services/geometry/print/PrintPreparation.h src/services/geometry/print/MeshBooleanService.h
        src/services/geometry/print/PrintMeshAnalysis.cpp src/services/geometry/print/PrintMeshAnalysis.h
        src/services/geometry/print/McutMeshBooleanService.cpp src/services/geometry/print/McutMeshBooleanService.h
        src/services/geometry/print/LDrawPrintGeometryBuilder.cpp src/services/geometry/print/LDrawPrintGeometryBuilder.h
        src/services/geometry/print/LDrawCertifiedInterfaceStitcher.cpp src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h
        src/services/geometry/print/PrintPreparationCache.cpp src/services/geometry/print/PrintPreparationCache.h
        src/services/geometry/print/LDrawPrintPreparationService.cpp src/services/geometry/print/LDrawPrintPreparationService.h
        src/services/geometry/print/SourceSurfaceSolidifier.cpp src/services/geometry/print/SourceSurfaceSolidifier.h)
    target_sources(PrintPreparationCoordinatorTest PRIVATE src/services/geometry/print/StudReceivingWallPocketSemantic.cpp src/services/geometry/print/StudReceivingAntiStudSemantic.cpp src/services/geometry/print/StandardBarSemantic.cpp src/services/geometry/print/CClipBarReceiverSemantic.cpp src/services/geometry/print/BallJointSemantic.cpp src/services/geometry/print/BallSocketSemantic.cpp src/services/geometry/print/PinBarrelHingeSemantic.cpp src/services/geometry/print/InterleavedFingerHingeSemantic.cpp src/services/geometry/print/ClickHingeSemantic.cpp src/services/geometry/print/RetainedRotatingWheelSemantic.cpp)
    target_link_libraries(PrintPreparationCoordinatorTest PRIVATE Qt6::Core Qt6::Gui Qt6::Concurrent BrickSuite::Mcut)
    bricksuite_stage_mcut_runtime(PrintPreparationCoordinatorTest)
    add_test(NAME PrintPreparationCoordinator COMMAND PrintPreparationCoordinatorTest)

    foreach(LDRAW_TEST_TARGET IN ITEMS LDrawLibraryTest LDrawParserTest LDrawGeometryTest LDrawObjExportTest)
        qt_add_executable(${LDRAW_TEST_TARGET} EXCLUDE_FROM_ALL
            tests/LDrawGeometryFoundationTest.cpp
            src/services/geometry/PartMesh.h
            src/services/geometry/LDrawLibraryService.h src/services/geometry/LDrawLibraryService.cpp
            src/services/geometry/LDrawObjWriter.h src/services/geometry/LDrawObjWriter.cpp
            src/services/geometry/BinaryStlWriter.h src/services/geometry/BinaryStlWriter.cpp
            src/services/geometry/ThreeMfWriter.h src/services/geometry/ThreeMfWriter.cpp
            src/services/geometry/ModelExportPolicy.h
            src/services/geometry/print/PrintMeshConversion.h src/services/geometry/print/PrintMeshConversion.cpp)
        target_link_libraries(${LDRAW_TEST_TARGET} PRIVATE Qt6::Core Qt6::Gui BrickSuite::Lib3mf)
        add_test(NAME ${LDRAW_TEST_TARGET} COMMAND ${LDRAW_TEST_TARGET})
    endforeach()

    qt_add_executable(LDrawIdentityResolutionTest EXCLUDE_FROM_ALL
        tests/LDrawIdentityTest.cpp
        src/models/ExternalPartIdentifier.h
        src/services/geometry/LDrawIdentitySelection.h)
    target_link_libraries(LDrawIdentityResolutionTest PRIVATE Qt6::Core)
    add_test(NAME LDrawIdentityResolutionTest COMMAND LDrawIdentityResolutionTest)

    qt_add_executable(PartViewerMathTest EXCLUDE_FROM_ALL
        tests/PartViewerMathTest.cpp
        src/services/geometry/PartMesh.h
        src/services/geometry/ConditionalEdgeVisibility.cpp src/services/geometry/ConditionalEdgeVisibility.h
        src/services/geometry/LDrawColorResolver.cpp src/services/geometry/LDrawColorResolver.h
        src/services/geometry/PartViewerCamera.cpp src/services/geometry/PartViewerCamera.h
        src/services/geometry/PartViewerState.h
        src/services/geometry/PrintOrientation.h)
    target_link_libraries(PartViewerMathTest PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME PartViewerMath COMMAND PartViewerMathTest)

    qt_add_executable(BuildActionEligibilityTest EXCLUDE_FROM_ALL
        tests/BuildActionEligibilityTest.cpp
        src/ui/builds/BuildActionEligibility.h
    )
    target_link_libraries(BuildActionEligibilityTest PRIVATE Qt6::Core)
    add_test(NAME BuildActionEligibility COMMAND BuildActionEligibilityTest)

    qt_add_executable(SingleInstanceWindowRegistryTest EXCLUDE_FROM_ALL
        tests/SingleInstanceWindowRegistryTest.cpp
        src/ui/common/SingleInstanceWindowRegistry.h
    )
    target_link_libraries(SingleInstanceWindowRegistryTest PRIVATE Qt6::Core)
    add_test(NAME SingleInstanceWindowRegistry COMMAND SingleInstanceWindowRegistryTest)

    qt_add_executable(InventoryRemoveHardeningTest EXCLUDE_FROM_ALL
        tests/InventoryRemoveHardeningTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/Manufacturer.h
        src/models/Part.h src/models/Part.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
    )
    target_link_libraries(InventoryRemoveHardeningTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME InventoryRemoveHardening COMMAND InventoryRemoveHardeningTest)

    qt_add_executable(InventoryTransactionSeamsTest EXCLUDE_FROM_ALL
        tests/InventoryTransactionSeamsTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/Manufacturer.h
        src/models/Part.h src/models/Part.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
    )
    target_link_libraries(InventoryTransactionSeamsTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME InventoryTransactionSeams COMMAND InventoryTransactionSeamsTest)

    qt_add_executable(BrickLinkCandidateResolutionTest EXCLUDE_FROM_ALL
        tests/BrickLinkCandidateResolutionTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/PartRelationship.h
        src/models/PartAlias.h
        src/models/PartResolutionResult.h
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/PartRelationshipRepository.h src/repositories/PartRelationshipRepository.cpp
        src/repositories/PartAliasRepository.h src/repositories/PartAliasRepository.cpp
        src/repositories/ExternalPartIdentifierRepository.h src/repositories/ExternalPartIdentifierRepository.cpp
        src/services/parts/PartResolver.h src/services/parts/PartResolver.cpp
        src/services/parts/BrickLinkCandidateDiscoveryService.h
        src/services/parts/BrickLinkCandidateDiscoveryService.cpp
    )
    target_link_libraries(BrickLinkCandidateResolutionTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME BrickLinkCandidateResolution COMMAND BrickLinkCandidateResolutionTest)

    qt_add_executable(BrickLinkProcurementTest EXCLUDE_FROM_ALL
        tests/BrickLinkProcurementTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/ExternalPartIdentifier.h
        src/models/ExternalPartMapping.h
        src/models/ExternalMappingStatus.h
        src/models/procurement/ProcurementDraft.h
        src/models/procurement/BrickLinkWantedListOptions.h
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ExternalPartIdentifierRepository.h src/repositories/ExternalPartIdentifierRepository.cpp
        src/repositories/ExternalPartMappingRepository.h src/repositories/ExternalPartMappingRepository.cpp
        src/services/mappings/BrickLinkPartResolver.h src/services/mappings/BrickLinkPartResolver.cpp
        src/services/procurement/BrickLinkWantedListXmlWriter.h src/services/procurement/BrickLinkWantedListXmlWriter.cpp
    )
    target_link_libraries(BrickLinkProcurementTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME BrickLinkProcurement COMMAND BrickLinkProcurementTest)

    qt_add_executable(ProcurementPartEnrichmentSessionTest EXCLUDE_FROM_ALL
        tests/ProcurementPartEnrichmentSessionTest.cpp
        src/services/procurement/ProcurementPartEnrichmentSession.h
        src/services/procurement/ProcurementPartEnrichmentSession.cpp
    )
    target_link_libraries(ProcurementPartEnrichmentSessionTest PRIVATE Qt6::Core)
    add_test(NAME ProcurementPartEnrichmentSession COMMAND ProcurementPartEnrichmentSessionTest)

    qt_add_executable(PartUsageDiscoveryTest EXCLUDE_FROM_ALL
        tests/PartUsageDiscoveryTest.cpp
        src/models/PartUsageDiscovery.h
        src/repositories/PartUsageDiscoveryRepository.h
        src/repositories/PartUsageDiscoveryRepository.cpp)
    target_link_libraries(PartUsageDiscoveryTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME PartUsageDiscovery COMMAND PartUsageDiscoveryTest)

    qt_add_executable(PartUsageDiscoveryServiceTest EXCLUDE_FROM_ALL
        tests/PartUsageDiscoveryServiceTest.cpp
        src/models/PartUsageDiscovery.h
        src/repositories/PartUsageDiscoveryRepository.h
        src/repositories/PartUsageDiscoveryRepository.cpp
        src/services/builds/PartUsageDiscoveryService.h
        src/services/builds/PartUsageDiscoveryService.cpp)
    target_link_libraries(PartUsageDiscoveryServiceTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME PartUsageDiscoveryService COMMAND PartUsageDiscoveryServiceTest)

    qt_add_executable(InventoryBuildabilityTest EXCLUDE_FROM_ALL
        tests/InventoryBuildabilityTest.cpp
        src/models/InventoryBuildability.h src/models/InventoryBuildability.cpp
        src/repositories/RepositoryConnection.h
        src/repositories/InventoryBuildabilityRepository.h
        src/repositories/InventoryBuildabilityRepository.cpp)
    target_link_libraries(InventoryBuildabilityTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME InventoryBuildability COMMAND InventoryBuildabilityTest)

    qt_add_executable(PartExternalIdEnrichmentServiceTest EXCLUDE_FROM_ALL
        tests/PartExternalIdEnrichmentServiceTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Part.h src/models/Part.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ExternalPartIdentifierRepository.h src/repositories/ExternalPartIdentifierRepository.cpp
        src/repositories/ExternalPartMappingRepository.h src/repositories/ExternalPartMappingRepository.cpp
        src/services/images/PartImageService.h src/services/images/PartImageService.cpp
        src/services/parts/PartExternalIdEnrichmentService.h src/services/parts/PartExternalIdEnrichmentService.cpp
        src/services/mappings/BrickLinkMappingService.h src/services/mappings/BrickLinkMappingService.cpp
        src/services/RebrickableApiClient.h src/services/RebrickableApiClient.cpp
        src/api/rebrickable/RebrickableService.h src/api/rebrickable/RebrickableService.cpp
        src/api/ApiNetworkService.h src/api/ApiNetworkService.cpp
        src/settings/UserSettings.h src/settings/UserSettings.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
        resources/resources.qrc
        src/models/Color.h src/models/Color.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/models/ExternalColorMapping.h
        src/repositories/ExternalColorMappingRepository.h src/repositories/ExternalColorMappingRepository.cpp
    )
    target_link_libraries(PartExternalIdEnrichmentServiceTest PRIVATE Qt6::Core Qt6::Sql Qt6::Network)
    add_test(NAME PartExternalIdEnrichmentService COMMAND PartExternalIdEnrichmentServiceTest)
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

    qt_add_executable(CollectionTransactionSeamsTest EXCLUDE_FROM_ALL
        tests/CollectionTransactionSeamsTest.cpp
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
    target_link_libraries(CollectionTransactionSeamsTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME CollectionTransactionSeams COMMAND CollectionTransactionSeamsTest)

    qt_add_executable(CollectionDisassemblyTest EXCLUDE_FROM_ALL
        tests/CollectionDisassemblyTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Build.h src/models/Build.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/SetCatalogItem.h src/models/SetCatalogItem.cpp
        src/models/MinifigCatalogItem.h src/models/MinifigCatalogItem.cpp
        src/models/MinifigCatalogSearchCriteria.h src/models/MinifigCatalogSearchResult.h
        src/models/MinifigExternalIdentifier.h
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/models/CollectionSearchCriteria.h src/models/CollectionSearchResult.h
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/Color.h src/models/Color.cpp
        src/models/MinifigCatalogPart.h src/models/Manufacturer.h
        src/repositories/CollectionRepository.h src/repositories/CollectionRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/EffectiveSetCompositionRepository.h src/repositories/EffectiveSetCompositionRepository.cpp
        src/repositories/MinifigCatalogPartRepository.h src/repositories/MinifigCatalogPartRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/SetCatalogRepository.h src/repositories/SetCatalogRepository.cpp
        src/repositories/MinifigCatalogRepository.h src/repositories/MinifigCatalogRepository.cpp
        src/services/collection/CollectionItemService.h
        src/services/collection/CollectionItemService.cpp
        src/services/collection/CollectionDisassemblyService.h
        src/services/collection/CollectionDisassemblyService.cpp)
    target_link_libraries(CollectionDisassemblyTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME CollectionDisassembly COMMAND CollectionDisassemblyTest)

    qt_add_executable(RemoteCollectionMutationTest EXCLUDE_FROM_ALL
        tests/RemoteCollectionMutationTest.cpp
        src/database/DatabaseManager.cpp src/database/DatabaseSchema.cpp src/services/database/AutomaticBackupPolicy.cpp
        src/models/Workspace.cpp src/models/StorageLocation.cpp src/models/CollectionItem.cpp
        src/models/Build.cpp src/models/BuildRequirement.cpp src/models/BuildAllocation.cpp
        src/models/InventoryRecord.cpp src/models/InventoryMovement.cpp src/models/Part.cpp src/models/Color.cpp
        src/models/SetCatalogItem.cpp src/models/MinifigCatalogItem.cpp
        src/repositories/WorkspaceRepository.cpp src/repositories/StorageLocationRepository.cpp
        src/repositories/CollectionRepository.cpp src/repositories/BuildRepository.cpp
        src/repositories/BuildRequirementRepository.cpp src/repositories/BuildAllocationRepository.cpp
        src/repositories/InventoryRecordRepository.cpp src/repositories/InventoryMovementRepository.cpp
        src/repositories/PartRepository.cpp src/repositories/ColorRepository.cpp
        src/repositories/EffectiveSetCompositionRepository.cpp src/repositories/MinifigCatalogPartRepository.cpp
        src/repositories/SetCatalogRepository.cpp src/repositories/MinifigCatalogRepository.cpp
        src/repositories/ManufacturerRepository.cpp
        src/services/collection/CollectionItemService.cpp
        src/services/collection/CollectionDisassemblyService.cpp
        src/services/builds/BuildLifecycleService.cpp
        src/services/application/CollectionDisassemblyPlanService.cpp
        src/services/application/HostCollectionMutationService.cpp
        src/services/application/dto/RemoteCollectionMutationDtos.cpp
        src/services/application/dto/RemoteMutationDtos.cpp
    )
    target_link_libraries(RemoteCollectionMutationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME RemoteCollectionMutation COMMAND RemoteCollectionMutationTest)

    qt_add_executable(RemoteBuildMutationTest EXCLUDE_FROM_ALL
        tests/RemoteBuildMutationTest.cpp
        src/services/application/dto/RemoteBuildMutationDtos.h
        src/services/application/dto/RemoteBuildMutationDtos.cpp
        src/services/application/dto/RemoteMutationDtos.h
        src/services/application/dto/RemoteMutationDtos.cpp)
    target_link_libraries(RemoteBuildMutationTest PRIVATE Qt6::Core)
    add_test(NAME RemoteBuildMutation COMMAND RemoteBuildMutationTest)

    qt_add_executable(HostSetBuildMutationTest EXCLUDE_FROM_ALL
        tests/HostSetBuildMutationTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/Build.h src/models/Build.cpp
        src/models/BuildRequirement.h src/models/BuildRequirement.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/models/SetCatalogItem.h src/models/SetCatalogItem.cpp
        src/models/MinifigCatalogItem.h src/models/MinifigCatalogItem.cpp
        src/models/MinifigCatalogSearchCriteria.h src/models/MinifigCatalogSearchResult.h
        src/models/MinifigExternalIdentifier.h
        src/models/Part.h src/models/Part.cpp
        src/models/Color.h src/models/Color.cpp
        src/models/Manufacturer.h
        src/repositories/RepositoryConnection.h
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/BuildRequirementRepository.h src/repositories/BuildRequirementRepository.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/SetCatalogRepository.h src/repositories/SetCatalogRepository.cpp
        src/repositories/MinifigCatalogRepository.h src/repositories/MinifigCatalogRepository.cpp
        src/repositories/EffectiveSetCompositionRepository.h src/repositories/EffectiveSetCompositionRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/CollectionRepository.h src/repositories/CollectionRepository.cpp
        src/repositories/RemoteMutationReceiptRepository.h src/repositories/RemoteMutationReceiptRepository.cpp
        src/services/application/HostOperationalGate.h
        src/services/application/HostBuildMutationService.h src/services/application/HostBuildMutationService.cpp
        src/services/application/HostWriteExecutor.h src/services/application/HostWriteExecutor.cpp
        src/services/application/HostMutationPublicationService.h src/services/application/HostMutationPublicationService.cpp
        src/services/application/dto/RemoteMutationDtos.h src/services/application/dto/RemoteMutationDtos.cpp
        src/services/application/dto/RemoteBuildMutationDtos.h src/services/application/dto/RemoteBuildMutationDtos.cpp
        src/services/builds/BuildMutationService.h src/services/builds/BuildMutationService.cpp
        src/services/builds/BuildRequirementMutationService.h src/services/builds/BuildRequirementMutationService.cpp
        src/services/builds/BuildAllocationMutationService.h src/services/builds/BuildAllocationMutationService.cpp
        src/services/builds/BuildLifecycleService.h src/services/builds/BuildLifecycleService.cpp
        src/services/builds/SetBuildCreationService.h src/services/builds/SetBuildCreationService.cpp
        src/services/collection/CollectionItemService.h src/services/collection/CollectionItemService.cpp
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
    )
    target_link_libraries(HostSetBuildMutationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME HostSetBuildMutation COMMAND HostSetBuildMutationTest)

    qt_add_executable(RemoteCollectionStorageFilterTest EXCLUDE_FROM_ALL
        tests/RemoteCollectionStorageFilterTest.cpp
    )
    target_link_libraries(RemoteCollectionStorageFilterTest PRIVATE Qt6::Core)
    add_test(NAME RemoteCollectionStorageFilter COMMAND RemoteCollectionStorageFilterTest)

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

    qt_add_executable(SessionFileDialogDirectoryServiceTest EXCLUDE_FROM_ALL
        tests/SessionFileDialogDirectoryServiceTest.cpp
        src/ui/common/SessionFileDialogDirectoryService.h
        src/ui/common/SessionFileDialogDirectoryService.cpp
    )
    target_link_libraries(SessionFileDialogDirectoryServiceTest PRIVATE Qt6::Core)
    add_test(NAME SessionFileDialogDirectoryService COMMAND SessionFileDialogDirectoryServiceTest)

    qt_add_executable(StorageMutationServiceTest EXCLUDE_FROM_ALL
        tests/StorageMutationServiceTest.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/StorageLocationType.h src/models/StorageLocationType.cpp
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/StorageLocationTypeRepository.h src/repositories/StorageLocationTypeRepository.cpp
        src/services/application/HostStorageMutationService.h
        src/services/application/HostStorageMutationService.cpp
        src/services/application/HostStorageProtocolMutationService.h
        src/services/application/HostStorageProtocolMutationService.cpp
        src/services/application/HostMutationPublicationService.h
        src/services/application/dto/RemoteMutationDtos.h
        src/services/application/dto/RemoteMutationDtos.cpp
        src/services/application/dto/RemoteReadDtos.h
        src/services/application/dto/RemoteReadJson.h
        src/services/application/dto/RemoteReadJson.cpp
        src/services/application/dto/RemoteStorageMutationDtos.h
        src/services/application/dto/RemoteStorageMutationDtos.cpp
    )
    target_link_libraries(StorageMutationServiceTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME StorageMutationService COMMAND StorageMutationServiceTest)

    qt_add_executable(RemoteStorageUiTest EXCLUDE_FROM_ALL
        tests/RemoteStorageUiTest.cpp
        src/ui/storage/StorageLocationDialog.h src/ui/storage/StorageLocationDialog.cpp
        src/ui/storage/RemoteStorageActionEligibility.h
    )
    target_link_libraries(RemoteStorageUiTest PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    add_test(NAME RemoteStorageUi COMMAND RemoteStorageUiTest)
    set_tests_properties(RemoteStorageUi PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
        src/import/RebrickableThemeCatalogImporter.h
        src/import/RebrickableThemeCatalogImporter.cpp
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
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/InventorySearchCriteria.h src/models/InventorySearchResult.h
        src/models/InventoryHistoryResult.h
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/Color.h src/models/Color.cpp
        src/models/SetCatalogItem.h src/models/SetCatalogItem.cpp
        src/models/SetCatalogSearchCriteria.h
        src/models/MinifigCatalogItem.h src/models/MinifigCatalogItem.cpp
        src/models/MinifigCatalogSearchCriteria.h
        src/models/MinifigCatalogSearchResult.h
        src/models/MinifigExternalIdentifier.h
        src/models/MinifigCatalogPart.h
        src/models/Manufacturer.h
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/BuildRequirementRepository.h src/repositories/BuildRequirementRepository.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/CollectionRepository.h src/repositories/CollectionRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/repositories/SetCatalogRepository.h src/repositories/SetCatalogRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/MinifigCatalogRepository.h src/repositories/MinifigCatalogRepository.cpp
        src/repositories/MinifigCatalogPartRepository.h src/repositories/MinifigCatalogPartRepository.cpp
        src/services/builds/MinifigBuildCreationService.h
        src/services/builds/MinifigBuildCreationService.cpp
        src/services/builds/BuildLifecycleService.h src/services/builds/BuildLifecycleService.cpp
        src/services/builds/BuildRequirementMutationService.h src/services/builds/BuildRequirementMutationService.cpp
        src/services/builds/BuildAllocationMutationService.h src/services/builds/BuildAllocationMutationService.cpp
        src/services/builds/BuildPullingService.h src/services/builds/BuildPullingService.cpp
        src/services/builds/BuildMutationService.h src/services/builds/BuildMutationService.cpp
        src/services/collection/CollectionItemService.h src/services/collection/CollectionItemService.cpp
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
        src/repositories/EffectiveSetCompositionRepository.h src/repositories/EffectiveSetCompositionRepository.cpp
        src/services/builds/SetBuildCreationService.h src/services/builds/SetBuildCreationService.cpp
        src/services/sets/SetCompositionReplacementService.h src/services/sets/SetCompositionReplacementService.cpp
    )
    target_link_libraries(SetBuildCreationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME SetBuildCreation COMMAND SetBuildCreationTest)

    qt_add_executable(BuildMutationTransactionSeamsTest EXCLUDE_FROM_ALL
        tests/BuildMutationTransactionSeamsTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/Build.h src/models/Build.cpp
        src/models/BuildRequirement.h src/models/BuildRequirement.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/Color.h src/models/Color.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/Manufacturer.h
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/BuildRequirementRepository.h src/repositories/BuildRequirementRepository.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/services/builds/BuildMutationService.h src/services/builds/BuildMutationService.cpp
        src/services/builds/BuildRequirementMutationService.h src/services/builds/BuildRequirementMutationService.cpp
        src/services/builds/BuildAllocationMutationService.h src/services/builds/BuildAllocationMutationService.cpp
    )
    target_link_libraries(BuildMutationTransactionSeamsTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME BuildMutationTransactionSeams COMMAND BuildMutationTransactionSeamsTest)

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
    qt_add_executable(ColorComboHelperTest EXCLUDE_FROM_ALL
        tests/ColorComboHelperTest.cpp
        src/ui/helpers/ColorComboHelper.h src/ui/helpers/ColorComboHelper.cpp
    )
    target_link_libraries(ColorComboHelperTest PRIVATE Qt6::Widgets)
    add_test(NAME ColorComboHelper COMMAND ColorComboHelperTest -platform offscreen)
    qt_add_executable(AddInventoryColorSelectionTest EXCLUDE_FROM_ALL
        tests/AddInventoryColorSelectionTest.cpp
        src/models/Color.h src/models/Color.cpp
        src/ui/inventory/AddInventoryColorSelection.h
    )
    target_link_libraries(AddInventoryColorSelectionTest PRIVATE Qt6::Core)
    add_test(NAME AddInventoryColorSelection COMMAND AddInventoryColorSelectionTest)
    qt_add_executable(MyInventoryAddSessionRefreshTest EXCLUDE_FROM_ALL
        tests/MyInventoryAddSessionRefreshTest.cpp
        src/ui/inventory/MyInventoryAddSessionRefresh.h)
    target_link_libraries(MyInventoryAddSessionRefreshTest PRIVATE Qt6::Core)
    add_test(NAME MyInventoryAddSessionRefresh COMMAND MyInventoryAddSessionRefreshTest)
    qt_add_executable(EditInventorySaveStateTest EXCLUDE_FROM_ALL
        tests/EditInventorySaveStateTest.cpp
        src/ui/inventory/EditInventorySaveState.h
    )
    target_link_libraries(EditInventorySaveStateTest PRIVATE Qt6::Core)
    add_test(NAME EditInventorySaveState COMMAND EditInventorySaveStateTest)
    qt_add_executable(InventoryColorAuditCsvTest EXCLUDE_FROM_ALL
        tests/InventoryColorAuditCsvTest.cpp
        src/services/inventory/InventoryColorAuditCsv.h
        src/services/inventory/InventoryColorAuditCsv.cpp
    )
    target_link_libraries(InventoryColorAuditCsvTest PRIVATE Qt6::Core)
    add_test(NAME InventoryColorAuditCsv COMMAND InventoryColorAuditCsvTest)
    qt_add_executable(PartSearchCompleterHelperTest EXCLUDE_FROM_ALL
        tests/PartSearchCompleterHelperTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/PartSearchCriteria.h src/models/PartSearchResult.h
        src/repositories/RepositoryConnection.h
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/ui/helpers/PartSearchCompleterHelper.h src/ui/helpers/PartSearchCompleterHelper.cpp
    )
    target_link_libraries(PartSearchCompleterHelperTest PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql)
    add_test(NAME PartSearchCompleterHelper COMMAND PartSearchCompleterHelperTest)
    set_tests_properties(PartSearchCompleterHelper PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(PartReferenceCustomizationTest EXCLUDE_FROM_ALL
        tests/PartReferenceCustomizationTest.cpp
        resources/resources.qrc
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/PartReferenceEntry.h src/models/UserPartReferenceEntry.h
        src/models/PartSearchCriteria.h src/models/PartSearchResult.h
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/UserPartReferenceRepository.h src/repositories/UserPartReferenceRepository.cpp
        src/services/parts/PartReferenceManifest.h src/services/parts/PartReferenceManifest.cpp
        src/services/parts/PartReferenceCustomizationService.h
        src/services/parts/PartReferenceCustomizationService.cpp
    )
    target_link_libraries(PartReferenceCustomizationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME PartReferenceCustomization COMMAND PartReferenceCustomizationTest)
    qt_add_executable(PartReferenceCardLifetimeTest EXCLUDE_FROM_ALL
        tests/PartReferenceCardLifetimeTest.cpp
        src/ui/parts/PartReferenceCardRegistry.h)
    target_link_libraries(PartReferenceCardLifetimeTest PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    add_test(NAME PartReferenceCardLifetime COMMAND PartReferenceCardLifetimeTest)
    set_tests_properties(PartReferenceCardLifetime PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(PartReferenceRemoteMutationTest EXCLUDE_FROM_ALL
        tests/PartReferenceRemoteMutationTest.cpp resources/resources.qrc
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Part.h src/models/Part.cpp src/models/PartReferenceEntry.h src/models/UserPartReferenceEntry.h
        src/models/PartSearchCriteria.h src/models/PartSearchResult.h
        src/repositories/RepositoryConnection.h
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/UserPartReferenceRepository.h src/repositories/UserPartReferenceRepository.cpp
        src/services/parts/PartReferenceManifest.h src/services/parts/PartReferenceManifest.cpp
        src/services/parts/PartReferenceCustomizationService.h src/services/parts/PartReferenceCustomizationService.cpp
        src/services/application/HostPartReferenceMutationService.h src/services/application/HostPartReferenceMutationService.cpp
        src/services/application/HostMutationPublicationService.h
        src/services/application/dto/RemoteMutationDtos.h src/services/application/dto/RemoteMutationDtos.cpp
        src/services/application/dto/RemotePartReferenceMutationDtos.h src/services/application/dto/RemotePartReferenceMutationDtos.cpp)
    target_link_libraries(PartReferenceRemoteMutationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME PartReferenceRemoteMutation COMMAND PartReferenceRemoteMutationTest)
    qt_add_executable(PagingProjectionTest EXCLUDE_FROM_ALL
        tests/PagingProjectionTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/PartSearchCriteria.h src/models/PartSearchResult.h
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/Manufacturer.h
        src/models/InventorySearchCriteria.h src/models/InventorySearchResult.h
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
    )
    target_link_libraries(PagingProjectionTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME PagingProjection COMMAND PagingProjectionTest)
    qt_add_executable(LargeViewLoadingGuardTest EXCLUDE_FROM_ALL
        tests/LargeViewLoadingGuardTest.cpp
        src/ui/helpers/LargeViewLoadingGuard.h
    )
    target_link_libraries(LargeViewLoadingGuardTest PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    add_test(NAME LargeViewLoadingGuard COMMAND LargeViewLoadingGuardTest)
    set_tests_properties(LargeViewLoadingGuard PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(GlobalRebrickableImporterFoundationTest EXCLUDE_FROM_ALL
        tests/GlobalRebrickableImporterFoundationTest.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/import/RebrickableCsvInputResolver.h src/import/RebrickableCsvInputResolver.cpp
        src/import/global/RebrickableImportTypes.h src/import/global/RebrickableImportTypes.cpp
        src/import/global/RebrickableDatasetRegistry.h src/import/global/RebrickableDatasetRegistry.cpp
        src/import/global/RebrickableImportDiscoveryService.h
        src/import/global/RebrickableImportDiscoveryService.cpp
        src/import/global/RebrickableImportCancellation.h
        src/import/global/RebrickableImportPlanController.h
        src/import/global/RebrickableImportPlanController.cpp
        src/import/global/RebrickableWorkerDatabaseSession.h
        src/import/global/RebrickableWorkerDatabaseSession.cpp
        src/import/global/RebrickableGlobalImportService.h
        src/import/global/RebrickableGlobalImportService.cpp
        src/import/RebrickableReferenceImporter.h src/import/RebrickableReferenceImporter.cpp
        src/import/RebrickableThemeCatalogImporter.h src/import/RebrickableThemeCatalogImporter.cpp
        src/import/RebrickablePartCatalogImporter.h src/import/RebrickablePartCatalogImporter.cpp
        src/import/RebrickablePartRelationshipImporter.h src/import/RebrickablePartRelationshipImporter.cpp
        src/import/RebrickableSetCatalogImporter.h src/import/RebrickableSetCatalogImporter.cpp
        src/import/RebrickableMinifigCatalogImporter.h src/import/RebrickableMinifigCatalogImporter.cpp
        src/import/RebrickableInventoryCompositionImporter.h src/import/RebrickableInventoryCompositionImporter.cpp
        src/import/RebrickableElementImporter.h src/import/RebrickableElementImporter.cpp
        src/services/minifigs/RebrickableMinifigThemeDerivationService.h
        src/services/minifigs/RebrickableMinifigThemeDerivationService.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/models/PartRelationship.h
    )
    target_link_libraries(GlobalRebrickableImporterFoundationTest PRIVATE
        Qt6::Core Qt6::Sql ZLIB::ZLIB)
    add_test(NAME GlobalRebrickableImporterFoundation
        COMMAND GlobalRebrickableImporterFoundationTest)

    qt_add_executable(InventoryCompositionPersistenceTest EXCLUDE_FROM_ALL
        tests/InventoryCompositionPersistenceTest.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/import/RebrickableCsvInputResolver.h src/import/RebrickableCsvInputResolver.cpp
        src/import/global/RebrickableImportCancellation.h
        src/import/RebrickableInventoryCompositionImporter.h
        src/import/RebrickableInventoryCompositionImporter.cpp
        src/repositories/SetInventoryRevisionRepository.h
        src/repositories/SetInventoryRevisionRepository.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
    )
    target_link_libraries(InventoryCompositionPersistenceTest PRIVATE
        Qt6::Core Qt6::Sql ZLIB::ZLIB)
    add_test(NAME InventoryCompositionPersistence
        COMMAND InventoryCompositionPersistenceTest)

    qt_add_executable(ElementsEffectiveCompositionTest EXCLUDE_FROM_ALL
        tests/ElementsEffectiveCompositionTest.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/import/RebrickableCsvInputResolver.h src/import/RebrickableCsvInputResolver.cpp
        src/import/global/RebrickableImportCancellation.h
        src/import/RebrickableElementImporter.h src/import/RebrickableElementImporter.cpp
        src/repositories/PartElementIdentifierRepository.h src/repositories/PartElementIdentifierRepository.cpp
        src/models/Manufacturer.h
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/services/parts/ElementIdentityService.h src/services/parts/ElementIdentityService.cpp
        src/repositories/EffectiveSetCompositionRepository.h src/repositories/EffectiveSetCompositionRepository.cpp
        src/services/minifigs/RebrickableMinifigThemeDerivationService.h
        src/services/minifigs/RebrickableMinifigThemeDerivationService.cpp
    )
    target_link_libraries(ElementsEffectiveCompositionTest PRIVATE
        Qt6::Core Qt6::Sql ZLIB::ZLIB)
    add_test(NAME ElementsEffectiveComposition COMMAND ElementsEffectiveCompositionTest)

    qt_add_executable(ApplicationServiceContractsTest EXCLUDE_FROM_ALL
        tests/ApplicationServiceContractsTest.cpp
        src/models/Build.h src/models/Build.cpp
        src/models/BuildRequirement.h src/models/BuildRequirement.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/services/application/ApplicationServices.h
        src/services/application/ApplicationServicesCore.cpp
        src/services/application/UnavailableApplicationServices.cpp
        src/services/application/SharedDataSource.h
        src/services/parts/PartReferenceManifest.h src/services/parts/PartReferenceManifest.cpp
        src/app/WorkspaceContext.h src/app/WorkspaceContext.cpp
    )
    target_link_libraries(ApplicationServiceContractsTest PRIVATE Qt6::Core)
    add_test(NAME ApplicationServiceContracts COMMAND ApplicationServiceContractsTest)

    qt_add_executable(SharedDataSourceSettingsTest EXCLUDE_FROM_ALL
        tests/SharedDataSourceSettingsTest.cpp
        src/settings/UserSettings.h src/settings/UserSettings.cpp
        src/settings/MeshRepairSettingsPolicy.h
        src/services/application/SharedDataSource.h
        src/services/CredentialStore.h src/services/CredentialStore.cpp
        src/services/database/AutomaticBackupPolicy.h
        src/services/database/AutomaticBackupPolicy.cpp
    )
    target_link_libraries(SharedDataSourceSettingsTest PRIVATE Qt6::Core Qt6::Sql)
    if(WIN32)
        target_link_libraries(SharedDataSourceSettingsTest PRIVATE Advapi32)
    elseif(APPLE)
        target_link_libraries(SharedDataSourceSettingsTest PRIVATE "-framework Security")
    endif()
    add_test(NAME SharedDataSourceSettings COMMAND SharedDataSourceSettingsTest)

    qt_add_executable(MissingPartsExportTest EXCLUDE_FROM_ALL
        tests/MissingPartsExportTest.cpp
        src/models/export/MissingPartsExportTypes.h
        src/models/Build.h src/models/Build.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/Color.h src/models/Color.cpp
        src/models/PartCategory.h src/models/PartCategory.cpp
        src/models/ExternalPartIdentifier.h
        src/models/ExternalPartMapping.h
        src/models/ExternalColorMapping.h
        src/repositories/RepositoryConnection.h
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/repositories/PartCategoryRepository.h src/repositories/PartCategoryRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/PartElementIdentifierRepository.h src/repositories/PartElementIdentifierRepository.cpp
        src/repositories/ExternalPartIdentifierRepository.h src/repositories/ExternalPartIdentifierRepository.cpp
        src/repositories/ExternalPartMappingRepository.h src/repositories/ExternalPartMappingRepository.cpp
        src/repositories/ExternalColorMappingRepository.h src/repositories/ExternalColorMappingRepository.cpp
        src/services/parts/ElementIdentityService.h src/services/parts/ElementIdentityService.cpp
        src/services/builds/MissingPartsExportService.h src/services/builds/MissingPartsExportService.cpp
        src/services/builds/MissingPartsCsvWriter.h src/services/builds/MissingPartsCsvWriter.cpp
        src/services/builds/PickABrickExportService.h src/services/builds/PickABrickExportService.cpp
        src/services/builds/PickABrickCsvWriter.h src/services/builds/PickABrickCsvWriter.cpp
        src/services/builds/PickABrickPartResolutionService.h src/services/builds/PickABrickPartResolutionService.cpp
        src/ui/builds/MissingPartsExportDialog.h src/ui/builds/MissingPartsExportDialog.cpp
        src/ui/builds/MissingPartsExportPreparationState.h
        src/ui/common/SessionFileDialogDirectoryService.h
        src/ui/common/SessionFileDialogDirectoryService.cpp
        src/settings/UserSettings.h src/settings/UserSettings.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
        src/services/database/AutomaticBackupPolicy.h
        src/services/database/AutomaticBackupPolicy.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
    )
    target_link_libraries(MissingPartsExportTest PRIVATE Qt6::Core Qt6::Sql Qt6::Widgets)
    if(WIN32)
        target_link_libraries(MissingPartsExportTest PRIVATE Advapi32)
    elseif(APPLE)
        target_link_libraries(MissingPartsExportTest PRIVATE "-framework Security")
    endif()
    add_test(NAME MissingPartsExport COMMAND MissingPartsExportTest)

    qt_add_executable(InventoryExportTest EXCLUDE_FROM_ALL
        tests/InventoryExportTest.cpp
        src/models/export/InventoryExportTypes.h
        src/models/InventorySearchResult.h
        src/models/Part.h src/models/Part.cpp
        src/models/Manufacturer.h
        src/models/ExternalPartIdentifier.h src/models/ExternalPartMapping.h src/models/ExternalColorMapping.h
        src/repositories/RepositoryConnection.h
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/PartElementIdentifierRepository.h src/repositories/PartElementIdentifierRepository.cpp
        src/repositories/ExternalPartIdentifierRepository.h src/repositories/ExternalPartIdentifierRepository.cpp
        src/repositories/ExternalPartMappingRepository.h src/repositories/ExternalPartMappingRepository.cpp
        src/repositories/ExternalColorMappingRepository.h src/repositories/ExternalColorMappingRepository.cpp
        src/services/parts/ElementIdentityService.h src/services/parts/ElementIdentityService.cpp
        src/services/inventory/InventoryExportService.h src/services/inventory/InventoryExportService.cpp
        src/services/inventory/InventoryCsvWriter.h src/services/inventory/InventoryCsvWriter.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
    )
    target_link_libraries(InventoryExportTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME InventoryExport COMMAND InventoryExportTest)

    qt_add_executable(CollectionExportTest EXCLUDE_FROM_ALL
        tests/CollectionExportTest.cpp
        src/models/export/CollectionExportTypes.h
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/models/CollectionSearchResult.h
        src/services/collection/CollectionExportService.h src/services/collection/CollectionExportService.cpp
        src/services/collection/CollectionCsvWriter.h src/services/collection/CollectionCsvWriter.cpp
    )
    target_link_libraries(CollectionExportTest PRIVATE Qt6::Core)
    add_test(NAME CollectionExport COMMAND CollectionExportTest)

    qt_add_executable(SecureHostFoundationTest EXCLUDE_FROM_ALL
        tests/SecureHostFoundationTest.cpp
        src/network/BrickSuiteProtocol.h src/network/BrickSuiteProtocol.cpp
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
        src/network/OperationalInvalidationPublisher.h src/network/OperationalInvalidationPublisher.cpp
        src/network/BrickSuiteAuthentication.h src/network/BrickSuiteAuthentication.cpp
        src/network/BrickSuiteHostIdentity.h src/network/BrickSuiteHostIdentity.cpp
        src/network/PairedDeviceRegistry.h src/network/PairedDeviceRegistry.cpp
        src/network/BrickSuitePairingService.h src/network/BrickSuitePairingService.cpp
        src/network/PairedDeviceAdministrationService.h src/network/PairedDeviceAdministrationService.cpp
        src/network/BrickSuiteConnectionState.h
        src/network/BrickSuiteOperationDispatcher.h src/network/BrickSuiteOperationDispatcher.cpp
        src/network/BrickSuiteWebSocketServer.h src/network/BrickSuiteWebSocketServer.cpp
        src/network/HostRequestContext.h
        src/network/HostRequestAdmissionController.h src/network/HostRequestAdmissionController.cpp
        src/network/HostOutboundPolicy.h src/network/HostOutboundPolicy.cpp
        src/network/HostAuthenticationThrottle.h src/network/HostAuthenticationThrottle.cpp
        src/network/BrickSuiteWebSocketClient.h src/network/BrickSuiteWebSocketClient.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
    )
    target_compile_definitions(SecureHostFoundationTest PRIVATE
        BRICKSUITE_VERSION="${PROJECT_VERSION}"
        BRICKSUITE_TESTING=1)
    target_link_libraries(SecureHostFoundationTest PRIVATE
        Qt6::Core Qt6::Network Qt6::WebSockets Qt6::Sql OpenSSL::Crypto)
    if(WIN32)
        target_link_libraries(SecureHostFoundationTest PRIVATE Advapi32)
    elseif(APPLE)
        target_link_libraries(SecureHostFoundationTest PRIVATE "-framework Security")
    endif()
    add_test(NAME SecureHostFoundation COMMAND SecureHostFoundationTest)

    qt_add_executable(HostRequestAdmissionControllerTest EXCLUDE_FROM_ALL
        tests/HostRequestAdmissionControllerTest.cpp
        src/network/HostRequestContext.h
        src/network/HostRequestAdmissionController.h
        src/network/HostRequestAdmissionController.cpp
    )
    target_link_libraries(HostRequestAdmissionControllerTest PRIVATE Qt6::Core)
    add_test(NAME HostRequestAdmissionController COMMAND HostRequestAdmissionControllerTest)

    qt_add_executable(HostOutboundPolicyTest EXCLUDE_FROM_ALL
        tests/HostOutboundPolicyTest.cpp
        src/network/BrickSuiteProtocol.h src/network/BrickSuiteProtocol.cpp
        src/network/HostOutboundPolicy.h src/network/HostOutboundPolicy.cpp
    )
    target_link_libraries(HostOutboundPolicyTest PRIVATE Qt6::Core)
    add_test(NAME HostOutboundPolicy COMMAND HostOutboundPolicyTest)

    qt_add_executable(HostReadExecutorTest EXCLUDE_FROM_ALL
        tests/HostReadExecutorTest.cpp
        src/models/InventoryBuildability.h src/models/InventoryBuildability.cpp
        src/repositories/InventoryBuildabilityRepository.h
        src/repositories/InventoryBuildabilityRepository.cpp
        src/services/application/dto/RemoteBuildabilityDtos.h
        src/services/application/dto/RemoteBuildabilityDtos.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/StorageLocationType.h src/models/StorageLocationType.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/InventorySearchCriteria.h src/models/InventorySearchResult.h
        src/models/InventoryHistoryResult.h
        src/models/LostInventoryItem.h
        src/models/Build.h src/models/Build.cpp
        src/models/BuildRequirement.h src/models/BuildRequirement.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/CollectionItem.h src/models/CollectionItem.cpp
        src/models/SetCatalogItem.h src/models/SetCatalogItem.cpp
        src/models/MinifigCatalogItem.h src/models/MinifigCatalogItem.cpp
        src/models/CollectionSearchCriteria.h src/models/CollectionSearchResult.h
        src/models/Part.h src/models/Part.cpp
        src/models/Color.h src/models/Color.cpp
        src/models/PartCategory.h src/models/PartCategory.cpp
        src/models/UserPartReferenceEntry.h
        src/repositories/RepositoryConnection.h
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/StorageLocationTypeRepository.h src/repositories/StorageLocationTypeRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/LostInventoryRepository.h src/repositories/LostInventoryRepository.cpp
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/BuildRequirementRepository.h src/repositories/BuildRequirementRepository.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/repositories/PartCategoryRepository.h src/repositories/PartCategoryRepository.cpp
        src/repositories/CollectionRepository.h src/repositories/CollectionRepository.cpp
        src/repositories/SetCatalogRepository.h src/repositories/SetCatalogRepository.cpp
        src/repositories/MinifigCatalogRepository.h src/repositories/MinifigCatalogRepository.cpp
        src/repositories/UserPartReferenceRepository.h src/repositories/UserPartReferenceRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/PartElementIdentifierRepository.h src/repositories/PartElementIdentifierRepository.cpp
        src/repositories/EffectiveSetCompositionRepository.h src/repositories/EffectiveSetCompositionRepository.cpp
        src/repositories/MinifigCatalogPartRepository.h src/repositories/MinifigCatalogPartRepository.cpp
        src/services/builds/MissingPartsService.h src/services/builds/MissingPartsService.cpp
        src/models/export/MissingPartsExportTypes.h
        src/services/builds/MissingPartsExportService.h src/services/builds/MissingPartsExportService.cpp
        src/services/builds/PickABrickPartResolutionService.h src/services/builds/PickABrickPartResolutionService.cpp
        src/services/builds/PickABrickExportService.h src/services/builds/PickABrickExportService.cpp
        src/repositories/ExternalPartIdentifierRepository.h src/repositories/ExternalPartIdentifierRepository.cpp
        src/repositories/ExternalPartMappingRepository.h src/repositories/ExternalPartMappingRepository.cpp
        src/repositories/ExternalColorMappingRepository.h src/repositories/ExternalColorMappingRepository.cpp
    src/services/builds/BuildPullingService.h src/services/builds/BuildPullingService.cpp
    src/services/builds/BuildRequirementAvailabilityService.h
    src/services/builds/BuildRequirementAvailabilityService.cpp
        src/services/builds/BuildLifecycleService.h src/services/builds/BuildLifecycleService.cpp
        src/services/collection/CollectionItemService.h src/services/collection/CollectionItemService.cpp
        src/services/collection/CollectionDisassemblyService.h src/services/collection/CollectionDisassemblyService.cpp
        src/services/parts/PartReferenceManifest.h src/services/parts/PartReferenceManifest.cpp
        src/services/parts/PartReferenceCustomizationService.h src/services/parts/PartReferenceCustomizationService.cpp
        src/services/parts/ElementIdentityService.h src/services/parts/ElementIdentityService.cpp
        src/services/application/ApplicationServices.h
        src/services/application/ApplicationServices.cpp
        src/services/application/ApplicationServicesCore.cpp
        src/services/application/HostReadExecutor.h src/services/application/HostReadExecutor.cpp
        src/models/export/InventoryExportTypes.h
        src/services/inventory/InventoryExportService.h src/services/inventory/InventoryExportService.cpp
        src/models/export/CollectionExportTypes.h
        src/services/collection/CollectionExportService.h src/services/collection/CollectionExportService.cpp
        src/services/application/CollectionDisassemblyPlanService.h
        src/services/application/CollectionDisassemblyPlanService.cpp
        src/services/application/HostWriteExecutor.h src/services/application/HostWriteExecutor.cpp
        src/services/application/HostMaintenanceCoordinator.h src/services/application/HostMaintenanceCoordinator.cpp
        src/services/application/HostOperationalGate.h
        src/services/application/HostMutationPublicationService.h
        src/services/application/HostMutationPublicationService.cpp
        src/services/application/dto/RemoteMutationDtos.h
        src/services/application/dto/RemoteMutationDtos.cpp
        src/models/RemoteMutationReceipt.h
        src/repositories/RemoteMutationReceiptRepository.h
        src/repositories/RemoteMutationReceiptRepository.cpp
        src/services/application/HostReadProtocolService.h src/services/application/HostReadProtocolService.cpp
        src/services/application/RemoteReadApplicationServices.h src/services/application/RemoteReadApplicationServices.cpp
        src/network/RemoteSessionState.h src/network/RemoteSessionState.cpp
        src/services/application/AsyncReadResult.h
        src/services/application/dto/RemoteReadDtos.h
        src/services/application/dto/RemoteReadJson.h src/services/application/dto/RemoteReadJson.cpp
        src/network/BrickSuiteProtocol.h src/network/BrickSuiteProtocol.cpp
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
        src/network/BrickSuiteOperationDispatcher.h src/network/BrickSuiteOperationDispatcher.cpp
        src/network/BrickSuiteAuthentication.h src/network/BrickSuiteAuthentication.cpp
        src/network/BrickSuiteHostIdentity.h src/network/BrickSuiteHostIdentity.cpp
        src/network/PairedDeviceRegistry.h src/network/PairedDeviceRegistry.cpp
        src/network/BrickSuitePairingService.h src/network/BrickSuitePairingService.cpp
        src/network/BrickSuiteConnectionState.h
        src/network/BrickSuiteWebSocketServer.h src/network/BrickSuiteWebSocketServer.cpp
        src/network/HostRequestContext.h
        src/network/HostRequestAdmissionController.h src/network/HostRequestAdmissionController.cpp
        src/network/HostOutboundPolicy.h src/network/HostOutboundPolicy.cpp
        src/network/HostAuthenticationThrottle.h src/network/HostAuthenticationThrottle.cpp
        src/network/BrickSuiteWebSocketClient.h src/network/BrickSuiteWebSocketClient.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
        resources/resources.qrc
    )
    target_compile_definitions(HostReadExecutorTest PRIVATE
        BRICKSUITE_VERSION="${PROJECT_VERSION}"
        BRICKSUITE_TESTING)
    target_link_libraries(HostReadExecutorTest PRIVATE
        Qt6::Core Qt6::Sql Qt6::Network Qt6::WebSockets OpenSSL::Crypto)
    if(WIN32)
        target_link_libraries(HostReadExecutorTest PRIVATE Advapi32)
    elseif(APPLE)
        target_link_libraries(HostReadExecutorTest PRIVATE "-framework Security")
    endif()
    add_test(NAME HostReadExecutor COMMAND HostReadExecutorTest)

    qt_add_executable(RemoteReadFoundationTest EXCLUDE_FROM_ALL
        tests/RemoteReadFoundationTest.cpp
        src/services/application/AsyncReadResult.h
        src/services/application/dto/RemoteReadDtos.h
        src/services/application/dto/RemoteReadJson.h
        src/services/application/dto/RemoteReadJson.cpp
        src/services/application/dto/RemoteBuildabilityDtos.h
        src/services/application/dto/RemoteBuildabilityDtos.cpp
        src/network/BrickSuiteProtocol.h src/network/BrickSuiteProtocol.cpp
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
        src/network/BrickSuiteOperationDispatcher.h src/network/BrickSuiteOperationDispatcher.cpp
    )
    target_compile_definitions(RemoteReadFoundationTest PRIVATE
        BRICKSUITE_VERSION="${PROJECT_VERSION}")
    target_link_libraries(RemoteReadFoundationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME RemoteReadFoundation COMMAND RemoteReadFoundationTest)

    qt_add_executable(RemoteSessionStateTest EXCLUDE_FROM_ALL
        tests/RemoteSessionStateTest.cpp
        src/network/RemoteSessionState.h
        src/network/RemoteSessionState.cpp
        src/app/WorkspaceContext.h
        src/app/WorkspaceContext.cpp
    )
    target_link_libraries(RemoteSessionStateTest PRIVATE Qt6::Core)
    add_test(NAME RemoteSessionState COMMAND RemoteSessionStateTest)

    qt_add_executable(RemoteRefreshCoordinatorTest EXCLUDE_FROM_ALL
        tests/RemoteRefreshCoordinatorTest.cpp
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
        src/services/application/RemoteRefreshCoordinator.h src/services/application/RemoteRefreshCoordinator.cpp
    )
    target_link_libraries(RemoteRefreshCoordinatorTest PRIVATE Qt6::Core)
    add_test(NAME RemoteRefreshCoordinator COMMAND RemoteRefreshCoordinatorTest)

    qt_add_executable(HostMutationPublicationTest EXCLUDE_FROM_ALL
        tests/HostMutationPublicationTest.cpp
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
        src/services/application/HostMutationPublicationService.h
        src/services/application/HostMutationPublicationService.cpp
    )
    target_link_libraries(HostMutationPublicationTest PRIVATE Qt6::Core)
    add_test(NAME HostMutationPublication COMMAND HostMutationPublicationTest)

    qt_add_executable(RemoteMutationFoundationTest EXCLUDE_FROM_ALL
        tests/RemoteMutationFoundationTest.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/RemoteMutationReceipt.h
        src/repositories/RepositoryConnection.h
        src/repositories/RemoteMutationReceiptRepository.h
        src/repositories/RemoteMutationReceiptRepository.cpp
        src/services/application/dto/RemoteMutationDtos.h
        src/services/application/dto/RemoteMutationDtos.cpp
        src/services/application/HostWriteExecutor.h
        src/services/application/HostWriteExecutor.cpp
        src/services/application/HostMutationProtocolService.h
        src/services/application/HostMutationProtocolService.cpp
        src/services/application/HostMutationPublicationService.h
        src/services/application/HostMutationPublicationService.cpp
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
        src/network/BrickSuiteProtocol.h src/network/BrickSuiteProtocol.cpp
        src/network/BrickSuiteOperationDispatcher.h src/network/BrickSuiteOperationDispatcher.cpp
    )
    target_compile_definitions(RemoteMutationFoundationTest PRIVATE
        BRICKSUITE_VERSION="${PROJECT_VERSION}")
    target_link_libraries(RemoteMutationFoundationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME RemoteMutationFoundation COMMAND RemoteMutationFoundationTest)

    qt_add_executable(RemotePullingMutationTest EXCLUDE_FROM_ALL
        tests/RemotePullingMutationTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Build.h src/models/Build.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/BuildRequirement.h src/models/BuildRequirement.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/Part.h src/models/Part.cpp
        src/models/Color.h src/models/Color.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/Manufacturer.h
        src/repositories/RepositoryConnection.h
        src/repositories/BuildRepository.h src/repositories/BuildRepository.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/BuildRequirementRepository.h src/repositories/BuildRequirementRepository.cpp
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/LostInventoryRepository.h src/repositories/LostInventoryRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/RemoteMutationReceiptRepository.h src/repositories/RemoteMutationReceiptRepository.cpp
        src/services/builds/BuildPullingService.h src/services/builds/BuildPullingService.cpp
        src/services/application/HostWriteExecutor.h src/services/application/HostWriteExecutor.cpp
        src/services/application/HostPullingMutationService.h src/services/application/HostPullingMutationService.cpp
        src/services/application/HostMutationPublicationService.h
        src/services/application/dto/RemoteMutationDtos.h src/services/application/dto/RemoteMutationDtos.cpp
        src/services/application/dto/RemotePullingMutationDtos.h src/services/application/dto/RemotePullingMutationDtos.cpp
    )
    target_link_libraries(RemotePullingMutationTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME RemotePullingMutation COMMAND RemotePullingMutationTest)

    qt_add_executable(RemoteInventoryMutationTest EXCLUDE_FROM_ALL
        tests/RemoteInventoryMutationTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/InventoryRecord.h src/models/InventoryRecord.cpp
        src/models/InventoryMovement.h src/models/InventoryMovement.cpp
        src/models/BuildAllocation.h src/models/BuildAllocation.cpp
        src/models/Workspace.h src/models/Workspace.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/models/StorageLocationType.h src/models/StorageLocationType.cpp
        src/models/Part.h src/models/Part.cpp src/models/Manufacturer.h
        src/repositories/RepositoryConnection.h
        src/repositories/InventoryRecordRepository.h src/repositories/InventoryRecordRepository.cpp
        src/repositories/InventoryMovementRepository.h src/repositories/InventoryMovementRepository.cpp
        src/repositories/BuildAllocationRepository.h src/repositories/BuildAllocationRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/repositories/WorkspaceRepository.h src/repositories/WorkspaceRepository.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/repositories/StorageLocationTypeRepository.h src/repositories/StorageLocationTypeRepository.cpp
        src/services/application/HostInventoryMutationService.h src/services/application/HostInventoryMutationService.cpp
        src/services/application/HostStorageMutationService.h src/services/application/HostStorageMutationService.cpp
        src/services/application/HostStorageProtocolMutationService.h src/services/application/HostStorageProtocolMutationService.cpp
        src/services/application/HostWriteExecutor.h src/services/application/HostWriteExecutor.cpp
        src/services/application/HostMutationPublicationService.h src/services/application/HostMutationPublicationService.cpp
        src/services/application/dto/RemoteMutationDtos.h src/services/application/dto/RemoteMutationDtos.cpp
        src/services/application/dto/RemoteInventoryMutationDtos.h src/services/application/dto/RemoteInventoryMutationDtos.cpp
        src/services/application/dto/RemoteReadDtos.h src/services/application/dto/RemoteReadJson.h src/services/application/dto/RemoteReadJson.cpp
        src/services/application/dto/RemoteStorageMutationDtos.h src/services/application/dto/RemoteStorageMutationDtos.cpp
        src/repositories/RemoteMutationReceiptRepository.h src/repositories/RemoteMutationReceiptRepository.cpp
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
        src/network/BrickSuiteProtocol.h src/network/BrickSuiteProtocol.cpp
        src/network/BrickSuiteAuthentication.h src/network/BrickSuiteAuthentication.cpp
        src/network/BrickSuiteHostIdentity.h src/network/BrickSuiteHostIdentity.cpp
        src/network/PairedDeviceRegistry.h src/network/PairedDeviceRegistry.cpp
        src/network/BrickSuitePairingService.h src/network/BrickSuitePairingService.cpp
        src/network/BrickSuiteConnectionState.h
        src/network/BrickSuiteOperationDispatcher.h src/network/BrickSuiteOperationDispatcher.cpp
        src/network/BrickSuiteWebSocketServer.h src/network/BrickSuiteWebSocketServer.cpp
        src/network/HostRequestContext.h
        src/network/HostRequestAdmissionController.h src/network/HostRequestAdmissionController.cpp
        src/network/HostOutboundPolicy.h src/network/HostOutboundPolicy.cpp
        src/network/HostAuthenticationThrottle.h src/network/HostAuthenticationThrottle.cpp
        src/network/BrickSuiteWebSocketClient.h src/network/BrickSuiteWebSocketClient.cpp
        src/services/application/HostMutationProtocolService.h src/services/application/HostMutationProtocolService.cpp
        src/services/application/RemoteMutationApplicationServices.h src/services/application/RemoteMutationApplicationServices.cpp
        src/services/application/RemoteInventoryMutationApplicationService.h src/services/application/RemoteInventoryMutationApplicationService.cpp
        src/services/application/RemoteStorageMutationApplicationService.h src/services/application/RemoteStorageMutationApplicationService.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
    )
    target_compile_definitions(RemoteInventoryMutationTest PRIVATE BRICKSUITE_VERSION="${PROJECT_VERSION}" BRICKSUITE_TESTING)
    target_link_libraries(RemoteInventoryMutationTest PRIVATE Qt6::Core Qt6::Sql Qt6::Network Qt6::WebSockets OpenSSL::Crypto)
    if(WIN32)
        target_link_libraries(RemoteInventoryMutationTest PRIVATE Advapi32)
    elseif(APPLE)
        target_link_libraries(RemoteInventoryMutationTest PRIVATE "-framework Security")
    endif()
    add_test(NAME RemoteInventoryMutation COMMAND RemoteInventoryMutationTest)

    qt_add_executable(RemoteAddInventoryDialogButtonTest EXCLUDE_FROM_ALL
        tests/RemoteAddInventoryDialogButtonTest.cpp
        src/ui/inventory/AddInventoryDialogButtonState.h
    )
    target_link_libraries(RemoteAddInventoryDialogButtonTest PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets)
    add_test(NAME RemoteAddInventoryDialogButton COMMAND RemoteAddInventoryDialogButtonTest)
    set_tests_properties(RemoteAddInventoryDialogButton PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(RemoteInventoryDialogParityTest EXCLUDE_FROM_ALL
        tests/RemoteInventoryDialogParityTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/models/Color.h src/models/Color.cpp src/models/Manufacturer.h src/models/Part.h src/models/Part.cpp
        src/repositories/ColorRepository.h src/repositories/ColorRepository.cpp
        src/repositories/PartRepository.h src/repositories/PartRepository.cpp
        src/repositories/ManufacturerRepository.h src/repositories/ManufacturerRepository.cpp
        src/ui/helpers/ColorComboHelper.h src/ui/helpers/ColorComboHelper.cpp
        src/ui/inventory/RemoteInventoryMutationDialog.h src/ui/inventory/RemoteInventoryMutationDialog.cpp
        src/services/storage/SessionStorageSelectionService.h src/services/storage/SessionStorageSelectionService.cpp
        src/repositories/StorageLocationRepository.h src/repositories/StorageLocationRepository.cpp
        src/models/StorageLocation.h src/models/StorageLocation.cpp
        src/services/application/dto/RemoteMutationDtos.h src/services/application/dto/RemoteMutationDtos.cpp
        src/services/application/dto/RemoteInventoryMutationDtos.h src/services/application/dto/RemoteInventoryMutationDtos.cpp
        src/services/application/RemoteMutationApplicationServices.h src/services/application/RemoteMutationApplicationServices.cpp
        src/services/application/RemoteInventoryMutationApplicationService.h src/services/application/RemoteInventoryMutationApplicationService.cpp
        src/network/BrickSuiteProtocol.h src/network/BrickSuiteProtocol.cpp
        src/network/BrickSuiteAuthentication.h src/network/BrickSuiteAuthentication.cpp
        src/network/BrickSuiteHostIdentity.h src/network/BrickSuiteHostIdentity.cpp
        src/network/BrickSuiteConnectionState.h
        src/network/OperationalInvalidation.h src/network/OperationalInvalidation.cpp
        src/network/BrickSuiteWebSocketClient.h src/network/BrickSuiteWebSocketClient.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp
    )
    target_compile_definitions(RemoteInventoryDialogParityTest PRIVATE BRICKSUITE_VERSION="${PROJECT_VERSION}")
    target_link_libraries(RemoteInventoryDialogParityTest PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Qt6::WebSockets OpenSSL::Crypto)
    if(WIN32)
        target_link_libraries(RemoteInventoryDialogParityTest PRIVATE Advapi32)
    elseif(APPLE)
        target_link_libraries(RemoteInventoryDialogParityTest PRIVATE "-framework Security")
    endif()
    add_test(NAME RemoteInventoryDialogParity COMMAND RemoteInventoryDialogParityTest)
    set_tests_properties(RemoteInventoryDialogParity PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(LoggerLevelTest EXCLUDE_FROM_ALL
        tests/LoggerLevelTest.cpp
        src/services/Logger.h src/services/Logger.cpp)
    target_link_libraries(LoggerLevelTest PRIVATE Qt6::Core)
    add_test(NAME LoggerLevel COMMAND LoggerLevelTest)

    qt_add_executable(DatabaseRestoreHardeningTest EXCLUDE_FROM_ALL
        tests/DatabaseRestoreHardeningTest.cpp
        src/database/DatabaseManager.h src/database/DatabaseManager.cpp
        src/database/DatabaseSchema.h src/database/DatabaseSchema.cpp
        src/network/HostDataEpoch.h src/network/HostDataEpoch.cpp
        src/network/PairedDeviceRegistry.h src/network/PairedDeviceRegistry.cpp
        src/network/RemoteSessionState.h src/network/RemoteSessionState.cpp
        src/services/database/DatabaseFileValidator.h
        src/services/database/DatabaseFileValidator.cpp
        src/services/database/DatabaseRestoreTransaction.h
        src/services/database/DatabaseRestoreTransaction.cpp
        src/services/database/AutomaticBackupPolicy.h
        src/services/database/AutomaticBackupPolicy.cpp)
    target_link_libraries(DatabaseRestoreHardeningTest PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME DatabaseRestoreHardening COMMAND DatabaseRestoreHardeningTest)

    qt_add_executable(HostAuthenticationThrottleTest EXCLUDE_FROM_ALL
        tests/HostAuthenticationThrottleTest.cpp
        src/network/HostAuthenticationThrottle.h
        src/network/HostAuthenticationThrottle.cpp)
    target_link_libraries(HostAuthenticationThrottleTest PRIVATE Qt6::Core)
    add_test(NAME HostAuthenticationThrottle COMMAND HostAuthenticationThrottleTest)

    qt_add_executable(PairingFoundationTest EXCLUDE_FROM_ALL
        tests/PairingFoundationTest.cpp
        src/network/BrickSuiteAuthentication.h src/network/BrickSuiteAuthentication.cpp
        src/network/PairedDeviceRegistry.h src/network/PairedDeviceRegistry.cpp
        src/network/BrickSuitePairingService.h src/network/BrickSuitePairingService.cpp
        src/services/CredentialStore.h src/services/CredentialStore.cpp)
    target_link_libraries(PairingFoundationTest PRIVATE Qt6::Core OpenSSL::Crypto)
    if(WIN32)
        target_link_libraries(PairingFoundationTest PRIVATE Advapi32)
    elseif(APPLE)
        target_link_libraries(PairingFoundationTest PRIVATE "-framework Security")
    endif()
    add_test(NAME PairingFoundation COMMAND PairingFoundationTest)

    qt_add_executable(RebrickableCoordinationPolicyTest EXCLUDE_FROM_ALL
        tests/RebrickableCoordinationPolicyTest.cpp
        src/services/application/RebrickableCoordinationService.h
        src/api/ApiProviderStatusRegistry.h src/api/ApiProviderStatusRegistry.cpp)
    target_link_libraries(RebrickableCoordinationPolicyTest PRIVATE Qt6::Core)
    add_test(NAME RebrickableCoordinationPolicy COMMAND RebrickableCoordinationPolicyTest)

    qt_add_executable(BricksetUsagePolicyTest EXCLUDE_FROM_ALL
        tests/BricksetUsagePolicyTest.cpp
        src/api/brickset/BricksetUsagePolicy.h src/api/brickset/BricksetUsagePolicy.cpp
        src/api/brickset/BricksetService.h src/api/brickset/BricksetService.cpp
        src/api/ApiNetworkService.h src/api/ApiNetworkService.cpp)
    target_compile_definitions(BricksetUsagePolicyTest PRIVATE
        BRICKSUITE_VERSION="${PROJECT_VERSION}" BRICKSUITE_TESTING=1)
    target_link_libraries(BricksetUsagePolicyTest PRIVATE Qt6::Core Qt6::Network)
    add_test(NAME BricksetUsagePolicy COMMAND BricksetUsagePolicyTest)

    qt_add_executable(WhatCanIBuildLaunchSelectionTest EXCLUDE_FROM_ALL
        tests/WhatCanIBuildLaunchSelectionTest.cpp
        src/models/WhatCanIBuildPartSelection.h)
    target_link_libraries(WhatCanIBuildLaunchSelectionTest PRIVATE Qt6::Core)
    add_test(NAME WhatCanIBuildLaunchSelection COMMAND WhatCanIBuildLaunchSelectionTest)

    function(bricksuite_configure_test_runtime test_name)
        if(WIN32)
            get_filename_component(_bricksuite_mingw_runtime_dir
                "${CMAKE_CXX_COMPILER}" DIRECTORY)
            if(OPENSSL_ROOT_DIR)
                set(_bricksuite_openssl_runtime_dir "${OPENSSL_ROOT_DIR}/bin")
            else()
                get_filename_component(_bricksuite_openssl_library_dir
                    "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
                get_filename_component(_bricksuite_openssl_root
                    "${_bricksuite_openssl_library_dir}" DIRECTORY)
                set(_bricksuite_openssl_runtime_dir "${_bricksuite_openssl_root}/bin")
            endif()
            set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${_bricksuite_mingw_runtime_dir}"
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
                "PATH=path_list_prepend:${_bricksuite_openssl_runtime_dir}"
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:mcut>"
                "QT_PLUGIN_PATH=set:$<TARGET_FILE_DIR:Qt6::Core>/../plugins"
                "QT_QPA_PLATFORM_PLUGIN_PATH=set:$<TARGET_FILE_DIR:Qt6::Core>/../plugins/platforms")
        endif()
    endfunction()

    get_property(_bricksuite_configured_tests DIRECTORY PROPERTY TESTS)
    foreach(_bricksuite_test IN LISTS _bricksuite_configured_tests)
        bricksuite_configure_test_runtime("${_bricksuite_test}")
    endforeach()
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
