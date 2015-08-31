# CMakeLists for manual page docs

include (${CMAKE_SOURCE_DIR}/data/cmake/documentation.cmake)

set(manpages "runapp.1"
	"limba.1"
	"lipkgen.1"
)

if(LICOMPILE)
	set(manpages ${manpages}
		"ligcc.1"
		"lig++.1"
		"relaytool.1"
	)
endif()

add_xml_manpages(doc-man "${manpages}" "" "")
