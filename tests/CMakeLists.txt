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
    foreach(name test_notes_bridge test_reader_blocks test_attachment_image_provider test_attachments_bridge test_about_bridge)
        add_executable(${name} bridge/${name}.cpp)
        target_link_libraries(${name} PRIVATE pbnotes_bridge OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
        target_compile_definitions(${name} PRIVATE PBNOTES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES LABELS "bridge" SKIP_RETURN_CODE 77)
    endforeach()
endif()

# Mechanical anonymization check of the fixtures, and a self-test proving the checker works.
add_test(NAME check_fixtures
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-fixtures.sh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
add_test(NAME check_fixtures_selftest
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-fixtures.sh --self-test)
add_test(NAME check_constitution COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-constitution.sh ${CMAKE_SOURCE_DIR})
add_test(NAME check_constitution_selftest COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-constitution.sh --self-test)
add_test(NAME check_version_bump_selftest COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-version-bump.sh --self-test)
set_tests_properties(check_fixtures check_fixtures_selftest check_constitution check_constitution_selftest
                     PROPERTIES LABELS "script")
