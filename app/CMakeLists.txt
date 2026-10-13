# The Qt Quick application. ARM: SDK Qt via absolute library paths, manual moc/rcc. HOST: host Qt6
# when the Qml/Quick dev packages are installed (otherwise only the core library and tests are built).

file(GLOB PBNOTES_QML_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/qml/*.qml ${CMAKE_SOURCE_DIR}/qml/pages/*.qml
    ${CMAKE_SOURCE_DIR}/qml/qmldir ${CMAKE_SOURCE_DIR}/qml/icons/*.svg)
set(PBNOTES_APP_SOURCES main.cpp device_bridge.cpp)

if(TARGET_TYPE STREQUAL "ARM")
    pbnotes_moc(PBNOTES_MOC_SOURCES device_bridge.h)
    pbnotes_rcc(PBNOTES_QML_RCC qml ${CMAKE_SOURCE_DIR}/qml/qml.qrc ${PBNOTES_QML_FILES})

    add_executable(pbnotes_app ${PBNOTES_APP_SOURCES} ${PBNOTES_MOC_SOURCES} ${PBNOTES_QML_RCC})
    target_include_directories(pbnotes_app SYSTEM PRIVATE ${PBNOTES_QT_INCLUDES})
    target_include_directories(pbnotes_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(pbnotes_app PRIVATE PLATFORM_POCKETBOOK PBNOTES_VERSION="${PBNOTES_VERSION}")
    # libinkview lives outside the default sysroot library directories.
    target_link_directories(pbnotes_app PRIVATE ${SYSROOT}/usr/local/lib)
    target_link_libraries(pbnotes_app PRIVATE pbnotes_bridge ${PBNOTES_QT_LIBS} pthread inkview)
    # Qt's own dependencies and libcurl's are found next to them at link time.
    target_link_options(pbnotes_app PRIVATE -Wl,-rpath-link,${QT6_ROOT}/lib -Wl,-rpath-link,${SYSROOT}/usr/lib)
    # The ELF is installed as <app>.bin and a shell launcher is installed as <app>.app.
    set_target_properties(pbnotes_app PROPERTIES SUFFIX ".app" OUTPUT_NAME "${PBNOTES_APP_NAME}")
else()
    find_package(Qt6 QUIET COMPONENTS Core Gui Qml Quick)
    if(Qt6Quick_FOUND AND Qt6Qml_FOUND AND TARGET pbnotes_bridge)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        add_executable(pbnotes_app ${PBNOTES_APP_SOURCES} ${CMAKE_SOURCE_DIR}/qml/qml.qrc)
        set_property(SOURCE ${CMAKE_SOURCE_DIR}/qml/qml.qrc PROPERTY OBJECT_DEPENDS ${PBNOTES_QML_FILES})
        target_include_directories(pbnotes_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(pbnotes_app PRIVATE
            PBNOTES_VERSION="${PBNOTES_VERSION}"
            PBNOTES_BUNDLED_CA="${CMAKE_SOURCE_DIR}/third_party/cacert/cacert.pem"
            PBNOTES_LICENSES_ROOT="${CMAKE_SOURCE_DIR}"
            PBNOTES_EMOJI_FONT="${CMAKE_SOURCE_DIR}/third_party/noto-emoji/NotoEmoji.ttf")
        target_link_libraries(pbnotes_app PRIVATE pbnotes_bridge Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
        set_target_properties(pbnotes_app PROPERTIES OUTPUT_NAME "${PBNOTES_APP_NAME}")
        # The UI must load without QML errors (offscreen platform, no display needed).
        add_test(NAME app_self_test COMMAND pbnotes_app --self-test)
        set_tests_properties(app_self_test PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    else()
        message(STATUS "Qt6 Qml/Quick dev packages not found - building the core library and tests only")
    endif()
endif()
