# CMakeLists for documentation

# Find required tools
find_program (XMLTO xmlto)
mark_as_advanced (XMLTO)
if (NOT XMLTO)
	message (FATAL_ERROR "Xmlto was not found! Please install it to continue!")
endif (NOT XMLTO)

add_subdirectory (man)

# Build the actual documentation

find_program (SPHINXBUILD sphinx-build)
mark_as_advanced (SPHINXBUILD)
if (SPHINXBUILD STREQUAL "SPHINXBUILD-NOTFOUND")
	message (FATAL_ERROR "Sphinx was not found. Please install it to continue!")
endif (SPHINXBUILD STREQUAL "SPHINXBUILD-NOTFOUND")

add_custom_target (documentation
	COMMAND ${SPHINXBUILD} -b html
			-d ${CMAKE_CURRENT_SOURCE_DIR}/html/doctrees
			${CMAKE_CURRENT_SOURCE_DIR}/source
			${CMAKE_CURRENT_SOURCE_DIR}/html
	DEPENDS "source/conf.py"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM
)
