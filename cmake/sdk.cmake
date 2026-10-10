# Cross toolchain for the PocketBook SDK (TARGET_TYPE=ARM). Included BEFORE project().
#
# The SDK location is taken ONLY from -DSDK_PATH=<dir> or the environment variable
# POCKETBOOK_SDK. There is deliberately no default path. Pattern adapted from InkFeed
# (MIT, see NOTICE).

if(NOT SDK_PATH)
    if(DEFINED ENV{POCKETBOOK_SDK} AND NOT "$ENV{POCKETBOOK_SDK}" STREQUAL "")
        set(SDK_PATH "$ENV{POCKETBOOK_SDK}")
    endif()
endif()

if(NOT SDK_PATH)
    message(FATAL_ERROR
        "TARGET_TYPE=ARM needs the PocketBook SDK. Set the environment variable POCKETBOOK_SDK "
        "or pass -DSDK_PATH=<dir> (the arm-buildroot-linux-gnueabihf_sdk-buildroot directory).")
endif()

get_filename_component(SDK_PATH "${SDK_PATH}" ABSOLUTE)
if(NOT IS_DIRECTORY "${SDK_PATH}")
    message(FATAL_ERROR "SDK_PATH '${SDK_PATH}' is not a directory.")
endif()
set(SDK_PATH "${SDK_PATH}" CACHE PATH "PocketBook SDK root" FORCE)

set(TOOLCHAIN_PREFIX "arm-buildroot-linux-gnueabihf")
set(SYSROOT "${SDK_PATH}/${TOOLCHAIN_PREFIX}/sysroot")

foreach(tool gcc g++)
    if(NOT EXISTS "${SDK_PATH}/bin/${TOOLCHAIN_PREFIX}-${tool}")
        message(FATAL_ERROR "SDK_PATH '${SDK_PATH}' does not contain bin/${TOOLCHAIN_PREFIX}-${tool}; "
                            "is it the arm-buildroot-linux-gnueabihf_sdk-buildroot directory?")
    endif()
endforeach()
if(NOT IS_DIRECTORY "${SYSROOT}")
    message(FATAL_ERROR "Sysroot '${SYSROOT}' not found under SDK_PATH.")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_SYSROOT "${SYSROOT}")
set(CMAKE_C_COMPILER   "${SDK_PATH}/bin/${TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${SDK_PATH}/bin/${TOOLCHAIN_PREFIX}-g++")

set(CMAKE_FIND_ROOT_PATH "${SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# The binary is installed under a different prefix than the SDK's; the device finds its
# shared libraries through the launcher's LD_LIBRARY_PATH.
set(CMAKE_SKIP_RPATH TRUE)
