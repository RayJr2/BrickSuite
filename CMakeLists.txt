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
)
target_link_libraries(BrickSuite PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)

set_target_properties(BrickSuite PROPERTIES
    WIN32_EXECUTABLE TRUE
)