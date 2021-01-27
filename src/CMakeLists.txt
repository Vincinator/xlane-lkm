cmake_minimum_required(VERSION 3.13)
project(asgard C)

# -------------------------------------------

set(ASGARD_DPDK 1)
set(ASGARD_KERNEL_MODULE 0)


set(kerneldir "/home/vincent/workspace/asgard-kernel")
set(bindir "${CMAKE_SOURCE_DIR}/../bin")

# -------------------------------------------
if(${ASGARD_DPDK})
    set(testrunner_target testrunner-dpdk)
else()
    set(testrunner_target testrunner)
endif()

message("version: v12")


if(${ASGARD_KERNEL_MODULE} AND ${ASGARD_DPDK})
    message(SEND_ERROR "Kernel Module and DPDK are mutual exclusive options. Select only one!" )
endif()

set(CMAKE_C_STANDARD 99)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_BUILD_TYPE Debug)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_library(LibUUID_LIBRARY  NAMES uuid)

include_directories(..)
include_directories(${LIBUUID_INCLUDE_DIR})

file(MAKE_DIRECTORY ${bindir})
file(MAKE_DIRECTORY ${bindir}/lib)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${bindir}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${bindir}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${bindir})


FUNCTION(PREPEND var prefix)
    SET(listVar "")
    FOREACH(f ${ARGN})
        LIST(APPEND listVar "${prefix}/${f}")
    ENDFOREACH(f)
    SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)


if(${ASGARD_DPDK})
    message("Selected Option: DPDK")

    find_package (DPDK REQUIRED)
    include_directories (${DPDK_INCLUDE_DIR})
    add_compile_options(-mssse3)

elseif(${ASGARD_KERNEL_MODULE})

    message("Selected Option: kernel module")
    message("kerneldir is set to ${kerneldir}")
    #set(kerneldir "" CACHE STRING "Path to the kernel build directory")
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
        core/eval_helper.c core/eval_helper.h core/echo.c core/echo.h core/config.h core/timekeeping.h core/timekeeping.c )

if(NOT ${ASGARD_KERNEL_MODULE})
    list(PREPEND library_sources userspace/tnode.c userspace/tnode.h userspace/ini.c)
endif()



add_library(asgard STATIC ${library_sources})


if(${ASGARD_KERNEL_MODULE})
    message("config kernel module")

    get_target_property(module_sources asgard SOURCES)
    file(GLOB lkmfiles "lkm/*")
    foreach(src ${lkmfiles})
        get_filename_component(SRC_NAME ${src} NAME)
        list(PREPEND module_sources lkm/${SRC_NAME})
        message("Adding source lkm/${SRC_NAME}")
    endforeach()

    list(PREPEND module_sources core/module.h)
    list(PREPEND module_sources core/module.c)

    foreach(src ${module_sources})
        list(APPEND module_sources_lkm ../src/${src})
        #message("configuring source path  ../src/${src}")
    endforeach()


    set(ASGARD_KERNEL_MODULE 1)

    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/Kbuild.in
            DESTINATION  ${bindir})

    string(REPLACE ";" " " module_sources_string "${module_sources_lkm}")
    configure_file(${bindir}/Kbuild.in ${bindir}/Kbuild @ONLY)

    foreach(src ${module_sources_lkm})
       # message("configuring ${src}")
        configure_file(${src} ${src} COPYONLY)
    endforeach()
    set(module_cmd $(MAKE) -C ${kerneldir} M=${bindir}  ccflags-y="-DASGARD_KERNEL_MODULE=1")
    message("using module cmd: ${module_cmd}")

    add_custom_command(OUTPUT asgard.ko
            COMMAND ${module_cmd} modules
            WORKING_DIRECTORY ${bindir}
            DEPENDS ${module_sources_lkm} ${bindir}
            VERBATIM)
    add_compile_definitions(ASGARD_KERNEL_MODULE)
    message("using  ${kerneldir}")

    add_custom_target(module DEPENDS asgard.ko )
    add_custom_command(TARGET module  PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory  ${bindir})
    add_custom_command(TARGET module  PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory  ${bindir}/lib)
    include_directories("${bindir}")

    # add_custom_target(module-clean COMMAND ${module_cmd} clean
    #        WORKING_DIRECTORY ${bindir})

else()
    target_link_libraries(asgard PRIVATE Threads::Threads)
    message("Adding ${testrunner_target} to targets")
    add_executable(${testrunner_target} userspace/runner.c)
    target_link_libraries(${testrunner_target} LINK_PUBLIC asgard)

endif()


if(${ASGARD_DPDK})

    if (NOT ${DPDK_FOUND})
        message( SEND_ERROR "DPDK installation not found on system" )
    endif ()

    add_compile_definitions(ASGARD_DPDK=${ASGARD_DPDK})
    target_link_libraries(asgard PRIVATE ${DPDK_LIBRARIES} )
endif ()


# Clean all .o and .cmd files
set(clean_cmd find .. -name "*.cmd" -type f -delete && find .. -name "*.o" -type f -delete)
add_custom_target(clean-all COMMAND ${clean_cmd})

