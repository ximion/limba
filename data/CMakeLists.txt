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

configure_file(org.freedesktop.Limba.service.in ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.Limba.service @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.Limba.service
	DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services/
	COMPONENT data
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.freedesktop.Limba.conf
	DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/dbus-1/system.d/
	COMPONENT data
)

# Install cleanup service if we have systemd
if (SYSTEMD_FOUND)
	configure_file(limba-cleanup.service.in ${CMAKE_CURRENT_BINARY_DIR}/limba-cleanup.service @ONLY)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/limba-cleanup.service
		DESTINATION ${SYSTEMD_SERVICES_INSTALL_DIR}
		COMPONENT data
	)
endif (SYSTEMD_FOUND)

add_subdirectory(mime)
add_subdirectory(policy)
