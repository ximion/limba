# CMakeLists for the Listaller library

find_package(GLIB2 2.36 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(GI REQUIRED)
find_library(M_LIB m)

include(GObjectIntrospectionMacros)
set(INTROSPECTION_GIRS)
set(INTROSPECTION_SCANNER_ARGS "--identifier-prefix=Li" "--symbol-prefix=li" "--warn-all" "--add-include-path=${CMAKE_CURRENT_SOURCE_DIR}")
set(INTROSPECTION_COMPILER_ARGS "--includedir=${CMAKE_CURRENT_SOURCE_DIR}")

set(LIBLISTALLER_SRC
		li-config-file.c
		li-ipk-package.c
)

set(LIBLISTALLER_PUBLIC_HEADERS
		listaller.h
		li-ipk-package.h
)

set(LIBLISTALLR_PRIVATE_HEADERS
		li-config-file.h
)

add_definitions("-DI_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE"
	"-DLI_COMPILATION"
	"-DG_LOG_DOMAIN=\"Listaller\""
)

# Variables used later to set library versioning
set(LILIB_VERSION "${LISTALLER_VERSION_MAJOR}.${LISTALLER_VERSION_MINOR}.${LISTALLER_VERSION_PATCH}")

# ensure that the GI compiler has been found (apparently, the automatic tests fail on some machines)
if (INTROSPECTION_COMPILER STREQUAL "")
  message(FATAL_ERROR "GObject-Introspection was not found! Please install it to continue.")
endif()

add_library(listaller SHARED ${LIBLISTALLER_SRC} ${LIBLISTALLER_PUBLIC_HEADERS} ${LIBLISTALLER_PRIVATE_HEADERS})
set_target_properties(listaller PROPERTIES VERSION ${LILIB_VERSION} SOVERSION ${LISTALLER_API_LEVEL})

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_CURRENT_SOURCE_DIR}
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
)

target_link_libraries(listaller
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${M_LIB}
)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/listaller.pc.in ${CMAKE_CURRENT_BINARY_DIR}/listaller.pc @ONLY)

####
# GObject Introspection
macro(_list_prefix _outvar _listvar _prefix)
  set(${_outvar})
  foreach(_item IN LISTS ${_listvar})
    list(APPEND ${_outvar} ${_prefix}${_item})
  endforeach()
endmacro(_list_prefix)

set(introspection_files ${LIBLISTALLER_SRC} ${LIBLISTALLER_PUBLIC_HEADERS})
set(Listaller_1_0_gir "listaller")
set(Listaller_1_0_gir_INCLUDES GObject-2.0 Gio-2.0)
set(Listaller_1_0_gir_CFLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/" "-DLI_COMPILATION")
set(Listaller_1_0_gir_LIBS listaller)

_list_prefix(Listaller_1_0_gir_FILES introspection_files "${CMAKE_CURRENT_SOURCE_DIR}/")
set(Listaller_1_0_gir_SCANNERFLAGS --c-include listaller.h)
set(Listaller_1_0_gir_EXPORT_PACKAGES listaller)

list(APPEND INTROSPECTION_GIRS Listaller-1.0.gir)
gir_add_introspections(INTROSPECTION_GIRS)
####

install(TARGETS listaller DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${LIBAPPSTREAM_PUBLIC_HEADERS} DESTINATION include/Listaller)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/listaller.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/)
