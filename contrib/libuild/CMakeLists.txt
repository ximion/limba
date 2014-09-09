# CMakeLists for LI-BuildTools

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)

set(BUILDLIST_SOURCES buildlist.vala
)

link_libraries(${GLIB2_LIBRARIES}
	       ${GEE_LIBRARIES}
	       ${GIO2_LIBRARIES}
)

include_directories(${CMAKE_BINARY_DIR}
		    ${CMAKE_CURRENT_SOURCE_DIR}
		    ${GLIB2_INCLUDE_DIR}
		    ${GEE_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
)

add_definitions(${GEE_CFLAGS} ${GEE_CFLAGS_OTHER})

#vala_add_executable(buildlist ${BUILDLIST_SOURCES}
#				PACKAGES posix glib-2.0 gio-2.0 gee-0.8
#)

#install(TARGETS buildlist DESTINATION lib/listaller COMPONENT buildtools)
install(PROGRAMS ligcc
		 lig++
		 relaytool
	DESTINATION bin
	COMPONENT buildtools
)
install(FILES LiBuild/GCC.pm
	      LiBuild/Utils.pm
	DESTINATION share/listaller/LiBuild
	COMPONENT buildtools
)
install(FILES ctype.h DESTINATION include/libuild COMPONENT headers)
install(FILES relaytool.m4 DESTINATION share/aclocal COMPONENT headers)
