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

# options
create_appdir=1
create_deb=1

# defines
APPNAME="taspaths"
APPDIRNAME="${APPNAME}"
APPDEBNAME="${APPNAME}.deb"

# install directories
BINDIR=/usr/local/bin
LIBDIR=/usr/local/lib
SHAREDIR=/usr/local/share
PY_DISTDIR=/usr/local/lib/python3.10/dist-packages
#PY_DISTDIR=/usr/lib/python3/dist-packages


#
# create application directories
#
if [ $create_appdir -ne 0 ]; then
	# clean up old dir
	rm -rfv "${APPDIRNAME}"

	# directories
	mkdir -p ${APPDIRNAME}${BINDIR}
	mkdir -p ${APPDIRNAME}${LIBDIR}
	mkdir -p ${APPDIRNAME}${SHAREDIR}/${APPNAME}/res
	mkdir -p ${APPDIRNAME}${SHAREDIR}/${APPNAME}/3rdparty_licenses
	mkdir -p ${APPDIRNAME}/usr/share/applications
	mkdir -p ${APPDIRNAME}/DEBIAN

	# debian package control file
	echo -e "Package: ${APPNAME}\nVersion: 1.4.5" > ${APPDIRNAME}/DEBIAN/control
	echo -e "Architecture: $(dpkg --print-architecture)" >> ${APPDIRNAME}/DEBIAN/control
	echo -e "Section: base\nPriority: optional" >> ${APPDIRNAME}/DEBIAN/control
	echo -e "Description: TAS pathfinding software" >> ${APPDIRNAME}/DEBIAN/control
	echo -e "Maintainer: tweber@ill.fr" >> ${APPDIRNAME}/DEBIAN/control

	# qcustomplot dependency is not needed if it's anyway installed in externals
	if [ "$1" == "noble" ] || [  "$1" == "" ]; then
		echo -e "Choosing debendencies for Noble..."

		echo -e "Depends:"\
			"libstdc++6 (>=10.0.0),"\
			"libboost-system1.74.0 (>=1.74.0),"\
			"libboost-filesystem1.74.0 (>=1.74.0),"\
			"libqhull-r8.0 (>=2020.2),"\
			"libqhullcpp8.0 (>=2020.2),"\
			"libgmp10 (>=2:6.2),"\
			"libqt5core5a (>=5.12.0),"\
			"libqt5gui5 (>=5.12.0),"\
			"libqt5widgets5 (>=5.12.0),"\
			"libqt5svg5 (>=5.12.0),"\
			"libqt5printsupport5 (>=5.12.0),"\
			"libqcustomplot2.1 (>=2.0.0),"\
			"python3 (>=3.10.0),"\
			"python3-matplotlib,"\
			"libopencv-core406t64,"\
			"libopencv-imgproc406t64,"\
			"liblapacke (>=3.11),"\
			"libopengl0 (>=1.3.0)\n" \
				>> ${APPDIRNAME}/DEBIAN/control

		PY_DISTDIR=/usr/local/lib/python3.10/dist-packages
	elif [ "$1" == "jammy" ]; then
		echo -e "Choosing debendencies for Jammy..."

		echo -e "Depends:"\
			"libstdc++6 (>=10.0.0),"\
			"libboost-system1.74.0 (>=1.74.0),"\
			"libboost-filesystem1.74.0 (>=1.74.0),"\
			"libqhull-r8.0 (>=2020.2),"\
			"libqhullcpp8.0 (>=2020.2),"\
			"libgmp10 (>=2:6.2),"\
			"libqt5core5a (>=5.12.0),"\
			"libqt5gui5 (>=5.12.0),"\
			"libqt5widgets5 (>=5.12.0),"\
			"libqt5svg5 (>=5.12.0),"\
			"libqt5printsupport5 (>=5.12.0),"\
			"libqcustomplot2.0 (>=2.0.0),"\
			"python3 (>=3.10.0),"\
			"python3-matplotlib,"\
			"libopencv-core4.5d,"\
			"libopencv-imgproc4.5d,"\
			"libopengl0 (>=1.3.0)\n" \
				>> ${APPDIRNAME}/DEBIAN/control

		PY_DISTDIR=/usr/local/lib/python3.10/dist-packages
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
			"python3 (>=3.8.0),"\
			"python3-matplotlib,"\
			"libopencv-core4.2,"\
			"libopencv-imgproc4.2,"\
			"libopengl0 (>=1.3.0)\n" \
				>> ${APPDIRNAME}/DEBIAN/control

		PY_DISTDIR=/usr/local/lib/python3.8/dist-packages
	else
		echo -e "Invalid target system: ${1}."
		exit -1
	fi

	# py directory
	mkdir -p ${APPDIRNAME}${PY_DISTDIR}

	# binaries
	cp -v build/taspaths		${APPDIRNAME}${BINDIR}
	cp -v build/taspaths-lines	${APPDIRNAME}${BINDIR}
	cp -v build/taspaths-hull	${APPDIRNAME}${BINDIR}
	cp -v build/taspaths-poly	${APPDIRNAME}${BINDIR}

	# libraries
	cp -v build/*.so		${APPDIRNAME}${LIBDIR}

	# py scripting files
	cp -v build/_taspaths_py.so	${APPDIRNAME}${PY_DISTDIR}
	cp -v build/taspaths.py		${APPDIRNAME}${PY_DISTDIR}

	# resources
	cp -rv res/*			${APPDIRNAME}${SHAREDIR}/${APPNAME}/res/
	cp -v AUTHORS			${APPDIRNAME}${SHAREDIR}/${APPNAME}/
	cp -v LICENSE			${APPDIRNAME}${SHAREDIR}/${APPNAME}/
	cp -rv 3rdparty_licenses/*	${APPDIRNAME}${SHAREDIR}/${APPNAME}/3rdparty_licenses/
	cp -v setup/deb/taspaths.desktop	${APPDIRNAME}/usr/share/applications

	# cleanups
	rm -v ${APPDIRNAME}${LIBDIR}/_taspaths_py.so

	# binary permissions & stripping
	chmod a+x ${APPDIRNAME}${BINDIR}/*
	strip -v ${APPDIRNAME}${BINDIR}/*
	strip -v ${APPDIRNAME}${LIBDIR}/*
	strip -v ${APPDIRNAME}${PY_DISTDIR}/*.so
fi


#
# create deb package
#
if [ $create_deb -ne 0 ]; then
	cd ${APPDIRNAME}/..
	chmod -R 775 ${APPDIRNAME}
	dpkg --build ${APPNAME}
fi
