# CMakeLists for the Limba runapp helper

set(LIMBA_RUNAPP_SRC
	runapp.c
)

add_definitions("-DLI_COMPILATION"
	"-DG_LOG_DOMAIN=\"Limba\""
)

add_executable(runapp ${LIMBA_RUNAPP_SRC})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/setuid-root.in"
  "${CMAKE_CURRENT_BINARY_DIR}/setuid-root"
  IMMEDIATE @ONLY)

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_BINARY_DIR}/src
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
)

target_link_libraries(runapp
		${GLIB_LIBRARIES}
		limba
)

install(TARGETS runapp RUNTIME DESTINATION bin
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE SETUID
)
