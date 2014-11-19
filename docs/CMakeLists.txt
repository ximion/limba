# CMakeLists for documentation

# Find required tools
find_program (XMLTO xmlto)
mark_as_advanced (XMLTO)
if (NOT XMLTO)
	message (FATAL_ERROR "Xmlto was not found! Please install it to continue!")
endif (NOT XMLTO)

add_subdirectory (man)
