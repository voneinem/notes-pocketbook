# Qt layer between the QML UI and notes_core. ARM: SDK Qt with manual moc. HOST: host Qt6 (Core only)
# when installed; otherwise the bridge, the app and the bridge tests are simply not built.

set(PBNOTES_BRIDGE_SOURCES src/notes_bridge.cpp src/reader_blocks.cpp)

set(PBNOTES_BRIDGE_SOURCES ${PBNOTES_BRIDGE_SOURCES} src/attachment_image_provider.cpp src/attachments_bridge.cpp src/file_preview_provider.cpp
    src/about_bridge.cpp src/edit_bridge.cpp)

if(TARGET_TYPE STREQUAL "ARM")
    pbnotes_moc(PBNOTES_BRIDGE_MOC ${CMAKE_CURRENT_SOURCE_DIR}/include/notes_bridge.h
                                   ${CMAKE_CURRENT_SOURCE_DIR}/include/attachment_image_provider.h
                                   ${CMAKE_CURRENT_SOURCE_DIR}/include/attachments_bridge.h
                                   ${CMAKE_CURRENT_SOURCE_DIR}/include/file_preview_provider.h
                                   ${CMAKE_CURRENT_SOURCE_DIR}/include/about_bridge.h
                                   ${CMAKE_CURRENT_SOURCE_DIR}/include/edit_bridge.h)
    add_library(pbnotes_bridge STATIC ${PBNOTES_BRIDGE_SOURCES} ${PBNOTES_BRIDGE_MOC})
    target_include_directories(pbnotes_bridge PUBLIC include)
    target_include_directories(pbnotes_bridge SYSTEM PUBLIC ${PBNOTES_QT_INCLUDES})
    target_compile_definitions(pbnotes_bridge PUBLIC PLATFORM_POCKETBOOK)
    target_link_libraries(pbnotes_bridge PUBLIC notes_core ${PBNOTES_QT_LIBS})
else()
    find_package(Qt6 QUIET COMPONENTS Core Gui Quick)
    if(Qt6Core_FOUND AND Qt6Gui_FOUND AND Qt6Quick_FOUND)
        set(CMAKE_AUTOMOC ON)
        add_library(pbnotes_bridge STATIC ${PBNOTES_BRIDGE_SOURCES} include/notes_bridge.h include/attachment_image_provider.h
                include/attachments_bridge.h include/file_preview_provider.h include/about_bridge.h include/edit_bridge.h)
        target_include_directories(pbnotes_bridge PUBLIC include)
        target_link_libraries(pbnotes_bridge PUBLIC notes_core Qt6::Core Qt6::Gui Qt6::Quick)
    else()
        message(STATUS "Qt6 Core/Gui/Quick not found - the Qt bridge, the app and the bridge tests are not built")
    endif()
endif()
