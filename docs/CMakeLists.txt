# CMakeLists for documentation

# Find required tools
find_program (XMLTO xmlto)
mark_as_advanced (XMLTO)
if (NOT XMLTO)
	message (FATAL_ERROR "Xmlto was not found! Please install it to continue!")
endif (NOT XMLTO)

add_subdirectory (man)

# Build the actual documentation

if(DOCUMENTATION)
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
		DEPENDS "source/conf.py" doc-limba
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
	)
	add_custom_command(
		TARGET documentation POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/api/html" "${CMAKE_CURRENT_SOURCE_DIR}/html/api"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/html)

	#########################
	# API documentation

	find_package(GtkDoc)
	if(NOT GTKDOC_FOUND)
		message(FATAL_ERROR "gtkdoc was not found, API documentation can not be built.")
	endif()
	include(UseGtkDoc)

	gtk_doc_add_module(limba ${CMAKE_SOURCE_DIR}/src
		IGNOREHEADERS li-utils-private.h li-daemon-job.h li-package-private.h li-run.h
		SUFFIXES h c
		LDFLAGS
			-L${CMAKE_BINARY_DIR}/src
			-llimba
		LDPATH
			"${CMAKE_BINARY_DIR}/src"
		OUTPUT_DIR
			"${CMAKE_CURRENT_SOURCE_DIR}/api"
		DEPENDS limba)

	set_directory_properties (PROPERTIES CLEAN_NO_CUSTOM ${CMAKE_CURRENT_SOURCE_DIR}/api)
 	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/api/html;${CMAKE_CURRENT_SOURCE_DIR}/api/xml;${CMAKE_CURRENT_SOURCE_DIR}/api/tmpl")
endif()
