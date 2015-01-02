# CMakeLists for the Limba helper daemon

set(LIMBA_DAEMON_SRC
	li-daemon.c
)

add_executable(limba-daemon ${LIMBA_DAEMON_SRC})
add_dependencies(limba-daemon limba)

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
			${POLKIT_INCLUDE_DIRS}
)

target_link_libraries(limba-daemon
		${GLIB_LIBRARIES}
		${POLKIT_LIBRARIES}
		limba
)

install(TARGETS limba-daemon RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/limba)
