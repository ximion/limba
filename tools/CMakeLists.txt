
set (LIMBA_TOOLS_COMMON_SRC
	${CMAKE_SOURCE_DIR}/tools/li-console-utils.c
	${CMAKE_SOURCE_DIR}/tools/li-console-utils.h
)

add_subdirectory(runapp)
add_subdirectory(lipa)
add_subdirectory(lipkgen)
add_subdirectory(lirepo)
