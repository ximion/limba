# CMakeLists for the Limba library and daemon

include(GObjectIntrospectionMacros)
set(INTROSPECTION_GIRS)
set(INTROSPECTION_SCANNER_ARGS "--identifier-prefix=Li" "--symbol-prefix=li" "--warn-all" "--add-include-path=${CMAKE_CURRENT_SOURCE_DIR}")
set(INTROSPECTION_COMPILER_ARGS "--includedir=${CMAKE_CURRENT_SOURCE_DIR}")

set(LIBLIMBA_SRC
	li-utils.c
	li-config-data.c
	li-pkg-info.c
	li-pkg-index.c
	li-package.c
	li-runtime.c
	li-exporter.c
	li-installer.c
	li-manager.c
	li-pkg-builder.c
)

set(LIBLIMBA_PUBLIC_HEADERS
	limba.h
	li-utils.h
	li-pkg-info.h
	li-pkg-index.h
	li-package.h
	li-installer.h
	li-manager.h
	li-pkg-builder.h
)

set(LIBLIMBA_PRIVATE_HEADERS
	li-utils-private.h
	li-config-data.h
	li-exporter.h
	li-runtime.h
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

####
# GObject Introspection
macro(_list_prefix _outvar _listvar _prefix)
  set(${_outvar})
  foreach(_item IN LISTS ${_listvar})
    list(APPEND ${_outvar} ${_prefix}${_item})
  endforeach()
endmacro(_list_prefix)

set(introspection_files ${LIBLIMBA_SRC} ${LIBLIMBA_PUBLIC_HEADERS})
set(Limba_1_0_gir "limba")
set(Limba_1_0_gir_INCLUDES GObject-2.0 Gio-2.0 AppStream-0.7)
set(Limba_1_0_gir_CFLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/" "-DLI_COMPILATION")
set(Limba_1_0_gir_LIBS limba)

_list_prefix(Limba_1_0_gir_FILES introspection_files "${CMAKE_CURRENT_SOURCE_DIR}/")
set(Limba_1_0_gir_SCANNERFLAGS --c-include limba.h)
set(Limba_1_0_gir_EXPORT_PACKAGES limba)

list(APPEND INTROSPECTION_GIRS Limba-1.0.gir)
gir_add_introspections(INTROSPECTION_GIRS)
####

install(TARGETS limba DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${LIBLIMBA_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Limba)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/limba.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/)
install(DIRECTORY DESTINATION ${SOFTWARE_ROOT_PREFIX})
install(DIRECTORY DESTINATION ${SOFTWARE_INSTALL_ROOT})
