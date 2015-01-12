# CMakeLists for the Limba repository creator

set(LIMBA_LIREPO_SRC
	lirepo.c
)

add_definitions("-DG_LOG_DOMAIN=\"Lirepo\"")

add_executable(lirepo ${LIMBA_LIREPO_SRC})

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
)

target_link_libraries(lirepo
		${GLIB_LIBRARIES}
		limba
)

install(TARGETS lirepo RUNTIME DESTINATION bin)
