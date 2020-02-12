cmake_minimum_required(VERSION 3.15)
project(asguard C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ASGUARD_KERNEL_ROOT ${CMAKE_SOURCE_DIR}/../asguard-kernel)

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

set(KERNEL_HEADERS
        ${ASGUARD_KERNEL_ROOT}/arch/x86/include
        ${ASGUARD_KERNEL_ROOT}/arch/x86/include/generated/uapi
        ${ASGUARD_KERNEL_ROOT}/arch/x86/include/generated
        ${ASGUARD_KERNEL_ROOT}/include
        ${ASGUARD_KERNEL_ROOT}/arch/x86/include/uapi
        ${ASGUARD_KERNEL_ROOT}/include/uapi
        ${ASGUARD_KERNEL_ROOT}/include/generated/uapi
        ${ASGUARD_KERNEL_ROOT}/net/asguard_drv/common
        )


include_directories(${KERNEL_HEADERS})
#add_definitions(-include ${ASGUARD_KERNEL_ROOT}/include/linux/kconfig.h)

include_directories(common)
include_directories(common/asguard)
include_directories(core)
include_directories(core/logic_layer/consensus/include)
include_directories(core/logic_layer/echo/include)
include_directories(core/logic_layer/fd/include)

add_executable(sassy
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
        core/asguard_payload_helper.c)
