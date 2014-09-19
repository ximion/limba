# CMakeLists for the Listaller Project
project(listaller)

cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

#UNSET THIS VARIABLE AT RELEASE TIME
set(VERSION_SUFFIX "-dev")

set(CMAKE_BUILD_TYPE "Debug")

# Detect Git revision (if present)
if(VERSION_SUFFIX MATCHES "-dev")
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
		find_package(Git)
		if(GIT_EXECUTABLE)
		  execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
				    OUTPUT_VARIABLE project_revision RESULT_VARIABLE check_fail)
		  string(REGEX REPLACE "\n" "" project_revision ${project_revision})
		  if(check_fail)
		   message(STATUS "Could not fetch current Git revision: ${check_fail}")
		  else()
		   message(STATUS "Found Git revision: ${project_revision}")
		   set(VERSION_SUFFIX "${VERSION_SUFFIX}:${project_revision}")
		  endif(check_fail)
		endif(GIT_EXECUTABLE)
	endif()
endif()

set(LISTALLER_VERSION_MAJOR  "0")
set(LISTALLER_VERSION_MINOR  "6")
set(LISTALLER_VERSION_PATCH  "0")
set(LISTALLER_VERSION_SUFFIX "${VERSION_SUFFIX}")
set(LISTALLER_VERSION "${LISTALLER_VERSION_MAJOR}.${LISTALLER_VERSION_MINOR}.${LISTALLER_VERSION_PATCH}")

set(LISTALLER_API_LEVEL "0")

# Forbid in-tree building
if(${CMAKE_SOURCE_DIR} MATCHES ${CMAKE_BINARY_DIR})
      message(STATUS "Please do an out-of-tree build:")
      message(STATUS "rm -f CMakeCache.txt && mkdir build && cd build; cmake .. && make")
      message(FATAL_ERROR "In-tree-build detected!")
endif(${CMAKE_SOURCE_DIR} MATCHES ${CMAKE_BINARY_DIR})

#
# Options
#
option(MAINTAINER "Enable maintainer mode" OFF)
option(LIBUILD "Build and install the LIBuild facilities" OFF)
option(VISUAL_LDD "Build the Visual-LDD utility" OFF)
option(L10N "Enable localization" ON)

#
# Default paths
#
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
    "/usr" CACHE PATH "Default install prefix" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# Special case for /etc and /var when prefix is /usr
if(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
  set(CMAKE_INSTALL_SYSCONFDIR "/etc" CACHE PATH "read-only single-machine data (etc)")
  set(CMAKE_INSTALL_LOCALSTATEDIR "/var" CACHE PATH "modifiable single-machine data (var)")
endif(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
include(GNUInstallDirs)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/data/cmake/)

message(STATUS "Compiling Listaller version ${LISTALLER_VERSION}")

configure_file("${CMAKE_SOURCE_DIR}/data/cmake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

#
# Generic dependencies
#
# These packages are definitively needed and are used everywhere
find_package (Gettext REQUIRED)
find_package(GLIB 2.40 REQUIRED COMPONENTS "gio" "gobject")

#
# Configure files
#
set (GETTEXT_PACKAGE "listaller")
set (VERSION "${LISTALLER_VERSION}")
set (BUILDDIR "${CMAKE_BINARY_DIR}")
configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)

#
# Enable testing
#
enable_testing()

#
# Custom C flags
#
set(MAINTAINER_CFLAGS "-Werror -Wall -Wcast-align -Wno-uninitialized -Wempty-body -Wformat-security -Winit-self")
if(NOT MAINTAINER)
	set(MAINTAINER_CFLAGS "")
endif(NOT MAINTAINER)
add_definitions(${MAINTAINER_CFLAGS})

# Required if GPGme was compiled with _FILE_OFFSET_BITS=64
add_definitions("-D_FILE_OFFSET_BITS=64")

#
find_package(PkgConfig REQUIRED)

add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(contrib)
add_subdirectory(tests)
if (GETTEXT_FOUND AND L10N)
  add_subdirectory(po)
endif (GETTEXT_FOUND AND L10N)
