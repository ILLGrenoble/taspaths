#
# TAS-Paths main cmake file
# @author Tobias Weber <tweber@ill.fr>
# @date feb-2021
# @license GPLv3, see 'LICENSE' file
#
# -----------------------------------------------------------------------------
# TAS-Paths (part of the Takin software suite)
# Copyright (C) 2021  Tobias WEBER (Institut Laue-Langevin (ILL),
#                     Grenoble, France).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# -----------------------------------------------------------------------------
#

cmake_minimum_required(VERSION 3.0)

project(taspaths)
enable_language(CXX)


list(APPEND CMAKE_MODULE_PATH
	"${PROJECT_SOURCE_DIR}"
	"${PROJECT_SOURCE_DIR}/cmake"
	"${PROJECT_SOURCE_DIR}/tlibs2/cmake"
)

#if(NOT "$ENV{LOCAL_BOOST_CMAKE_DIR}" STREQUAL "")
#	message("Using local boost cmake dir: $ENV{LOCAL_BOOST_CMAKE_DIR}.")
#
#	list(PREPEND CMAKE_MODULE_PATH
#		"$ENV{LOCAL_BOOST_CMAKE_DIR}"
#	)
#endif()


option(USE_QT6 "use qt 6" FALSE)
option(USE_LAPACK "use lapack" TRUE)
option(USE_OVD "use openvoronoi" FALSE)
option(USE_CGAL "use cgal" TRUE)
option(USE_OCV "use opencv" TRUE)
option(USE_PY "use python scripting" TRUE)
option(BUILD_TEST_TOOLS "build test tools" FALSE)
option(UNIT_TESTS "build unit tests" FALSE)


if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	set(CMAKE_VERBOSE_MAKEFILE TRUE)
endif()


set(GL_MAJOR_VER 3)
set(GL_MINOR_VER 3)

if(USE_QT6)
	set(QT_VER 6)
else()
	set(QT_VER 5)
endif()


message("Build type: ${CMAKE_BUILD_TYPE}.")
message("Compiler: " ${CMAKE_CXX_COMPILER_ID}.)
message("Selected Qt version ${QT_VER}.")
message("Selected GL version ${GL_MAJOR_VER}.${GL_MINOR_VER}.")


set(WARN_OPTS -Wall -Wextra -Weffc++)
add_compile_options(${WARN_OPTS})


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	# generate debug symbols
	add_compile_options(-g -ggdb)

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	add_compile_options("-DNDEBUG")
	add_compile_options("-Wno-#pragma-messages")
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# remove pragma messages
		# see: https://gcc.gnu.org/onlinedocs/gcc/Developer-Options.html
		add_compile_options(-fcompare-debug-second)
	endif()
endif()


# -----------------------------------------------------------------------------
# compiler settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-std=c++20)

add_definitions(-D_GL_MAJ_VER=${GL_MAJOR_VER} -D_GL_MIN_VER=${GL_MINOR_VER})
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# system specific settings
# -----------------------------------------------------------------------------
message("Building for ${CMAKE_SYSTEM_NAME} systems.")

set(BOOST_SUFFIX)
set(MINGW_WINSOCK)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# pass linker --subsystem option
	add_compile_options(-Wl,--subsystem,windows)

	set(BOOST_SUFFIX -x64)
	set(MINGW_WINSOCK "ws2_32")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	#add_compile_options(-mmacosx-version-min=10.15)
	add_compile_options(-mmacosx-version-min=11.0)
endif()

include_directories("${PROJECT_SOURCE_DIR}")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/externals")

if(QT_VER EQUAL 6)
	include_directories("${Qt6Core_INCLUDE_DIRS}/..")
elseif(QT_VER EQUAL 5)
	include_directories("${Qt5Core_INCLUDE_DIRS}/..")
endif()
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# packages
# -----------------------------------------------------------------------------
find_package(Threads REQUIRED)

set(Boost_NO_BOOST_CMAKE FALSE)
set(Boost_USE_MULTITHREADED TRUE)
set(Boost_FIND_QUIETLY FALSE)

find_package(Boost REQUIRED COMPONENTS filesystem${BOOST_SUFFIX})
message("Using Boost version ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}.")
include_directories("${Boost_INCLUDE_DIRS}/..")


find_package(Qhull)
if(Qhull_FOUND)
	add_definitions(-DUSE_QHULL)
	include_directories(BEFORE SYSTEM "${Qhull_INCLUDE_DIRS}" "${Qhull_INCLUDE_DIRS}/..")
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# hard-code the library path for mingw
	set(QCP_LIBRARIES "${PROJECT_SOURCE_DIR}/externals/qcustomplot/libqcustomplot_local.dll")
else()
	find_package(QCP)
	if(QCP_FOUND)
		include_directories("${QCP_INCLUDE_DIRS}")
	endif()
endif()


if(USE_OVD)
	find_package(OVD)
	if(OVD_FOUND)
		message("Openvoronoi enabled.")
		add_definitions(-DUSE_OVD)
		include_directories("${OVD_INCLUDE_DIRS}")
	else()
		message("Openvoronoi disabled.")
		set(OVD_LIBRARIES "")
	endif()
endif()


if(USE_OCV)
	find_package(OpenCV REQUIRED COMPONENTS core imgproc)

	if(OpenCV_FOUND)
		message("OpenCV enabled.")
		add_definitions(-DUSE_OCV)
		include_directories("${OpenCV_INCLUDE_DIRS}")
	else()
		message("OpenCV disabled.")
	endif()
endif()


if(USE_CGAL)
	message("CGAL enabled.")
	add_definitions(-DUSE_CGAL)
	set(CGAL_LIBRARIES "-lgmp")
endif()


if(USE_LAPACK)
	find_package(Lapacke)
	if(Lapacke_FOUND)
		message("Lapacke enabled.")
		add_definitions(-DUSE_LAPACK)
		include_directories("${Lapacke_INCLUDE_DIRS}")
	else()
		message("Lapacke disabled.")
	endif()
endif()


if(USE_PY)
	find_package(Python3 COMPONENTS Interpreter Development)
	find_package(SWIG COMPONENTS python)

	if(SWIG_FOUND AND SWIG_python_FOUND)
		message("Scripting using python version ${Python3_VERSION} enabled; packages: ${Python3_SITEARCH}.")

		cmake_policy(SET CMP0078 NEW)
		cmake_policy(SET CMP0086 NEW)

		set(UseSWIG_TARGET_NAME_PREFERENCE STANDARD)
		include(${SWIG_USE_FILE})

		set_source_files_properties(scripting/taspaths.i PROPERTIES CPLUSPLUS TRUE)
		set_source_files_properties(scripting/taspaths.i PROPERTIES SWIG_FLAGS "-I ${PROJECT_SOURCE_DIR}")

		swig_add_library(taspaths_py LANGUAGE python SOURCES scripting/taspaths.i)

		target_link_libraries(taspaths_py
			taspaths_core
			Python3::Python Threads::Threads
			"${Boost_LIBRARIES}" "${Lapacke_LIBRARIES}"
		)
	endif()
else()
	message("Python scripting disabled.")
endif()


if(QT_VER EQUAL 6)
	find_package(Qt6 REQUIRED
		COMPONENTS Core Gui Svg Widgets
			OpenGLWidgets PrintSupport)
	list(APPEND QtBaseLibraries Qt6::Core Qt6::Gui Qt6::Widgets
		Qt6::Svg)
	list(APPEND QtAllLibraries Qt6::Core Qt6::Gui Qt6::Widgets
		Qt6::Svg Qt6::OpenGLWidgets Qt6::PrintSupport)
elseif(QT_VER EQUAL 5)
	find_package(Qt5 REQUIRED
		COMPONENTS Core Gui Svg Widgets PrintSupport)
	list(APPEND QtBaseLibraries Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Svg)
	list(APPEND QtAllLibraries Qt5::Core Qt5::Gui Qt5::Widgets
		Qt5::Svg Qt5::PrintSupport)
else()
	message(FATAL_ERROR "Unknown Qt version selected: ${QT_VER}")
endif()

set(CMAKE_AUTOUIC TRUE)
set(CMAKE_AUTOMOC TRUE)
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# more compiler settings
# -----------------------------------------------------------------------------
add_compile_options(${Boost_CXX_FLAGS})

if(CMAKE_BUILD_TYPE STREQUAL "Release")
	# disable some warnings for generated source files
	set_source_files_properties("${PROJECT_BINARY_DIR}/taspaths_autogen/mocs_compilation.cpp"
		PROPERTIES COMPILE_FLAGS "-Wno-effc++")
endif()
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# target library settings
# -----------------------------------------------------------------------------
# core library
add_library(taspaths_core STATIC
	src/core/Geometry.cpp src/core/Geometry.h
	src/core/Axis.cpp src/core/Axis.h
	src/core/Instrument.cpp src/core/Instrument.h
	src/core/InstrumentSpace.cpp src/core/InstrumentSpace.h
	src/core/PathsBuilder.cpp src/core/PathsMeshBuilder.cpp src/core/PathsBuilder.h
	src/core/PathsExporter.cpp src/core/PathsExporter.h
	src/core/TasCalculator.cpp src/core/TasCalculator.h
	src/core/types.h

	src/libs/lines.h src/libs/graphs.h
	src/libs/voronoi.h src/libs/voronoi_lines.h
	src/libs/hull.h src/libs/img.h
	src/libs/ptree.h
)

set_property(TARGET taspaths_core
	PROPERTY POSITION_INDEPENDENT_CODE True)

target_link_libraries(taspaths_core
	"${Boost_LIBRARIES}"
	"${Lapacke_LIBRARIES}"
	#"${Qhull_LIBRARIES}"
	"${CGAL_LIBRARIES}"
	"${OpenCV_LIBRARIES}"
	Threads::Threads
)
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# target executable settings
# -----------------------------------------------------------------------------
add_executable(taspaths
	src/gui/main.cpp
	src/gui/PathsTool.cpp src/gui/PathsTool.h
	src/gui/settings_variables.cpp src/gui/settings_variables.h

	src/gui/renderer/PathsRenderer.cpp src/gui/renderer/PathsRenderer.h
	src/gui/renderer/Camera.h

	src/gui/dock/TASProperties.cpp src/gui/dock/TASProperties.h
	src/gui/dock/XtalProperties.cpp src/gui/dock/XtalProperties.h
	src/gui/dock/CoordProperties.cpp src/gui/dock/CoordProperties.h
	src/gui/dock/PathProperties.cpp src/gui/dock/PathProperties.h
	src/gui/dock/CamProperties.cpp src/gui/dock/CamProperties.h

	src/gui/dialogs/About.cpp src/gui/dialogs/About.h
	src/gui/dialogs/Licenses.cpp src/gui/dialogs/Licenses.h
	src/gui/dialogs/ConfigSpace.cpp src/gui/dialogs/ConfigSpace.h
	src/gui/dialogs/XtalConfigSpace.cpp src/gui/dialogs/XtalConfigSpace.h
	src/gui/dialogs/GeoBrowser.cpp src/gui/dialogs/GeoBrowser.h
	src/gui/dialogs/TextureBrowser.cpp src/gui/dialogs/TextureBrowser.h
	src/gui/dialogs/Settings.h

	src/gui/common/Resources.cpp src/gui/common/Resources.h

	src/libs/lines.h src/libs/graphs.h
	src/libs/voronoi.h src/libs/voronoi_lines.h
	src/libs/hull.h src/libs/img.h
	src/libs/ptree.h
	src/libs/proc.cpp src/libs/proc.h

	src/tools/hull.cpp src/tools/hull.h
	src/tools/lines.cpp src/tools/lines.h
	src/tools/poly.cpp src/tools/poly.h
	src/tools/vertex.cpp src/tools/vertex.h
	src/tools/about.cpp src/tools/about.h
	src/tools/info.cpp src/tools/info.h

	tlibs2/libs/qt/recent.cpp tlibs2/libs/qt/recent.h
	tlibs2/libs/qt/gl.cpp tlibs2/libs/qt/gl.h
	tlibs2/libs/maths.h #tlibs2/libs/cam.h

	#externals/qcustomplot/qcustomplot.cpp externals/qcustomplot/qcustomplot.h
)


if(BUILD_TEST_TOOLS)
	add_executable(taspaths-hull
		src/tools/hull_main.cpp
		src/tools/hull.cpp src/tools/hull.h
		src/tools/vertex.cpp src/tools/vertex.h
		src/tools/about.cpp src/tools/about.h
		src/gui/dialogs/Settings.h
		src/tools/settings_variables.cpp src/tools/settings_variables.h

		src/libs/lines.h src/libs/hull.h
		src/libs/voronoi.h

		tlibs2/libs/qt/recent.cpp tlibs2/libs/qt/recent.h
	)

	add_executable(taspaths-lines
		src/tools/lines_main.cpp
		src/tools/lines.cpp src/tools/lines.h
		src/tools/vertex.cpp src/tools/vertex.h
		src/tools/info.cpp src/tools/info.h
		src/tools/about.cpp src/tools/about.h
		src/gui/dialogs/Settings.h
		src/tools/settings_variables.cpp src/tools/settings_variables.h

		src/libs/lines.h src/libs/hull.h
		src/libs/voronoi_lines.h

		tlibs2/libs/qt/recent.cpp tlibs2/libs/qt/recent.h
	)

	add_executable(taspaths-poly
		src/tools/poly_main.cpp
		src/tools/poly.cpp src/tools/poly.h
		src/tools/vertex.cpp src/tools/vertex.h
		src/tools/about.cpp src/tools/about.h
		src/gui/dialogs/Settings.h
		src/tools/settings_variables.cpp src/tools/settings_variables.h

		src/libs/lines.h src/libs/hull.h
		src/libs/voronoi_lines.h

		src/gui/common/Recent.cpp src/gui/common/Recent.h
	)

	target_compile_definitions(taspaths-hull PUBLIC TASPATHS_TOOLS_STANDALONE)
	target_compile_definitions(taspaths-lines PUBLIC TASPATHS_TOOLS_STANDALONE)
	target_compile_definitions(taspaths-poly PUBLIC TASPATHS_TOOLS_STANDALONE)
endif()


#if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
#	# create an __info_plist section in the binary
#	target_link_options(taspaths
#		PRIVATE LINKER:-sectcreate,__TEXT,__info_plist,${PROJECT_SOURCE_DIR}/setup/osx/Info.plist
#	)
#endif()


target_link_libraries(taspaths
	taspaths_core
	"${Boost_LIBRARIES}"
	"${Lapacke_LIBRARIES}"
	"${Qhull_LIBRARIES}"
	"${CGAL_LIBRARIES}"
	"${OpenCV_LIBRARIES}"
	"${OVD_LIBRARIES}"
	"${MINGW_WINSOCK}"
	Threads::Threads
	"${QtAllLibraries}"
	"${QCP_LIBRARIES}"
)


if(BUILD_TEST_TOOLS)
	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		target_link_options(taspaths-hull
			PRIVATE LINKER:-sectcreate,__TEXT,__info_plist,${PROJECT_SOURCE_DIR}/setup/osx/hull.plist
		)

		target_link_options(taspaths-lines
			PRIVATE LINKER:-sectcreate,__TEXT,__info_plist,${PROJECT_SOURCE_DIR}/setup/osx/lines.plist
		)

		target_link_options(taspaths-poly
			PRIVATE LINKER:-sectcreate,__TEXT,__info_plist,${PROJECT_SOURCE_DIR}/setup/osx/poly.plist
		)
	endif()


	target_link_libraries(taspaths-hull
		"${Boost_LIBRARIES}"
		"${Lapacke_LIBRARIES}"
		"${Qhull_LIBRARIES}"
		##/usr/x86_64-w64-mingw32/sys-root/mingw/lib/libqhullstatic.a
		#/usr/x86_64-w64-mingw32/sys-root/mingw/lib/libqhullstatic_r.a
		##/usr/x86_64-w64-mingw32/sys-root/mingw/lib/libqhullcpp.a
		"${QtBaseLibraries}"
	)

	target_link_libraries(taspaths-lines
		"${Boost_LIBRARIES}"
		"${Lapacke_LIBRARIES}"
		"${CGAL_LIBRARIES}"
		"${OVD_LIBRARIES}"
		"${MINGW_WINSOCK}"
		Threads::Threads
		"${QtBaseLibraries}"
	)

	target_link_libraries(taspaths-poly
		"${Boost_LIBRARIES}"
		"${Lapacke_LIBRARIES}"
		"${QtBaseLibraries}"
	)
endif()



# link resource directory
#add_custom_command(
#	TARGET taspaths PRE_BUILD
#	COMMAND if [ ! -d res ]\; then ln -sf ../res\; fi
#)
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# unit tests
# -----------------------------------------------------------------------------
if(UNIT_TESTS)
	add_subdirectory(unittests)
endif()
# -----------------------------------------------------------------------------
