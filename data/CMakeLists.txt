
add_subdirectory(mime)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/foundations.list
	DESTINATION ${CMAKE_INSTALL_DATADIR}/limba
	COMPONENT data
)
