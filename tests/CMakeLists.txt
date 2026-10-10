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

# The Qt bridge test needs Qt Core; it runs real local HTTPS servers like the core TLS tests.
if(TARGET pbnotes_bridge)
    add_executable(test_notes_bridge bridge/test_notes_bridge.cpp)
    target_link_libraries(test_notes_bridge PRIVATE pbnotes_bridge OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
    target_include_directories(test_notes_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
    target_compile_definitions(test_notes_bridge PRIVATE PBNOTES_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME test_notes_bridge COMMAND test_notes_bridge)
    set_tests_properties(test_notes_bridge PROPERTIES LABELS "bridge" SKIP_RETURN_CODE 77)
endif()

# Mechanical anonymization check of the fixtures, and a self-test proving the checker works.
add_test(NAME check_fixtures
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-fixtures.sh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
add_test(NAME check_fixtures_selftest
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-fixtures.sh --self-test)
add_test(NAME check_constitution COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-constitution.sh ${CMAKE_SOURCE_DIR})
add_test(NAME check_constitution_selftest COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-constitution.sh --self-test)
set_tests_properties(check_fixtures check_fixtures_selftest check_constitution check_constitution_selftest
                     PROPERTIES LABELS "script")
