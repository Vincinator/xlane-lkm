cmake_minimum_required(VERSION 3.13)
project(libasraft C)

# -------------------------------------------

set(ASGARD_DPDK 1)
set(ASGARD_KERNEL_MODULE 0)
message("version: 1")


if(${ASGARD_KERNEL_MODULE} AND ${ASGARD_DPDK})
    message( SEND_ERROR "Kernel Module and DPDK are mutual exclusive options. Select only one!" )
endif()

# -------------------------------------------
set(CMAKE_C_STANDARD 99)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_BUILD_TYPE Debug)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_library(LibUUID_LIBRARY  NAMES uuid)

include_directories(.)
include_directories(${LIBUUID_INCLUDE_DIR})

if(${ASGARD_DPDK})
    message("Selected Option: DPDK")

    find_package (DPDK REQUIRED)
    include_directories (${DPDK_INCLUDE_DIR})
    add_compile_options(-mssse3)

elseif(${ASGARD_KERNEL_MODULE})

    message("Selected Option: kernel module")
    set(kerneldir "" CACHE STRING "Path to the kernel build directory")
    if("${kerneldir}" STREQUAL "")
        message("Searching for local kernel installation. CAUTION: you may want to use the asgard sources!")
        execute_process(COMMAND uname -r OUTPUT_VARIABLE uname_r
                OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(kerneldir "/lib/modules/${uname_r}/build")
    endif()
    find_file(kernel_makefile NAMES Makefile
            PATHS ${kerneldir} NO_DEFAULT_PATH)
    include_directories(${kerneldir}/include)

    if(NOT kernel_makefile)
        message(FATAL_ERROR "There is no Makefile in kerneldir!")
    endif()
endif()


add_library(libasraft STATIC libasraft.c libasraft.h types.h types.c logger.c logger.h consensus.c
        consensus.h kvstore.c kvstore.h pacemaker.c pacemaker.h payload.c payload.h
        ringbuffer.c ringbuffer.h membership.c membership.h leader.c leader.h follower.c
        follower.h candidate.c candidate.h replication.c replication.h pktqueue.c pktqueue.h
         pkthandler.c pkthandler.h proto.c proto.h multicast.c multicast.h
        eval_helper.c eval_helper.h  echo.c echo.h)


if(${ASGARD_KERNEL_MODULE})
    get_target_property(module_sources libasraft SOURCES)
    file(GLOB lkmfiles "lkm/*")
    foreach(src ${lkmfiles})
        get_filename_component(SRC_NAME ${src} NAME)
        list(PREPEND module_sources lkm/${SRC_NAME})
        # message("Adding source ${SRC_NAME}")
    endforeach()

    list(PREPEND module_sources module.h)
    list(PREPEND module_sources module.c)

    set(ASGARD_KERNEL_MODULE 1)


    string(REPLACE ";" " " module_sources_string "${module_sources}")
    configure_file(Kbuild.in Kbuild @ONLY)

    foreach(src ${module_sources})
        configure_file(${src} ${src} COPYONLY)
    endforeach()

    set(module_cmd ${CMAKE_MAKE_PROGRAM} -C ${kerneldir} M=${CMAKE_CURRENT_BINARY_DIR}  ccflags-y="-DASGARD_KERNEL_MODULE=1")
    add_custom_command(OUTPUT asgard.ko
            COMMAND ${module_cmd} modules
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS ${module_sources} ${CMAKE_CURRENT_BINARY_DIR}/Kbuild
            VERBATIM)
    add_compile_definitions(ASGARD_KERNEL_MODULE)
    message("using ${kerneldir}")
    add_custom_target(module DEPENDS asgard.ko )
    add_custom_target(module-clean COMMAND ${module_cmd} clean)

else()
    get_target_property(module_sources libasraft SOURCES)
    file(GLOB lkmfiles "userspace/*")
    foreach(src ${lkmfiles})
        get_filename_component(SRC_NAME ${src} NAME)
        list(PREPEND module_sources lkm/${SRC_NAME})
        # message("Adding source ${SRC_NAME}")
    endforeach()

    add_executable(testrunner runner.c)
endif()




if(${ASGARD_DPDK})

    if (NOT ${DPDK_FOUND})
        message( SEND_ERROR "DPDK installation not found on system" )
    endif ()
    add_compile_definitions(ASGARD_DPDK=${ASGARD_DPDK})
    target_link_libraries(libasraft PRIVATE ${DPDK_LIBRARIES} )
endif ()


target_link_libraries(libasraft PRIVATE Threads::Threads)

if(NOT ${ASGARD_KERNEL_MODULE})
    target_link_libraries(testrunner LINK_PUBLIC libasraft)
endif()