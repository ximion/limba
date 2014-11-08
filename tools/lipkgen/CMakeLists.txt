# CMakeLists for the Limba package generator

set(LIMBA_LIPKGEN_SRC
	main.c
)

add_definitions("-DG_LOG_DOMAIN=\"Lipkgen\"")

add_executable(lipkgen ${LIMBA_LIPKGEN_SRC})

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
			${LibArchive_INCLUDE_DIR}
)

target_link_libraries(lipkgen
		${GLIB_LIBRARIES}
		${LibArchive_LIBRARIES}
		limba
)

install(TARGETS lipkgen RUNTIME DESTINATION bin)
