cmake_minimum_required(VERSION 3.9)

if(HUNTER_ENABLED)
	include("cmake/HunterGate.cmake")
	HunterGate(
		URL "https://github.com/cpp-pm/hunter/archive/v0.23.311.tar.gz"
		SHA1 "1a82b9b73055879181cb1466b2ab5d48ee8ae410"
		LOCAL
	)
	message(STATUS "sqlitepp: using hunter for dependency resolution")
endif()
project(sqlitepp)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(HUNTER_ENABLED)
	hunter_add_package(sqlite3)
	find_package(sqlite3 CONFIG REQUIRED)
else()
	find_package(SQLite3 REQUIRED)
endif()

option(SQLITEPP_BUILD_TESTS "Configure CMake to build tests (or not)" OFF)

set(SQLITEPP_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SQLITEPP_CMAKE_FILES_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/cmake/sqlitepp)

set(SQLITEPP_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/database.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/error_code.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/orm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/result_iterator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/statement.cpp
)
set(SQLITEPP_HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlitepp/error_code.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlitepp/orm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlitepp/statement.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlitepp/fwd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlitepp/database.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlitepp/orm_entity.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlitepp/condition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlitepp/result_iterator.h
)
set(SQLITEPP_TEST_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/condition_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/database.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/orm.cpp
)

add_library(sqlitepp EXCLUDE_FROM_ALL ${SQLITEPP_SOURCE_FILES})
add_library(sqlitepp::sqlitepp ALIAS sqlitepp) # To match export
target_compile_features(sqlitepp PUBLIC cxx_std_17)
if(HUNTER_ENABLED)
	target_link_libraries(sqlitepp sqlite3::sqlite3)
else()
	target_link_libraries(sqlitepp SQLite::SQLite3)
endif()
target_include_directories(sqlitepp PUBLIC $<BUILD_INTERFACE:${SQLITEPP_INCLUDE_PATH}>
                                             $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/sqlitepp-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/sqlitepp-config.cmake
    INSTALL_DESTINATION ${SQLITEPP_CMAKE_FILES_INSTALL_DIR})
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/sqlitepp-config-version.cmake VERSION 0.1.0 COMPATIBILITY ExactVersion)
                                             
install(TARGETS sqlitepp EXPORT sqlitepp-targets PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT sqlitepp-targets NAMESPACE sqlitepp:: FILE sqlitepp-targets.cmake DESTINATION ${SQLITEPP_CMAKE_FILES_INSTALL_DIR})
install(FILES ${SQLITEPP_HEADER_FILES} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sqlitepp)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sqlitepp-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/sqlitepp-config-version.cmake
        DESTINATION ${SQLITEPP_CMAKE_FILES_INSTALL_DIR})

if(NOT TARGET uninstall)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        IMMEDIATE @ONLY)
    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()
      

if(SQLITEPP_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    include(GoogleTest)
    add_executable(sqlitepp-test ${SQLITEPP_TEST_FILES})
    target_link_libraries(sqlitepp-test PRIVATE sqlitepp GTest::GTest GTest::Main)
    gtest_add_tests(TARGET sqlitepp-test)
endif(SQLITEPP_BUILD_TESTS)
