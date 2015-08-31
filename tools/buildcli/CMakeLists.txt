# CMakeLists for the Limba repository creator

set(LIMBA_LIBUILD_SRC
	libuildcli.c
)

add_definitions("-DG_LOG_DOMAIN=\"LimbaBuildCLI\"")

add_executable(limba-build ${LIMBA_LIBUILD_SRC} ${LIMBA_TOOLS_COMMON_SRC})

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_SOURCE_DIR}/tools
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
)

target_link_libraries(limba-build
		${GLIB_LIBRARIES}
		limba
)

install(TARGETS limba-build RUNTIME DESTINATION bin)
