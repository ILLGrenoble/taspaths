#!/bin/bash
#
# set the library path and starts TAS-Paths
#
# @author Tobias Weber <tweber@ill.fr>
# @date may-2023
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


# possible locations for the main binary
declare -a TASPATHS_BINS=(
	$(dirname $0)/taspaths
	$(dirname $0)/build/taspaths
)

TASPATHS_BIN_FOUND=0
for (( binidx=0; binidx<${#TASPATHS_BINS[@]}; ++binidx )); do
	TASPATHS_BIN=${TASPATHS_BINS[binidx]}

	if [ ! -f ${TASPATHS_BIN} ]; then
		# no binary at this path
		continue
	fi

	# binary has been found
	echo -e "Using TAS-Paths binary: ${TASPATHS_BIN}."
	TASPATHS_BIN_FOUND=1
done

if [ ${TASPATHS_BIN_FOUND} -eq 0 ]; then
	echo -e "Error: TAS-Paths binary cannot be found."
	exit -1
fi


# local qhull installation
QHULL_PATH_LOCAL=./externals/qhull-inst/usr/local/lib

# add library search paths
export LD_LIBRARY_PATH=${QHULL_PATH_LOCAL}:${LD_LIBRARY_PATH}
export DYLD_LIBRARY_PATH=${QHULL_PATH_LOCAL}:${DYLD_LIBRARY_PATH}


# start the program
${TASPATHS_BIN} "$@"
