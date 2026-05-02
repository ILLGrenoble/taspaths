#
# finds the cgal libs
# @author Tobias Weber <tweber@ill.fr>
# @date 2-may-2026
# @license GPLv3
#
# -----------------------------------------------------------------------------
# TAS-Paths (part of the Takin software suite)
# Copyright (C) 2026  Tobias WEBER (Institut Laue-Langevin (ILL),
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


find_path(CGAL_INCLUDE_DIRS
	NAMES version.h
	PATH_SUFFIXES CGAL
	HINTS /usr/local/include/ /usr/include/ /opt/local/include /opt/homebrew/include
	DOC "CGAL include directories"
)


find_library(CGAL_LIBRARIES
	NAMES gmp
	PATH_SUFFIXES CGAL
	HINTS /usr/local/lib64 /usr/local/lib /usr/lib64 /usr/lib /opt/local/lib /opt/homebrew/lib
	DOC "CGAL libraries"
)


if(CGAL_INCLUDE_DIRS AND CGAL_LIBRARIES)
	set(CGAL_FOUND TRUE)

	message("CGAL include directories: ${CGAL_INCLUDE_DIRS}")
	message("CGAL libraries: ${CGAL_LIBRARIES}")
else()
	set(CGAL_FOUND FALSE)

	message("Error: CGAL could not be found!")
endif()
