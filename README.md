# TAS-Paths
Pathfinding software for triple-axis spectrometers.

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.4625649.svg)](https://doi.org/10.5281/zenodo.4625649)


## Online Resources

### Website
*TAS-Path's* website can be found under [www.ill.eu/tas-paths](http://www.ill.eu/tas-paths).

### Downloads
The *Mac* version of *TAS-Paths* can be [downloaded from the App Store](https://apps.apple.com/app/id1594199491).
Versions for other systems are available on the software's website.

### Demonstration Videos
Basic tutorial videos are available here:
- [Basic pathfinding workflow](https://youtu.be/xs2BLuppQPQ).
- [Visualising the instrument's angular configuration space](https://youtu.be/WPUCVzMDKDc).


## Pathfinding Workflow
Steps to try out the pathfinding functionality:
- Move existing or add new walls or obstacles to the scene.
- Open the configuration space dialog using the "Calculation" -> "Angular Configuration Space..." menu item.
- Click the "Update Path Mesh" button in the configuration space dialog to compute the roadmap corresponding to the current instrument and wall configuration.
- Click the "Move Current Position" and "Move Target Position" radio buttons and click in the configuration space plot to set and move the start and target positions, respectively. 
- A path from the start to the target position is calculated. It can be directly traced by clicking the "Go" button in the main window's "Path Properties" dock window.


## Building TAS-Paths
### On *GNU/Linux*
- Install all required software for building (see list of external dependencies below):
  - On [*Ubuntu*](https://ubuntu.com): `sudo apt install git wget build-essential cmake libboost-all-dev qtbase5-dev libqt5svg5-dev libcgal-dev libqhull-dev libqcustomplot-dev swig libpython3-dev`.
  - On [*Fedora*](https://getfedora.org): `sudo dnf install git wget gcc cmake boost-devel qt5-qtbase-devel qt5-qtsvg-devel CGAL-devel qhull-devel qcustomplot-qt5-devel swig python3-devel`.
- Clone the source repository: `git clone https://github.com/ILLGrenoble/taspaths`.
- Go to the repository's root directory: `cd taspaths`.
- Get the external dependencies: `./setup/get_libs.sh`.
- Rebuild the latest versions of libraries with possible C++20 issues (see below): `./setup/rebuild_libs.sh`.
- Get the external licenses (for a release package): `./setup/get_3rdparty_licenses.sh`.
- Build *TAS-Paths* using: `./setup/release_build.sh`.
- Optionally create a package on *Ubuntu* using `./setup/deb/mk.sh`.
- The application can be started via `./build/taspaths`.

### On *Mac*
- Install the [*Homebrew*](https://brew.sh) package manager.
- Install all required software for building: `brew install git wget gcc cmake boost boost-python3 qt@5 cgal qhull swig python`.
- Clone the source repository: `git clone https://github.com/ILLGrenoble/taspaths`.
- Go to the repository's root directory: `cd taspaths`.
- Get the external dependencies: `./setup/get_libs.sh`.
- Rebuild the latest versions of libraries with possible C++20 issues (see below): `./setup/rebuild_libs.sh`.
- Get the external licenses (for a release package): `./setup/get_3rdparty_licenses.sh`.
- Build *TAS-Paths* using: `./setup/release_build.sh`.
- Optionally create a package using `./setup/osx/mk.sh`.
- The application can be started via `./build/taspaths`.

<!--
### For *MinGW* using *Fedora*
- Install all required software for building: `dnf install mingw64-gcc mingw64-gcc-c++ mingw64-boost mingw64-qt5-qtbase-devel mingw64-qt5-qtsvg mingw64-python3`.
-->

### Possible Compile Errors
Because *TAS-Paths* uses the still relatively new C++20 standard, there may be compatibility issues with older versions of some libraries.
- In case of compilation errors involving *QHull*, get and compile the latest version directly [from its source](https://github.com/qhull/qhull).
  This C++20 compatibility issue was solved [in late 2022](https://github.com/qhull/qhull/commit/bdd99371b995e02d6b39acc93221c477aafd284a).
- In case of *Boost* compilation errors, e.g. if *Boost's* auto_buffer.hpp complains that std::allocator<...>::pointer is missing (which was removed in C++20),
  get and compile the latest version directly [from its source](http://www.boost.org).
  This C++20 compatibility issue was solved [in early 2020](https://github.com/boostorg/signals2/commit/15fcf213563718d2378b6b83a1614680a4fa8cec).


## External Dependencies
|Library     |URL                                        |License URL                                                               |
|------------|-------------------------------------------|--------------------------------------------------------------------------|
|Boost       |http://www.boost.org                       |http://www.boost.org/LICENSE_1_0.txt                                      |
|CGAL        |https://www.cgal.org                       |https://github.com/CGAL/cgal/blob/master/Installation/LICENSE             |
|Qt          |https://www.qt.io                          |https://github.com/qt/qt5/blob/dev/LICENSE.QT-LICENSE-AGREEMENT           |
|QCustomPlot |https://www.qcustomplot.com                |https://gitlab.com/DerManu/QCustomPlot/-/raw/master/GPL.txt               |
|Lapack(e)   |https://www.netlib.org/lapack/lapacke.html |http://www.netlib.org/lapack/LICENSE.txt                                  |
|QHull       |http://www.qhull.org                       |https://github.com/qhull/qhull/blob/master/COPYING.txt                    |
|SWIG        |http://www.swig.org                        |https://github.com/swig/swig/blob/master/LICENSE                          |
|Python      |https://www.python.org                     |https://github.com/python/cpython/blob/main/Doc/license.rst               |
|Numpy       |https://numpy.org                          |https://github.com/numpy/numpy/blob/main/LICENSE.txt                      |
|Scipy       |https://www.scipy.org                      |https://github.com/scipy/scipy/blob/master/LICENSE.txt                    |
|Matplotlib  |https://matplotlib.org                     |https://github.com/matplotlib/matplotlib/blob/master/LICENSE/LICENSE      |
|tlibs       |https://doi.org/10.5281/zenodo.5717779     |https://code.ill.fr/scientific-software/takin/tlibs2/-/raw/master/LICENSE |
