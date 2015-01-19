
add_custom_target (merge-l10n-policy ALL
	${LC_INTLTOOL_MERGE} -x ${CMAKE_SOURCE_DIR}/po
		${CMAKE_CURRENT_SOURCE_DIR}/org.freedesktop.limba.policy.in
		${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.limba.policy
	DEPENDS org.freedesktop.limba.policy.in
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.limba.policy
	DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions/
	COMPONENT policy
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.freedesktop.limba.rules
	DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/rules.d/
	COMPONENT policy
)
