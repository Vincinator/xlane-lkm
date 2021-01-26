cmake_minimum_required(VERSION 3.13)
project(libasraft C)

# -------------------------------------------

set(ASGARD_DPDK 0)
set(ASGARD_KERNEL_MODULE 0)
message("version: 2")


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


list(APPEND library_sources core/libasraft.c core/libasraft.h core/types.h core/types.c core/logger.c core/logger.h core/consensus.c
        core/consensus.h core/kvstore.c core/kvstore.h core/pacemaker.c core/pacemaker.h core/payload.c core/payload.h
        core/ringbuffer.c core/ringbuffer.h core/membership.c core/membership.h core/leader.c core/leader.h core/follower.c
        core/follower.h core/candidate.c core/candidate.h core/replication.c core/replication.h core/pktqueue.c core/pktqueue.h
        core/pkthandler.c core/pkthandler.h core/proto.c core/proto.h core/multicast.c core/multicast.h
        core/eval_helper.c core/eval_helper.h core/echo.c core/echo.h)

if(NOT ${ASGARD_KERNEL_MODULE})
    list(PREPEND library_sources userspace/tnode.c userspace/tnode.h userspace/ini.c)
endif()




add_library(libasraft STATIC ${library_sources})


if(${ASGARD_KERNEL_MODULE})
    get_target_property(module_sources libasraft SOURCES)
    file(GLOB lkmfiles "lkm/*")
    foreach(src ${lkmfiles})
        get_filename_component(SRC_NAME ${src} NAME)
        list(PREPEND module_sources lkm/${SRC_NAME})
        # message("Adding source ${SRC_NAME}")
    endforeach()

    list(PREPEND module_sources core/module.h)
    list(PREPEND module_sources core/module.c)

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

    add_executable(testrunner userspace/runner.c)
    target_link_libraries(testrunner LINK_PUBLIC libasraft)

endif()


if(${ASGARD_DPDK})

    if (NOT ${DPDK_FOUND})
        message( SEND_ERROR "DPDK installation not found on system" )
    endif ()
    add_compile_definitions(ASGARD_DPDK=${ASGARD_DPDK})
    target_link_libraries(libasraft PRIVATE ${DPDK_LIBRARIES} )
endif ()


target_link_libraries(libasraft PRIVATE Threads::Threads)

