find_package(Qt6 6.5.3 EXACT REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup()

qt_add_executable(stv_app
    src/main.cpp
    src/presenter.cpp
    src/auth_presenter.cpp
    src/project_presenter.cpp
    src/storyboard_presenter.cpp
    src/job_presenter.cpp
    src/asset_presenter.cpp
    src/export_presenter.cpp
)

target_include_directories(stv_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(stv_app
    PRIVATE
        stv_core
        stv_infra
        Qt6::Quick
        Qt6::QuickControls2
)

qt_add_resources(stv_app "qml_resources"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        qml/main.qml
        qml/CreatePage.qml
        qml/ProgressPage.qml
        qml/LoginPage.qml
        qml/ProjectListPage.qml
        qml/ProjectEditorPage.qml
        qml/StoryboardPage.qml
        qml/AssetLibraryPage.qml
        qml/ExportPage.qml
)

target_compile_features(stv_app PRIVATE cxx_std_17)
set_project_warnings(stv_app)
