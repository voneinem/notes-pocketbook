# Manual moc/rcc helpers for the device build (no AUTOMOC/AUTORCC), adapted from InkFeed
# (MIT, see NOTICE). Included from app/CMakeLists.txt for TARGET_TYPE=ARM.
#
#   moc: the SDK's host moc (6.8.2) matches the sysroot Qt and runs on the host.
#   rcc: the SDK's own rcc needs an ICU the SDK does not ship, so the host Qt tools are used
#        (package qt6-base-dev-tools). Its output is compatible across Qt 6.x when written with
#        --no-zstd (the sysroot Qt does not export qt_resourceFeatureZstd).

set(QT6_ROOT "${SYSROOT}/usr/local/qt6")

find_program(HOST_MOC NAMES moc PATHS "${SDK_PATH}/local/qt6/libexec" NO_DEFAULT_PATH)
if(NOT HOST_MOC)
    message(FATAL_ERROR "SDK host moc not found in ${SDK_PATH}/local/qt6/libexec.")
endif()

find_program(HOST_RCC NAMES rcc
    PATHS /usr/lib/qt6/libexec /usr/lib/x86_64-linux-gnu/qt6/libexec /usr/lib/qt6/bin)
if(NOT HOST_RCC)
    message(FATAL_ERROR
        "Host Qt 'rcc' not found. Install the Qt host tools (Debian/Ubuntu: qt6-base-dev-tools); "
        "the SDK's own rcc cannot run on current hosts. You can also pass -DHOST_RCC=<path>.")
endif()
execute_process(COMMAND ${HOST_RCC} --version OUTPUT_VARIABLE PBNOTES_RCC_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
message(STATUS "Host moc: ${HOST_MOC}")
message(STATUS "Host rcc: ${HOST_RCC} (${PBNOTES_RCC_VERSION}); device Qt 6.8.x runtime")

set(PBNOTES_QT_INCLUDES
    ${QT6_ROOT}/include ${QT6_ROOT}/include/QtQml ${QT6_ROOT}/include/QtQuick
    ${QT6_ROOT}/include/QtGui ${QT6_ROOT}/include/QtCore ${QT6_ROOT}/include/QtNetwork
    ${QT6_ROOT}/include/QtConcurrent
    # private QPA headers (only used by the rotation experiment, to reach the platform screen)
    ${QT6_ROOT}/include/QtGui/6.8.2 ${QT6_ROOT}/include/QtGui/6.8.2/QtGui)

# Qt is linked by absolute path; QtQuick.Controls is resolved from the device's /usr/qml.
set(PBNOTES_QT_LIBS
    ${QT6_ROOT}/lib/libQt6Quick.so ${QT6_ROOT}/lib/libQt6Qml.so
    ${QT6_ROOT}/lib/libQt6Gui.so ${QT6_ROOT}/lib/libQt6Core.so)

# pbnotes_moc(<out-var> <header>...)  -> list of generated moc_*.cpp
function(pbnotes_moc out_var)
    set(outs)
    set(includes)
    foreach(inc ${PBNOTES_QT_INCLUDES} ${CMAKE_SOURCE_DIR}/app ${CMAKE_SOURCE_DIR}/bridge/include)
        list(APPEND includes -I${inc})
    endforeach()
    foreach(hdr ${ARGN})
        get_filename_component(abs_hdr ${hdr} ABSOLUTE)
        get_filename_component(bn ${hdr} NAME_WE)
        set(out ${CMAKE_CURRENT_BINARY_DIR}/moc_${bn}.cpp)
        add_custom_command(OUTPUT ${out}
            COMMAND ${HOST_MOC} -DPLATFORM_POCKETBOOK -o ${out} ${abs_hdr} ${includes}
            DEPENDS ${abs_hdr} VERBATIM)
        list(APPEND outs ${out})
    endforeach()
    set(${out_var} ${outs} PARENT_SCOPE)
endfunction()

# pbnotes_rcc(<out-var> <name> <qrc> [<dependency>...])  -> generated qrc_<name>.cpp
function(pbnotes_rcc out_var name qrc)
    get_filename_component(abs_qrc ${qrc} ABSOLUTE)
    set(out ${CMAKE_CURRENT_BINARY_DIR}/qrc_${name}.cpp)
    add_custom_command(OUTPUT ${out}
        COMMAND ${HOST_RCC} --no-zstd --name ${name} --output ${out} ${abs_qrc}
        DEPENDS ${abs_qrc} ${ARGN} VERBATIM)
    set(${out_var} ${out} PARENT_SCOPE)
endfunction()
