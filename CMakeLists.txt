cmake_minimum_required(VERSION 3.13)
project(libasraft2 C)

# -------------------------------------------

set(ASGARD_WITH_DPDK 1)

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

if(${ASGARD_WITH_DPDK})
    find_package (DPDK REQUIRED)
    include_directories (${DPDK_INCLUDE_DIR})
    add_compile_options(-mssse3)
endif()

add_library(libasraft libasraft.c libasraft.h logger.c logger.h consensus.c consensus.h kvstore.c kvstore.h pacemaker.c pacemaker.h payload.c payload.h ringbuffer.c ringbuffer.h membership.c membership.h leader.c leader.h follower.c follower.h candidate.c candidate.h replication.c replication.h pktqueue.c pktqueue.h list.h tnode.c tnode.h pkthandler.c pkthandler.h proto.c proto.h multicast.c multicast.h eval_helper.c eval_helper.h)
add_library(inih ini.c ini.h)
add_executable(testrunner runner.c)

if(${ASGARD_WITH_DPDK})

    if (NOT ${DPDK_FOUND})
        message( SEND_ERROR "DPDK installation not found on system" )
    endif ()
    add_compile_definitions(ASGARD_DPDK=${ASGARD_WITH_DPDK})
    target_link_libraries(libasraft PRIVATE ${DPDK_LIBRARIES} )
endif ()


target_link_libraries(libasraft PRIVATE Threads::Threads)
target_link_libraries(testrunner LINK_PUBLIC inih)
target_link_libraries(testrunner LINK_PUBLIC libasraft)
