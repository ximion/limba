# CMakeLists for UAI Unit Tests

pkg_check_modules(APPSTREAM REQUIRED appstream)

link_libraries(${GLIB_LIBRARIES}
		${GLIB_GIO_LIBRARIES}
		${GLIB_GOBJECT_LIBRARIES}
		${APPSTREAM_LIBRARIES}
		listaller
)

include_directories(${CMAKE_BINARY_DIR}
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}
		${GLIB_INCLUDE_DIRS}
		${APPSTREAM_INCLUDE_DIRS}
)

# Basic tests
add_executable(li-test-basics test-basics.c)
add_dependencies(li-test-basics listaller)
add_test(basic-test li-test-basics ${CMAKE_CURRENT_SOURCE_DIR})
