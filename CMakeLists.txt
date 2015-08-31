# CMakeLists for the Limba Project
project(limba)

cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

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

set(LIMBA_VERSION_MAJOR  "0")
set(LIMBA_VERSION_MINOR  "5")
set(LIMBA_VERSION_PATCH  "1")
set(LIMBA_VERSION "${LIMBA_VERSION_MAJOR}.${LIMBA_VERSION_MINOR}.${LIMBA_VERSION_PATCH}")

set(LIMBA_API_LEVEL "0")

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
option(LICOMPILE "Build and install the LiCompile facilities" OFF)
option(L10N "Enable localization" ON)
option(INSTALL_SUID "Install runapp with suid bit set" ON)

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

message(STATUS "Compiling Limba version ${LIMBA_VERSION}")

configure_file("${CMAKE_SOURCE_DIR}/data/cmake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

#
# External dependencies
#
find_package(Gettext REQUIRED)
find_package(GLIB 2.40 REQUIRED COMPONENTS "gio" "gobject" "gio-unix")
pkg_check_modules(APPSTREAM REQUIRED appstream>=0.8.2)
pkg_check_modules(UUID REQUIRED uuid>=2.0)
pkg_check_modules (POLKIT REQUIRED polkit-gobject-1>=0.104)

find_package(GI REQUIRED)
find_package(LibArchive REQUIRED)
find_package(GPGMe REQUIRED)
find_package(CURL REQUIRED)
find_library(M_LIB m)

# we want to use systemd, if possible
include(${CMAKE_SOURCE_DIR}/data/cmake/systemdservice.cmake)

# perform some basic checks on the kernel.
# very old kernels are a problem, so warn about it (but don't fail the build)
execute_process(
	COMMAND uname -r
	OUTPUT_VARIABLE KERNEL_RELEASE
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND uname -o
	OUTPUT_VARIABLE OS_NAME
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (KERNEL_RELEASE VERSION_LESS 4.0)
	message(WARNING "Kernel is too old. Limba might not work properly with it (needs at least OverlayFS from Linux 4.0).")
endif()

message(STATUS "Found ${OS_NAME} ${KERNEL_RELEASE}")

#
# Configure files
#
set (GETTEXT_PACKAGE "limba")
set (VERSION "${LIMBA_VERSION}")
set (BUILDDIR "${CMAKE_BINARY_DIR}")
set (SOFTWARE_ROOT_PREFIX "/opt/bundle")
set (SOFTWARE_INSTALL_ROOT "/opt/software")
configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)

#
# Enable testing
#
enable_testing()

#
# Custom C flags
#
set(MAINTAINER_CFLAGS "")
if(MAINTAINER)
	set(MAINTAINER_CFLAGS "-Werror -Wall -Wcast-align -Wno-uninitialized -Wempty-body -Wformat-security -Winit-self")
	if (CMAKE_COMPILER_IS_GNUCC)
		execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                OUTPUT_VARIABLE GCC_VERSION)
		if (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
			set(MAINTAINER_CFLAGS ${MAINTAINER_CFLAGS} "-fdiagnostics-color=auto")
		endif()
	endif()
endif(MAINTAINER)
add_definitions(${MAINTAINER_CFLAGS})

# Required if GPGme was compiled with _FILE_OFFSET_BITS=64
add_definitions("-D_FILE_OFFSET_BITS=64")

#
find_package(PkgConfig REQUIRED)

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(data)
add_subdirectory(docs)
add_subdirectory(tests)
if (GETTEXT_FOUND AND L10N)
  add_subdirectory(po)
endif (GETTEXT_FOUND AND L10N)
add_subdirectory(contrib)
