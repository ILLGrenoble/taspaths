#!/bin/bash
#
# rebuild external libs
# @author Tobias Weber <tweber@ill.fr>
# @date mar-2023
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


# -----------------------------------------------------------------------------
# tools
# -----------------------------------------------------------------------------
git_tool=git
cmake_tool=cmake
make_tool=make


# number of build processes
nproc_tool=$(which nproc)

if [ $? -ne 0 ]; then
	NUM_PROCS=4
else
	NUM_PROCS=$(($(${nproc_tool})/2+1))
fi

echo -e "Number of build processes: ${NUM_PROCS}."


# mingw build tools
if [ "$1" == "mingw" ]; then
	cmake_tool=mingw64-cmake
	make_tool=mingw64-make

	echo -e "Building using mingw."
fi
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
# URLs for external libraries
# -----------------------------------------------------------------------------
QHULL_REPO=https://github.com/qhull/qhull
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
# cleans externals
# -----------------------------------------------------------------------------
function clean_dirs()
{
	# remove old versions, but not if they're links
	if [ ! -L externals/qhull ]; then
		rm -rfv externals/qhull
	fi
	#if [ ! -L externals ]; then
	#	rm -rfv externals
	#fi
}
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
function rebuild_qhull()
{
	pushd externals

	if ! ${git_tool} clone ${QHULL_REPO}; then
		echo -e "QHull could not be cloned."
		exit -1
	fi

	cd qhull

	if ! ${cmake_tool} -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_VERBOSE_MAKEFILE=True \
		-DCMAKE_C_FLAGS="-fPIC" \
		-DCMAKE_CXX_FLAGS="-fPIC"; then
		echo -e "cmake failed for qhull."
		exit -1
	fi

	if ! ${make_tool} -j${NUM_PROCS}; then
		echo -e "make failed for qhull."
		exit -1
	fi

	if ! sudo ${make_tool} install; then
		echo -e "QHull could not be installed."
		exit -1
	fi

	popd
}
# -----------------------------------------------------------------------------



echo -e "--------------------------------------------------------------------------------"
echo -e "Removing old files and directories...\n"
clean_dirs
mkdir externals
echo -e "--------------------------------------------------------------------------------\n"

echo -e "--------------------------------------------------------------------------------"
echo -e "Downloading and building QHull...\n"
rebuild_qhull
echo -e "--------------------------------------------------------------------------------\n"
