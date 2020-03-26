cmake_minimum_required(VERSION 3.10)
project(asguard C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# This is a Hack to use code inspection and linux kernel build via ADD_CUSTOM_TARGET in a docker container.
# Having code inspections with ADD_CUSTOM_TARGET is currently not possible.
# see: https://youtrack.jetbrains.com/issue/CPP-1028?_ga=2.247155716.1669537796.1581681496-1821123226.1581515981
#
# KBuild does a lot of work for us. We can call KBuild from CMake via ADD_CUSTOM_TARGET
# Using a CMake "native" method would require a reimplementation of the KBuild functionality with CMake logic.
if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(TARGET_ENV 0)
else()
    set(TARGET_ENV 1)
endif()


SET( MODULE_SOURCE_FILES
        core/Synbuf/kernel-space/synbuf-chardev.c
        core/Synbuf/kernel-space/include/synbuf-chardev.h
        common/asguard/asguard.h
        common/asguard/consensus.h
        common/asguard/logger.h
        common/asguard/payload_helper.h
        core/event_logger/event_logger.c
        core/event_logger/event_logger_ctrl.c
        core/logic_layer/consensus/include/asguard_consensus_ops.h
        core/logic_layer/consensus/include/candidate.h
        core/logic_layer/consensus/include/consensus_helper.h
        core/logic_layer/consensus/include/follower.h
        core/logic_layer/consensus/include/leader.h
        core/logic_layer/consensus/include/test.h
        core/logic_layer/consensus/asguard_consensus.c
        core/logic_layer/consensus/asguard_consensus_config_ctrl.c
        core/logic_layer/consensus/asguard_consensus_eval_ctrl.c
        core/logic_layer/consensus/asguard_consensus_ops.c
        core/logic_layer/consensus/asguard_consensus_request_gen.c
        core/logic_layer/consensus/candidate.c
        core/logic_layer/consensus/follower.c
        core/logic_layer/consensus/leader.c
        core/logic_layer/consensus/log.c
        core/logic_layer/consensus/state_transitions.c
        core/logic_layer/echo/include/asguard_echo.h
        core/logic_layer/echo/include/asguard_echo_ops.h
        core/logic_layer/echo/asguard_app_echo.c
        core/logic_layer/echo/asguard_echo.c
        core/logic_layer/echo/asguard_echo_ops.c
        core/logic_layer/fd/include/asguard_fd.h
        core/logic_layer/fd/include/asguard_fd_ops.h
        core/logic_layer/fd/include/asguard_fd_tx_procfs.h
        core/logic_layer/fd/include/asguard_fdtx.h
        core/logic_layer/fd/tx/asguard_fd_tx_procfs.c
        core/logic_layer/fd/asguard_fd.c
        core/logic_layer/fd/asguard_fd_ops.c
        core/logic_layer/fd/asguard_fdtx.c
        core/net/asguard_net.c
        core/net/asguard_async_queue.c
        core/net/payload_helper.c
        core/pm/asguard_pm.c
        core/pm/asguard_pm_ctrl.c
        core/proto/proto_instance_ctrl.c
        core/proto/proto_mgmt.c
        core/ts/asguard_ts.c
        core/ts/asguard_ts_ctrl.c
        core/asguard_core.c
        core/asguard_core.h
        core/asguard_core_ctrl.c
        core/asguard_payload_helper.c core/uface/cluster_membership.c core/uface/apply_data.c core/uface/consensus_request.c common/asguard/asgard_uface.h)

# Use KBuild System in Container to compile
if (TARGET_ENV EQUAL 1)

    MESSAGE("Using a ${CMAKE_HOST_SYSTEM_NAME} Container to Build!!!")

    SET(CMAKE_USE_RELATIVE_PATHS On)

    SET( ASGUARD_LKM_BIN    /home/cerebro/asguard-kernel/net/asguard/asguard.ko )
    SET( MODULE_SOURCE_DIR  /home/cerebro/asguard-kernel/net/asguard)

    Set( KERNEL_DIR "/home/cerebro/asguard-kernel")

    Set( KBUILD_CMD ${CMAKE_MAKE_PROGRAM} -C ${KERNEL_DIR} M=${MODULE_SOURCE_DIR} modules -j8)

    ADD_CUSTOM_COMMAND(OUTPUT ASGUARD_LKM_BIN
            COMMAND ${KBUILD_CMD}
            DEPENDS ${MODULE_SOURCE_FILES} VERBATIM
            )

    ADD_CUSTOM_TARGET(asguard ALL
            DEPENDS ASGUARD_LKM_BIN)


elseif(TARGET_ENV EQUAL 0)

    MESSAGE("Using Local Environment (No Container)")
    MESSAGE(${CMAKE_SOURCE_DIR})
    set(ASGUARD_KERNEL_ROOT /Volumes/Workspace/asguard/asguard-kernel/)

    execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY  ${ASGUARD_KERNEL_ROOT}
            OUTPUT_VARIABLE ASGUARD_KERNEL_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )


    message(STATUS "ASGuard Kernel Version: ${ASGUARD_KERNEL_VERSION}")
    message(STATUS "ASGuard Kernel src path: ${ASGUARD_KERNEL_ROOT}")

    add_definitions(-D__KERNEL__)
    add_definitions(-DMODULE)
    add_definitions(-std=c89)
    add_definitions(-ARCH=x86)


    set(KERNEL_HEADERS
            ${ASGUARD_KERNEL_ROOT}/arch/x86/include
            ${ASGUARD_KERNEL_ROOT}/arch/x86/include/generated/uapi
            ${ASGUARD_KERNEL_ROOT}/arch/x86/include/generated
            ${ASGUARD_KERNEL_ROOT}/include
            ${ASGUARD_KERNEL_ROOT}/arch/x86/include/uapi
            ${ASGUARD_KERNEL_ROOT}/include/uapi
            ${ASGUARD_KERNEL_ROOT}/include/generated/uapi
            ${ASGUARD_KERNEL_ROOT}/net/asguard_drv/common
            ${ASGUARD_KERNEL_ROOT}/usr/include/uapi
            )

    include_directories(${KERNEL_HEADERS})

    include_directories(core/Synbuf/kernel-space/include)
    include_directories(common)
    include_directories(common/asguard)
    include_directories(core)
    include_directories(core/logic_layer/consensus/include)
    include_directories(core/logic_layer/echo/include)
    include_directories(core/logic_layer/fd/include)

    add_executable(asguard ${MODULE_SOURCE_FILES})

else()
    MESSAGE("Unsupported Value for TARGET_ENV")
endif ()