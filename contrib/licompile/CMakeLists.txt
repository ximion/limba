# CMakeLists for LI CompileHelper

set(BUILDLIST_SOURCES
	buildlist.c
)

link_libraries(${GLIB_LIBRARIES}
	       ${GIO2_LIBRARIES}
	       limba
)

include_directories(${CMAKE_BINARY_DIR}
			${CMAKE_BINARY_DIR}/src
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_CURRENT_SOURCE_DIR}
			${GLIB_INCLUDE_DIRS}
)

add_executable(buildlist ${BUILDLIST_SOURCES})

install(TARGETS buildlist DESTINATION lib/licompile COMPONENT buildtools)
install(PROGRAMS ligcc
		 lig++
		 relaytool
	DESTINATION bin
	COMPONENT buildtools
)
install(FILES LiCompile/GCC.pm
	      LiCompile/Utils.pm
	DESTINATION share/licompile/LiCompile
	COMPONENT buildtools
)
install(FILES ctype.h DESTINATION include/licompile COMPONENT headers)
install(FILES relaytool.m4 DESTINATION share/aclocal COMPONENT headers)
