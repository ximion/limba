# CMakeLists for UAI Unit Tests

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)

link_libraries(${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		listaller
)

include_directories(${CMAKE_BINARY_DIR}
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}
		${GLIB2_INCLUDE_DIR}
		${GIO2_INCLUDE_DIR}
)

# Basic tests
add_executable(li-test-basics test-basics.c)
add_dependencies(li-test-basics listaller)
add_test(basic-test li-test-basics ${CMAKE_CURRENT_SOURCE_DIR})
