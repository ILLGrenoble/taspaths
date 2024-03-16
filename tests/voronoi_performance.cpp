/**
 * test performance of line segment voronoi diagram calculation
 * @author Tobias Weber <tweber@ill.fr>
 * @date 19-sep-2021
 * @license GPLv3, see 'LICENSE' file
 *
 * ----------------------------------------------------------------------------
 * TAS-Paths (part of the Takin software suite)
 * Copyright (C) 2021  Tobias WEBER (Institut Laue-Langevin (ILL),
 *                     Grenoble, France).
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
 *
 * g++-11 -std=c++20 -O2 -march=native -I../externals -I.. -DUSE_CGAL -DNDEBUG -o voronoi_performance voronoi_performance.cpp -lgmp
 */


#include <iostream>
#include <iomanip>
#include <vector>

#include "src/libs/voronoi_lines.h"
#include "src/core/types.h"
#include "tlibs2/libs/algos.h"


using t_real = double;
//using t_vector = tl2::vec<t_real, std::vector>;
//using t_matrix = tl2::mat<t_real, std::vector>;
using t_vector = t_vec2;
using t_matrix = t_mat22;
using t_line = std::pair<t_vector, t_vector>;


int main()
{
	std::cout.precision(8);

	std::cout
		<< std::left << std::setw(20) << "# number of lines "
		<< std::left << std::setw(20) << "time (boost)"
		<< std::left << std::setw(20) << "time (cgal)"
		<< std::left << std::setw(20) << "vertices (boost)"
		<< std::left << std::setw(20) << "vertices (cgal)"
		<< std::endl;


	for(std::size_t num_lines = 10; num_lines <= 500; num_lines += 10)
	{
		// create non-intersecting line segments
		std::vector<t_line> lines =
			geo::random_nonintersecting_lines<t_line, t_vec, t_matrix, t_real>
				(num_lines, 1e4, 1., 100., true);;

		// calculate the voronoi diagrams
		tl2::Stopwatch<t_real> timer_boost;
		timer_boost.start();
		auto res_boost = geo::calc_voro<t_vector, t_line>(lines);
		timer_boost.stop();

		tl2::Stopwatch<t_real> timer_cgal;
		timer_cgal.start();
		auto res_cgal = geo::calc_voro_cgal<t_vector, t_line>(lines);
		timer_cgal.stop();

		std::cout << std::left << std::setw(20) << num_lines
			<< std::left << std::setw(20) << timer_boost.GetDur()
			<< std::left << std::setw(20) << timer_cgal.GetDur()
			<< std::left << std::setw(20) << res_boost.GetVoronoiVertices().size()
			<< std::left << std::setw(20) << res_cgal.GetVoronoiVertices().size()
			<< std::endl;
	}

	return 0;
}
