# CMakeLists for LI-BuildTools

set(BUILDLIST_SOURCES buildlist.vala
)

link_libraries(${GLIB_LIBRARIES}
	       ${GIO2_LIBRARIES}
)

include_directories(${CMAKE_BINARY_DIR}
		    ${CMAKE_CURRENT_SOURCE_DIR}
		    ${GLIB_INCLUDE_DIRS}
		    ${GEE_INCLUDE_DIR}
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
