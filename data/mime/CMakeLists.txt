# CMakeLists to install Limba mime data

set (XDG_MIME_INSTALL_DIR "share/mime/packages")

add_custom_target (merge-l10n-mime ALL
	${LC_INTLTOOL_MERGE} -x ${CMAKE_SOURCE_DIR}/po
		${CMAKE_CURRENT_SOURCE_DIR}/x-ipk.xml.in
		${CMAKE_CURRENT_BINARY_DIR}/x-ipk.xml
	DEPENDS x-ipk.xml.in
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/x-ipk.xml
	DESTINATION ${XDG_MIME_INSTALL_DIR}
	COMPONENT data
)
