# CMakeLists for Limba data

# Find intltool-merge for XML internationalization
find_program (LC_INTLTOOL_MERGE intltool-merge)
mark_as_advanced (LC_INTLTOOL_MERGE)

if (LC_INTLTOOL_MERGE STREQUAL "LC_INTLTOOL_MERGE-NOTFOUND")
	message (FATAL_ERROR "Intltool was not found! Please install it to continue!")
endif (LC_INTLTOOL_MERGE STREQUAL "LC_INTLTOOL_MERGE-NOTFOUND")

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/foundations.list
	DESTINATION ${CMAKE_INSTALL_DATADIR}/limba
	COMPONENT data
)

configure_file(org.test.Limba.service.in ${CMAKE_CURRENT_BINARY_DIR}/org.test.Limba.service @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.test.Limba.service
	DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services/
	COMPONENT data
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.test.Limba.conf
	DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/dbus-1/system.d/
	COMPONENT data
)

add_subdirectory(mime)
add_subdirectory(policy)
