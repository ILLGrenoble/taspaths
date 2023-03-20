#!/bin/bash
#
# updates version numbers
#
# @author Tobias Weber <tweber@ill.fr>
# @date dec-2021
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


# version to set
TASPATHS_VERSION="1.4.4"
TASPATHS_BUNDLE_VERSION="1440"


# sed tool
SED=$(which gsed)
if [ $? -ne 0 ]; then
	SED=$(which sed)
fi


${SED} -i "s/TASPATHS_VERSION \"[0-9].[0-9].[0-9]\"/TASPATHS_VERSION \"${TASPATHS_VERSION}\"/g" src/core/types.h
${SED} -i "s/Version: [0-9].[0-9].[0-9]/Version: ${TASPATHS_VERSION}/g" setup/deb/mk.sh


# plist files
declare -a PLIST_FILES=(setup/osx/Info.plist setup/osx/hull.plist setup/osx/lines.plist setup/osx/poly.plist setup/osx/Info.plist)

for (( fileidx=0; fileidx<${#PLIST_FILES[@]}; ++fileidx )); do
	PLIST_FILE=${PLIST_FILES[$fileidx]}

	${SED} -i "s/<string>[0-9].[0-9].[0-9]<\/string>/<string>${TASPATHS_VERSION}<\/string>/g" ${PLIST_FILE}
	${SED} -i "s/<string>[0-9][0-9][0-9][0-9]<\/string>/<string>${TASPATHS_BUNDLE_VERSION}<\/string>/g" ${PLIST_FILE}
done
