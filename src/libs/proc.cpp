/**
 * TAS path tool, process helpers
 * @author Tobias Weber <tweber@ill.fr>
 * @date August 2021
 * @license GPLv3, see 'LICENSE' file
 * @note Forked on 27-aug-2021 from my privately developed "misc" project (https://github.com/t-weber/misc).
 *
 * References:
 *  * https://www.boost.org/doc/libs/1_75_0/doc/html/boost_process/tutorial.html
 *  * https://www.boost.org/doc/libs/1_75_0/doc/html/process.html
 *  * https://www.boost.org/doc/libs/1_75_0/doc/html/boost/process/child.html
 *
 * ----------------------------------------------------------------------------
 * TAS-Paths (part of the Takin software suite)
 * Copyright (C) 2021       Tobias WEBER (Institut Laue-Langevin (ILL),
 *                          Grenoble, France).
 * "misc" project
 * Copyright (C) 2020-2021  Tobias WEBER (privately developed).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 */

#include "proc.h"

#if !defined(__MINGW32__) && !defined(__MINGW64__)
  #if __has_include(<boost/process/v1.hpp>)
		// new versions of boost
		#include <boost/process/v1.hpp>
		namespace proc = boost::process::v1;
  #else
		// old version of boost
		#include <boost/process.hpp>
		namespace proc = boost::process;
	#endif
	namespace this_proc = boost::this_process;
#endif


/**
 * start a sub-process
 */
void create_process(const std::string& binary)
{
#if !defined(__MINGW32__) && !defined(__MINGW64__)
	proc::spawn(binary.c_str(), this_proc::environment());
#else
	std::system((binary + "&").c_str());
#endif
}
