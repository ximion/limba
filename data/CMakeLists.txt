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

########
# Find systemd

pkg_check_modules(SYSTEMD "systemd")
if (SYSTEMD_FOUND AND "${SYSTEMD_SERVICES_INSTALL_DIR}" STREQUAL "")
	execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
		--variable=systemdsystemunitdir systemd
		OUTPUT_VARIABLE SYSTEMD_SERVICES_INSTALL_DIR)
	string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SERVICES_INSTALL_DIR
		"${SYSTEMD_SERVICES_INSTALL_DIR}")
elseif (NOT SYSTEMD_FOUND AND SYSTEMD_SERVICES_INSTALL_DIR)
	message (FATAL_ERROR "Variable SYSTEMD_SERVICES_INSTALL_DIR is\
		defined, but we can't find systemd using pkg-config")
endif()

if (SYSTEMD_FOUND)
	set(WITH_SYSTEMD "ON")
	message(STATUS "systemd services install dir: ${SYSTEMD_SERVICES_INSTALL_DIR}")
else()
	message(STATUS "systemd was not found!")
	set(WITH_SYSTEMD "OFF")
endif (SYSTEMD_FOUND)

########

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
