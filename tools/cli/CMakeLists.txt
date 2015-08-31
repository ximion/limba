# CMakeLists for the Limba CLI tool

set(LIMBA_CLI_SRC
	limbacli.c
)

add_definitions("-DG_LOG_DOMAIN=\"LimbaCLI\"")

add_executable(limba_cli ${LIMBA_CLI_SRC} ${LIMBA_TOOLS_COMMON_SRC})
set_target_properties(limba_cli
	PROPERTIES OUTPUT_NAME limba)

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_SOURCE_DIR}/tools
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
)

target_link_libraries(limba_cli
		${GLIB_LIBRARIES}
		limba
)

install(TARGETS limba_cli RUNTIME DESTINATION bin)
