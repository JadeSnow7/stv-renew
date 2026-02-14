# ---- app module: stv_app (Qt 6 required) ----
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup()

qt_add_executable(stv_app
    src/main.cpp
    src/presenter.cpp
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

# Process QML resources
qt_add_resources(stv_app "qml_resources"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        qml/main.qml
        qml/CreatePage.qml
        qml/ProgressPage.qml
)

target_compile_features(stv_app PRIVATE cxx_std_17)
