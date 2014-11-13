# CMakeLists for documentation

# Find required tools
find_program (XMLTO xmlto)
mark_as_advanced (XMLTO)
if (XMLTO STREQUAL "")
	message (FATAL_ERROR "Xmlto was not found! Please install it to continue!")
endif (XMLTO STREQUAL "")

add_subdirectory (man)
