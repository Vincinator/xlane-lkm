cmake_minimum_required(VERSION 3.10)
project(synbuf C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)


# -----------------  Set Up Path to your Kernel Sources ------------------------
set(KERNEL_ROOT /Volumes/Workspace/asguard/asguard-kernel/)

# -----------------  Linux Kernel Module Stuff (Optional)-----------------------
add_definitions(-D__KERNEL__)
add_definitions(-DMODULE)
add_definitions(-std=c89)
add_definitions(-ARCH=x86)

add_definitions(-DDEVNAME="synbuf")

# -----------------  Header & Source Definitions) ------------------------------

set(KERNEL_HEADERS
        ${KERNEL_ROOT}/arch/x86/include
        ${KERNEL_ROOT}/arch/x86/include/generated/uapi
        ${KERNEL_ROOT}/arch/x86/include/generated
        ${KERNEL_ROOT}/include
        ${KERNEL_ROOT}/arch/x86/include/uapi
        ${KERNEL_ROOT}/include/uapi
        ${KERNEL_ROOT}/include/generated/uapi
        ${KERNEL_ROOT}/net/asguard_drv/common
        ${KERNEL_ROOT}/usr/include/uapi
        )

SET( MODULE_SOURCE_FILES
        kernel-space/synbuf-chardev.c
        kernel-space/include/synbuf-chardev.h)

include_directories(kernel-space/include)
include_directories(${KERNEL_HEADERS})


# -----------------  (Fictional Execectuable)  ---------------------------------

add_executable(synbuf ${MODULE_SOURCE_FILES})
