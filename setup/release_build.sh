#!/bin/bash
#
# @author Tobias Weber <tweber@ill.fr>
# @date apr-2021
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
# options
# -----------------------------------------------------------------------------
USE_LAPACK=False
USE_QT6=False
USE_CGAL=True
USE_OVD=False
USE_PY=True
BUILD_EXTERNALS=1
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
# tools
# -----------------------------------------------------------------------------
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

	USE_CGAL=False
	USE_PY=False

	echo -e "Building using mingw."
fi
# -----------------------------------------------------------------------------



if [ $BUILD_EXTERNALS -ne 0 ]; then
	echo -e "--------------------------------------------------------------------------------"
	echo -e "Building external libraries..."
	echo -e "--------------------------------------------------------------------------------"

	cp -v CMakeLists_externals.txt externals/CMakeLists.txt

	rm -rfv externals/build
	mkdir externals/build
	pushd externals/build

	if ! ${cmake_tool} -DUSE_QT6=${USE_QT6} \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2" \
		-DCMAKE_VERBOSE_MAKEFILE=False ..
	then
		echo -e "cmake failed (external libraries)."
		exit -1
	fi

	#if ! ${cmake_tool} --build . --parallel ${NUM_PROCS}; then
	if ! ${make_tool} -j${NUM_PROCS}; then
		echo -e "make failed (external libraries)."
		exit -1
	fi

	strip -v libqcustomplot.*
	cp -v libqcustomplot.* ../qcustomplot/

	popd
	echo -e "--------------------------------------------------------------------------------"
fi


echo -e "--------------------------------------------------------------------------------"
echo -e "Creating Documentation..."
echo -e "--------------------------------------------------------------------------------"
rm -rfv res/dev_doc
doxygen setup/dev_doc
echo -e "--------------------------------------------------------------------------------"



echo -e "--------------------------------------------------------------------------------"
echo -e "Building TAS-Paths..."
echo -e "--------------------------------------------------------------------------------"

rm -rfv build
mkdir build
pushd build

if ! ${cmake_tool} -DUSE_LAPACK=${USE_LAPACK} -DUSE_QT6=${USE_QT6} \
	-DUSE_OVD=${USE_OVD} -DUSE_CGAL=${USE_CGAL} \
	-DUSE_PY=${USE_PY} \
	-DBUILD_TEST_TOOLS=False \
	-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2" \
	-DCMAKE_VERBOSE_MAKEFILE=False ..
then
	echo -e "cmake failed."
	exit -1
fi

#if ! ${cmake_tool} --build . --parallel ${NUM_PROCS}; then
if ! ${make_tool} -j${NUM_PROCS}; then
	echo -e "make failed."
	exit -1
fi

strip -v taspaths
strip -v taspaths-lines
strip -v taspaths-hull
strip -v taspaths-poly

if [ $BUILD_EXTERNALS -ne 0 ]; then
	cp -v ../externals/qcustomplot/libqcustomplot.* .
fi

popd
echo -e "--------------------------------------------------------------------------------"
