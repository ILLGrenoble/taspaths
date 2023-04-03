#!/bin/bash
#
# creates a debian package
# @author Tobias Weber <tweber@ill.fr>
# @date 2016, 23-may-2021
# @license GPLv3
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

create_appdir=1
create_deb=1
new_libs=0


# defines
APPNAME="taspaths"
APPDIRNAME="${APPNAME}"
APPDEBNAME="${APPNAME}.deb"


#
# create application directories
#
if [ $create_appdir -ne 0 ]; then
	# clean up old dir
	rm -rfv "${APPDIRNAME}"

	# directories
	mkdir -p ${APPDIRNAME}/usr/local/bin
	mkdir -p ${APPDIRNAME}/usr/local/lib
	mkdir -p ${APPDIRNAME}/usr/local/share/${APPNAME}/res
	mkdir -p ${APPDIRNAME}/usr/local/share/${APPNAME}/3rdparty_licenses
	mkdir -p ${APPDIRNAME}/usr/share/applications
	mkdir -p ${APPDIRNAME}/DEBIAN

	# debian package control file
	echo -e "Package: ${APPNAME}\nVersion: 1.4.5" > ${APPDIRNAME}/DEBIAN/control
	echo -e "Architecture: $(dpkg --print-architecture)" >> ${APPDIRNAME}/DEBIAN/control
	echo -e "Section: base\nPriority: optional" >> ${APPDIRNAME}/DEBIAN/control
	echo -e "Description: TAS pathfinding software" >> ${APPDIRNAME}/DEBIAN/control
	echo -e "Maintainer: tweber@ill.fr" >> ${APPDIRNAME}/DEBIAN/control

	# qcustomplot dependency is not needed if it's anyway installed in externals
	if [ "$1" == "jammy" ] || [  "$1" == "" ]; then
		echo -e "Choosing debendencies for Jammy..."

		echo -e "Depends:"\
			"libstdc++6 (>=10.0.0),"\
			"libboost-system1.74.0 (>=1.74.0),"\
			"libboost-filesystem1.74.0 (>=1.74.0),"\
			"libqhull-r8.0 (>=2020.2),"\
			"libgmp10 (>=2:6.2),"\
			"libqt5core5a (>=5.12.0),"\
			"libqt5gui5 (>=5.12.0),"\
			"libqt5widgets5 (>=5.12.0),"\
			"libqt5svg5 (>=5.12.0),"\
			"libqt5printsupport5 (>=5.12.0),"\
			"libqcustomplot2.0 (>=2.0.0),"\
			"libopengl0 (>=1.3.0)\n" \
				>> ${APPDIRNAME}/DEBIAN/control
	elif [ "$1" == "focal" ]; then
		echo -e "Choosing debendencies for Focal..."

		echo -e "Depends:"\
			"libstdc++6 (>=10.0.0),"\
			"libboost-system1.71.0 (>=1.71.0),"\
			"libboost-filesystem1.71.0 (>=1.71.0),"\
			"libqhull-r7 (>=2015.2),"\
			"libgmp10 (>=2:6.2),"\
			"libqt5core5a (>=5.12.0),"\
			"libqt5gui5 (>=5.12.0),"\
			"libqt5widgets5 (>=5.12.0),"\
			"libqt5svg5 (>=5.12.0),"\
			"libqt5printsupport5 (>=5.12.0),"\
			"libqcustomplot2.0 (>=2.0.0),"\
			"libopengl0 (>=1.3.0)\n" \
				>> ${APPDIRNAME}/DEBIAN/control
	else
		echo -e "Invalid target system: ${1}."
		exit -1
	fi


	# binaries
	cp -v build/taspaths		${APPDIRNAME}/usr/local/bin
	cp -v build/taspaths-lines	${APPDIRNAME}/usr/local/bin
	cp -v build/taspaths-hull	${APPDIRNAME}/usr/local/bin
	cp -v build/taspaths-poly	${APPDIRNAME}/usr/local/bin

	# libraries
	cp -v build/*.so		${APPDIRNAME}/usr/local/lib

	# resources
	cp -rv res/*			${APPDIRNAME}/usr/local/share/${APPNAME}/res/
	cp -v AUTHORS			${APPDIRNAME}/usr/local/share/${APPNAME}/
	cp -v LICENSE			${APPDIRNAME}/usr/local/share/${APPNAME}/
	cp -rv 3rdparty_licenses/*	${APPDIRNAME}/usr/local/share/${APPNAME}/3rdparty_licenses/
	cp -v setup/deb/taspaths.desktop	${APPDIRNAME}/usr/share/applications

	# binary permissions & stripping
	chmod a+x ${APPDIRNAME}/usr/local/bin/*
	strip -v ${APPDIRNAME}/usr/local/bin/*
	strip -v ${APPDIRNAME}/usr/local/lib/*
fi


#
# create deb package
#
if [ $create_deb -ne 0 ]; then
	cd ${APPDIRNAME}/..
	chmod -R 775 ${APPDIRNAME}
	dpkg --build ${APPNAME}
fi
