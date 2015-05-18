# CMakeLists for the Limba lipa tool

set(LIMBA_LIPA_SRC
	lipa.c
	${LIMBA_TOOLS_COMMON_SRC}
)

add_definitions("-DG_LOG_DOMAIN=\"Lipa\"")

add_executable(lipa ${LIMBA_LIPA_SRC})

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_BINARY_DIR}/src
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_SOURCE_DIR}/tools
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
)

target_link_libraries(lipa
		${GLIB_LIBRARIES}
		limba
)

install(TARGETS lipa RUNTIME DESTINATION bin)
