cmake_minimum_required(VERSION 2.6)

PROJECT(cmocka-hello-tests)

ENABLE_TESTING()

if ("$ENV{ASGUARD_KERNEL_SRC}" STREQUAL "")
    message( FATAL_ERROR "You must set env variable ASGUARD_KERNEL_SRC" )
endif()



ADD_DEFINITIONS(-Wall -Werror --std=gnu99 -Wmissing-declarations)

FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/build/modules")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# List of tests and their flags
LIST(APPEND tests_names "hello_world")
LIST(APPEND tests_flags " ")
LIST(APPEND tests_categories "core")

LIST(APPEND tests_names "test_follower")
LIST(APPEND tests_flags " ")
LIST(APPEND tests_categories "raft")


# LIST(APPEND tests_names "tests_processes")
# LIST(APPEND tests_flags "-Wl,--wrap,popen -Wl,--wrap,fgets -Wl,--wrap,pclose")

# LIST(APPEND tests_names "tests_open_i2c")
# LIST(APPEND tests_flags "-Wl,--wrap,open -Wl,--wrap,ioctl -Wl,--wrap,close")

# LIST(APPEND tests_names "tests_do_something")
# LIST(APPEND tests_flags "-Wl,--wrap,close")

# Declare all tests targets
# Declare all tests targets
LIST(LENGTH tests_names count)
MATH(EXPR count "${count} - 1")
FOREACH(i RANGE ${count})
    LIST(GET tests_names ${i} test_name)
    LIST(GET tests_flags ${i} test_flags)
    LIST(GET tests_categories ${i} test_cat)

    ADD_EXECUTABLE(${test_name} ${test_cat}/${test_name}.c)
    FIND_PACKAGE(CMOCKA REQUIRED)
    INCLUDE_DIRECTORIES(${LIBCMOCKA_INCLUDE_DIR}
                # $ENV{ASGUARD_KERNEL_SRC}/arch/x86/include
                # $ENV{ASGUARD_KERNEL_SRC}/arch/x86/include/generated
                # $ENV{ASGUARD_KERNEL_SRC}/include/
                # $ENV{ASGUARD_KERNEL_SRC}/usr/include/
                # $ENV{ASGUARD_KERNEL_SRC}/net/asguard_drv/common/
                /usr/src/linux-headers-4.15.0-45/
                ../common/
                )
    TARGET_LINK_LIBRARIES(
        ${test_name}
        ${LIBCMOCKA_LIBRARIES}
        -fprofile-arcs -ftest-coverage
    )
    IF(test_flags STREQUAL " ")
    ELSE()
        TARGET_LINK_LIBRARIES(
            ${test_name}
            ${test_flags}
        )
    ENDIF()
    ADD_TEST(${test_name} ${test_name})
ENDFOREACH()

add_custom_target(check ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1
                  ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --verbose
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# ADD_EXECUTABLE(almost-calc-sum ${SOURCES})

# FIND_PACKAGE(CMOCKA REQUIRED)
# INCLUDE_DIRECTORIES(${LIBCMOCKA_INCLUDE_DIR})
# TARGET_LINK_LIBRARIES(almost-calc-sum ${LIBCMOCKA_LIBRARIES})

# INSTALL(TARGETS almost-calc-sum RUNTIME DESTINATION bin)