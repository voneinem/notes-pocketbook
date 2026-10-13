# Host-only tests. Every tests/{unit,contract,integration}/test_*.cpp becomes its own
# executable (linked with notes_core) registered with CTest and labelled by directory.
# Tests are written first and must fail before the implementation exists (Principle I).

file(GLOB PBNOTES_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contract/test_*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/test_*.cpp)

# The TLS tests run local HTTPS servers with libssl and need the `openssl` CLI at run time.
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

foreach(src ${PBNOTES_TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    get_filename_component(dir ${src} DIRECTORY)
    get_filename_component(kind ${dir} NAME)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE notes_core OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
    target_compile_definitions(${name} PRIVATE
        PBNOTES_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        PBNOTES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${kind}" SKIP_RETURN_CODE 77)
endforeach()

# The Qt bridge tests need Qt Core; test_notes_bridge runs real local HTTPS servers like the core TLS tests.
if(TARGET pbnotes_bridge)
    foreach(name test_notes_bridge test_reader_blocks test_attachment_image_provider test_attachments_bridge test_file_preview_provider test_about_bridge test_edit_bridge)
        add_executable(${name} bridge/${name}.cpp)
        target_link_libraries(${name} PRIVATE pbnotes_bridge OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
        target_compile_definitions(${name} PRIVATE PBNOTES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES LABELS "bridge" SKIP_RETURN_CODE 77)
    endforeach()
endif()

# The real QML pages driven offscreen (spec 005): a driver script per scenario, the page under test and stubs for the
# bridge objects. Needs Qt Quick on the host; skipped otherwise (and on the device build).
if(TARGET pbnotes_bridge AND NOT TARGET_TYPE STREQUAL "ARM")
    find_package(Qt6 QUIET COMPONENTS Qml Quick Test)
    if(Qt6Qml_FOUND AND Qt6Quick_FOUND AND Qt6Test_FOUND)
        set(CMAKE_AUTOMOC ON)
        add_executable(test_qml_pages qml/test_qml_pages.cpp)
        target_link_libraries(test_qml_pages PRIVATE pbnotes_bridge Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
        target_compile_definitions(test_qml_pages PRIVATE
            PBNOTES_QML_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml" PBNOTES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME qml_editor_flow COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/editor_flow.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/EditorPage.qml)
        set_tests_properties(qml_editor_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_editor_slash COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/editor_slash.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/EditorPage.qml)
        set_tests_properties(qml_editor_slash PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_reader_title COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/reader_title.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/ReaderPage.qml)
        set_tests_properties(qml_reader_title PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_reader_select COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/reader_select.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/ReaderPage.qml)
        set_tests_properties(qml_reader_select PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_reader_blocks COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/reader_blocks.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/ReaderPage.qml)
        set_tests_properties(qml_reader_blocks PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_settings_sync COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/settings_sync.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/SettingsPage.qml)
        set_tests_properties(qml_settings_sync PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_ui_style COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/ui_style.qml
                 ${CMAKE_SOURCE_DIR}/qml/Icon.qml)
        set_tests_properties(qml_ui_style PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_note_list COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/note_list.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/NoteListPage.qml)
        set_tests_properties(qml_note_list PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_ui_dialogs COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/ui_dialogs.qml
                 ${CMAKE_SOURCE_DIR}/qml/ConfirmDialog.qml)
        set_tests_properties(qml_ui_dialogs PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_editor_bars COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/editor_bars.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/EditorPage.qml)
        set_tests_properties(qml_editor_bars PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_reader_page COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/reader_page.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/ReaderPage.qml)
        set_tests_properties(qml_reader_page PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_setup_page COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/setup_page.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/SetupPage.qml)
        set_tests_properties(qml_setup_page PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_about_pages COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/about_pages.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/AboutPage.qml)
        set_tests_properties(qml_about_pages PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
        add_test(NAME qml_file_picker COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/file_picker.qml
                 ${CMAKE_SOURCE_DIR}/qml/pages/FilePickerPage.qml)
        set_tests_properties(qml_file_picker PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge")
    endif()
endif()

# Mechanical anonymization check of the fixtures, and a self-test proving the checker works.
add_test(NAME check_fixtures
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-fixtures.sh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
add_test(NAME check_fixtures_selftest
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-fixtures.sh --self-test)
add_test(NAME check_constitution COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-constitution.sh ${CMAKE_SOURCE_DIR})
add_test(NAME check_constitution_selftest COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-constitution.sh --self-test)
# Spec 010: no bordered text button anywhere in the QML (FR-006, SC-003), and a self-test proving the checker works.
add_test(NAME check_ui_style COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-ui-style.sh ${CMAKE_SOURCE_DIR})
add_test(NAME check_ui_style_selftest COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-ui-style.sh --self-test)
add_test(NAME check_version_bump_selftest COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-version-bump.sh --self-test)
set_tests_properties(check_fixtures check_fixtures_selftest check_constitution check_constitution_selftest
                     PROPERTIES LABELS "script")
add_test(NAME launcher_script COMMAND ${CMAKE_SOURCE_DIR}/tests/launcher/test_launcher.sh ${CMAKE_SOURCE_DIR})
