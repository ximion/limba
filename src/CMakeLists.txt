# CMakeLists for the Limba library and daemon

include(GObjectIntrospectionMacros)
set(INTROSPECTION_GIRS)
set(INTROSPECTION_SCANNER_ARGS "--identifier-prefix=Li" "--symbol-prefix=li" "--warn-all" "--add-include-path=${CMAKE_CURRENT_SOURCE_DIR}")
set(INTROSPECTION_COMPILER_ARGS "--includedir=${CMAKE_CURRENT_SOURCE_DIR}")

set(LIBLIMBA_SRC
	li-utils.c
	li-config-data.c
	li-ipk-control.c
	li-ipk-package.c
	li-installer.c
	li-manager.c
)

set(LIBLIMBA_PUBLIC_HEADERS
	limba.h
	li-utils.h
	li-ipk-control.h
	li-ipk-package.h
	li-installer.h
	li-manager.h
)

set(LIBLIMBA_PRIVATE_HEADERS
	li-utils-private.h
	li-config-data.h
)

add_definitions("-DLI_COMPILATION"
	"-DG_LOG_DOMAIN=\"Limba\""
)

# Variables used later to set library versioning
set(LILIB_VERSION "${LIMBA_VERSION_MAJOR}.${LIMBA_VERSION_MINOR}.${LIMBA_VERSION_PATCH}")

# ensure that the GI compiler has been found (apparently, the automatic tests fail on some machines)
if (NOT INTROSPECTION_FOUND)
  message(FATAL_ERROR "GObject-Introspection was not found! Please install it to continue.")
endif()

add_library(limba SHARED ${LIBLIMBA_SRC} ${LIBLIMBA_PUBLIC_HEADERS} ${LIBLIMBA_PRIVATE_HEADERS})
set_target_properties(limba PROPERTIES VERSION ${LILIB_VERSION} SOVERSION ${LIMBA_API_LEVEL})

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_CURRENT_BINARY_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
			${UUID_INCLUDE_DIRS}
			${APPSTREAM_INCLUDE_DIRS}
			${LibArchive_INCLUDE_DIR}
			${GPGME_VANILLA_INCLUDE_DIR}
)

target_link_libraries(limba
		${GLIB_LIBRARIES}
		${GLIB_GIO_LIBRARIES}
		${GLIB_GIO_UNIX_LIBRARIES}
		${GLIB_GOBJECT_LIBRARIES}
		${UUID_LIBRARIES}
		${M_LIB}
		${APPSTREAM_LIBRARIES}
		${LibArchive_LIBRARIES}
		${GPGME_VANILLA_LIBRARIES}
)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/limba.pc.in ${CMAKE_CURRENT_BINARY_DIR}/limba.pc @ONLY)

install(TARGETS limba DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${LIBLIMBA_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Limba)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/limba.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/)
