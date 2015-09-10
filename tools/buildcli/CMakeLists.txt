# CMakeLists for the Limba repository creator

# we need YAML to read the build configuration
pkg_check_modules(YAML REQUIRED yaml-0.1)

set(LIMBA_LIBUILD_SRC
	libuildcli.c
	li-build-conf.h
	li-build-conf.c
	li-build-master.h
	li-build-master.c
	li-build-templates.h
	li-build-templates.c
)

add_definitions("-DG_LOG_DOMAIN=\"LimbaBuildCLI\"")

add_executable(limba-build ${LIMBA_LIBUILD_SRC} ${LIMBA_TOOLS_COMMON_SRC})

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_SOURCE_DIR}/tools
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
			${YAML_INCLUDE_DIR}
			${APPSTREAM_INCLUDE_DIRS}
)

target_link_libraries(limba-build
		${GLIB_LIBRARIES}
		${YAML_LIBRARIES}
		${APPSTREAM_LIBRARIES}
		limba
)

install(TARGETS limba-build RUNTIME DESTINATION bin)
