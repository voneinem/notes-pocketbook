# notes_core: Qt-free core library (API client, sync engine, SQLite store, markdown).
# It MUST NOT depend on Qt or InkView (constitution Principle V); it builds on the host
# without the SDK and, for the device, with the SDK toolchain.

# Vendored SQLite amalgamation (pinned, see third_party/sqlite/VERSION).
add_library(pbnotes_sqlite STATIC ${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
target_include_directories(pbnotes_sqlite PUBLIC ${CMAKE_SOURCE_DIR}/third_party/sqlite)
target_compile_definitions(pbnotes_sqlite PRIVATE
    SQLITE_THREADSAFE=1 SQLITE_DQS=0 SQLITE_OMIT_LOAD_EXTENSION SQLITE_OMIT_DEPRECATED)
target_compile_options(pbnotes_sqlite PRIVATE -w)
target_link_libraries(pbnotes_sqlite PUBLIC pthread m)

set(PBNOTES_CORE_SOURCES
    src/version.cpp
    src/about.cpp
    src/types.cpp
    src/http_transport.cpp
    src/title.cpp
    src/text_clean.cpp
    src/edit_history.cpp
    src/slash.cpp
    src/utf8.cpp
    src/ca_selector.cpp
    src/log.cpp
    src/note_store.cpp
    src/note_store_attachments.cpp
    src/json.cpp
    src/api_client.cpp
    src/attachment_service.cpp
    src/connection_test.cpp
    src/curl_transport.cpp
    src/links.cpp
    src/attachment_ref.cpp
    src/markdown.cpp
    src/markdown_inline.cpp
    src/markdown_table.cpp
    src/markdown_copy.cpp
    src/paging.cpp
    src/sync_engine.cpp
    src/sync_attachments.cpp
    src/sync_status.cpp
    src/sync_scheduler.cpp
)

# libcurl: the system package on the host, the SDK sysroot library for the device.
find_package(CURL REQUIRED)

add_library(notes_core STATIC ${PBNOTES_CORE_SOURCES})
target_include_directories(notes_core PUBLIC include)
target_compile_features(notes_core PUBLIC cxx_std_17)
target_compile_definitions(notes_core PRIVATE PBNOTES_VERSION="${PBNOTES_VERSION}")
target_link_libraries(notes_core PUBLIC pbnotes_sqlite CURL::libcurl)
