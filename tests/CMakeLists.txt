# CMakeLists for Limba Unit Tests

link_libraries(${GLIB_LIBRARIES}
		${GLIB_GIO_LIBRARIES}
		${GLIB_GOBJECT_LIBRARIES}
		${APPSTREAM_LIBRARIES}
		limba
)

include_directories(${CMAKE_BINARY_DIR}
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}
		${GLIB_INCLUDE_DIRS}
		${APPSTREAM_INCLUDE_DIRS}
)

set(TESTRUNNER_SRC
	testrunner.h
	testrunner.c
)

# Test runner helper
add_executable(litestrunner ${TESTRUNNER_SRC})
add_dependencies(litestrunner limba)
set(LITESTRUNNER_EXEC ${CMAKE_CURRENT_BINARY_DIR}/litestrunner)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/testrunner-root.in"
  "${CMAKE_CURRENT_BINARY_DIR}/testrunner-root"
  IMMEDIATE @ONLY)
set(LITESTRUNNER_SUEXEC ${CMAKE_CURRENT_BINARY_DIR}/testrunner-root)

# Basic tests
add_executable(li-test-basics test-basics.c)
add_dependencies(li-test-basics limba)
add_test(basic-test li-test-basics ${CMAKE_CURRENT_SOURCE_DIR})

# Keyring tests
add_executable(li-test-keyring test-keyring.c)
add_dependencies(li-test-keyring limba litestrunner)
add_test(keyring-test ${LITESTRUNNER_SUEXEC} ${CMAKE_CURRENT_BINARY_DIR}/li-test-keyring ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TEST keyring-test APPEND PROPERTY DEPENDS basic-test)

# IPK package tests
add_executable(li-test-package test-package.c)
add_dependencies(li-test-package limba)
add_test(package-test li-test-package ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TEST package-test APPEND PROPERTY DEPENDS basic-test)
set_property(TEST package-test APPEND PROPERTY DEPENDS keyring-test)

# Software installation and management tests
add_executable(li-test-manager test-manager.c)
add_dependencies(li-test-manager limba litestrunner)
add_test(manager-test ${LITESTRUNNER_SUEXEC} ${CMAKE_CURRENT_BINARY_DIR}/li-test-manager ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TEST manager-test APPEND PROPERTY DEPENDS package-test)
