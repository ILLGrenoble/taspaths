/**
 * the paths builder comprises two steps:
 *   (1) it calculates the path mesh (i.e. the roadmap) of possible instrument
 *       paths (file: PathsMeshBuilder.cpp).
 *   (2) it calculates a specific path on the path mesh from the current to
 *       the target instrument position (file: PathsBuilder.cpp, this file)
 *
 * @author Tobias Weber <tweber@ill.fr>
 * @date jun-2021
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
 */

#include "PathsBuilder.h"
#include "tlibs2/libs/fit.h"

#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <cstdint>



// ----------------------------------------------------------------------------
// paths builder -- path calculation part
// ----------------------------------------------------------------------------
/**
 * get path length, taking into account the motor speeds
 */
t_real PathsBuilder::GetPathLength(const t_vec2& _vec) const
{
	// directly calculate length if motor speeds are not used
	if(!m_use_motor_speeds)
		return tl2::norm<t_vec2>(_vec);


	// move analysator instead of monochromator?
	bool kf_fixed = true;
	if(m_tascalc)
	{
		if(!std::get<1>(m_tascalc->GetKfix()))
			kf_fixed = false;
	}

	const Instrument& instr = m_instrspace->GetInstrument();

	// monochromator 2theta angular speed (alternatively analyser speed if kf is not fixed)
	t_real a2_speed = kf_fixed
		? instr.GetMonochromator().GetAxisAngleOutSpeed()
		: instr.GetAnalyser().GetAxisAngleOutSpeed();

	// sample 2theta angular speed
	t_real a4_speed = instr.GetSample().GetAxisAngleOutSpeed();

	t_vec2 vec = _vec;
	vec[0] /= a4_speed;
	vec[1] /= a2_speed;

	return tl2::norm<t_vec2>(vec);
}


/**
 * find a path from an initial (a2, a4) to a final (a2, a4)
 * the monochromator a1/a2 variables can alternatively refer to the analyser a5/a6 in case kf is not fixed
 */
InstrumentPath PathsBuilder::FindPath(
	t_real a2_i, t_real a4_i,
	t_real a2_f, t_real a4_f,
	PathStrategy pathstrategy)
{
	InstrumentPath path{};
	path.ok = false;

	// check if start or target point are within obstacles
	{
		if(!m_instrspace)
			return path;

		const t_real *sensesCCW = nullptr;
		std::size_t mono_idx = 0;
		bool kf_fixed = true;
		if(m_tascalc)
		{
			sensesCCW = m_tascalc->GetScatteringSenses();

			// move analysator instead of monochromator?
			if(!std::get<1>(m_tascalc->GetKfix()))
			{
				kf_fixed = false;
				mono_idx = 2;
			}
		}

		InstrumentSpace instrspace_cpy = *this->m_instrspace;
		Instrument& instr = instrspace_cpy.GetInstrument();

		// set instrument angles to start point
		t_real a2 = a2_i;
		t_real a4 = a4_i;
		if(sensesCCW)
		{
			a2 *= sensesCCW[mono_idx];
			a4 *= sensesCCW[1];
		}

		if(kf_fixed)
		{
			instr.GetMonochromator().SetAxisAngleOut(a2);
			instr.GetMonochromator().SetAxisAngleInternal(0.5 * a2);
		}
		else
		{
			instr.GetAnalyser().SetAxisAngleOut(a2);
			instr.GetAnalyser().SetAxisAngleInternal(0.5 * a2);
		}
		instr.GetSample().SetAxisAngleOut(a4);

		bool in_angular_limits = instrspace_cpy.CheckAngularLimits();
		bool colliding = instrspace_cpy.CheckCollision2D();
		if(!in_angular_limits || colliding)
			return path;

		// set instrument angles to target point
		a2 = a2_f;
		a4 = a4_f;
		if(sensesCCW)
		{
			a2 *= sensesCCW[mono_idx];
			a4 *= sensesCCW[1];
		}

		if(kf_fixed)
		{
			instr.GetMonochromator().SetAxisAngleOut(a2);
			instr.GetMonochromator().SetAxisAngleInternal(0.5 * a2);
		}
		else
		{
			instr.GetAnalyser().SetAxisAngleOut(a2);
			instr.GetAnalyser().SetAxisAngleInternal(0.5 * a2);
		}
		instr.GetSample().SetAxisAngleOut(a4);

		in_angular_limits = instrspace_cpy.CheckAngularLimits();
		colliding = instrspace_cpy.CheckCollision2D();
		if(!in_angular_limits || colliding)
			return path;
	}


	// convert angles to degrees
	a2_i *= 180. / tl2::pi<t_real>;
	a4_i *= 180. / tl2::pi<t_real>;
	a2_f *= 180. / tl2::pi<t_real>;
	a4_f *= 180. / tl2::pi<t_real>;

#ifdef DEBUG
	std::cout << "a4_i = " << a4_i << ", a2_i = " << a2_i
		<< "; a4_f = " << a4_f << ", a2_f = " << a2_f
		<< "." << std::endl;
#endif

	// vertices in configuration space
	path.vec_i = AngleToPixel(a4_i, a2_i, true);
	path.vec_f = AngleToPixel(a4_f, a2_f, true);

#ifdef DEBUG
	std::cout << "start pixel: (" << path.vec_i[0] << ", " << path.vec_i[1] << std::endl;
	std::cout << "target pixel: (" << path.vec_f[0] << ", " << path.vec_f[1] << std::endl;
#endif


	// test if a direct path is possible
	if(m_directpath)
	{
		// is distance between start and target point within search radius
		t_real dist_i_f = GetPathLength(
			PixelToAngle(path.vec_f, false, false) -
			PixelToAngle(path.vec_i, false, false));

		if(dist_i_f <= m_directpath_search_radius)
		{
			bool collides = DoesDirectPathCollidePixel(
				path.vec_i, path.vec_f, true);

			// direct-path shortcut found
			if(!collides)
			{
				path.ok = true;
				path.is_direct = true;
				return path;
			}
		}
	}


	// find closest voronoi vertices
	const auto& voro_vertices = m_voro_results.GetVoronoiVertices();

	// no voronoi vertices available
	if(voro_vertices.size() == 0)
		return path;


	std::vector<std::size_t> indices_i{};
	std::vector<std::size_t> indices_f{};
	std::size_t idx_i = 0;
	std::size_t idx_f = 0;

	// calculation of closest voronoi vertices using the index tree
	if(m_voro_results.GetIndexTreeSize())
	{
		// check closest voronoi vertices for a possible path from the initial position to a retraction point
		bool found_i = false;
		indices_i = m_voro_results.GetClosestVoronoiVertices(
			path.vec_i, m_num_closest_voronoi_vertices, true);

		// first look for the voronoi vertex where the path keeps the minimum
		// distance to the walls; second just use first non-colliding path
		for(bool use_min_dist : {true, false})
		{
			for(std::size_t _idx_i : indices_i)
			{
				bool collides = DoesDirectPathCollidePixel(
					path.vec_i, voro_vertices[_idx_i], use_min_dist);

				if(!collides)
				{
					idx_i = _idx_i;
					found_i = true;
					break;
				}
			}

			if(found_i)
				break;
		}

		if(!found_i)
		{
			//std::cerr << "Initial voronoi vertex not found!" << std::endl;
			path.ok = false;
			return path;
		}


		// check closest voronoi vertices for a possible path from the target position to a retraction point
		bool found_f = false;
		indices_f = m_voro_results.GetClosestVoronoiVertices(
			path.vec_f, m_num_closest_voronoi_vertices, true);

		for(bool use_min_dist : {true, false})
		{
			for(std::size_t _idx_f : indices_f)
			{
				bool collides = DoesDirectPathCollidePixel(
					path.vec_f, voro_vertices[_idx_f], use_min_dist);

				if(!collides)
				{
					idx_f = _idx_f;
					found_f = true;
					break;
				}
			}

			if(found_f)
				break;
		}

		if(!found_f)
		{
			//std::cerr << "Final voronoi vertex not found!" << std::endl;
			path.ok = false;
			return path;
		}
	}

	// alternate calculation without index tree
	else
	{
		t_real mindist_i = std::numeric_limits<t_real>::max();
		t_real mindist_f = std::numeric_limits<t_real>::max();

		for(std::size_t idx_vert = 0; idx_vert < voro_vertices.size(); ++idx_vert)
		{
			const t_vec2& cur_vert = voro_vertices[idx_vert];
			//std::cout << "cur_vert: " << cur_vert[0] << " " << cur_vert[1] << std::endl;

			t_vec2 diff_i = path.vec_i - cur_vert;
			t_vec2 diff_f = path.vec_f - cur_vert;

			t_real dist_i_sq = tl2::inner<t_vec2>(diff_i, diff_i);
			t_real dist_f_sq = tl2::inner<t_vec2>(diff_f, diff_f);

			if(dist_i_sq < mindist_i)
			{
				mindist_i = dist_i_sq;
				idx_i = idx_vert;
			}

			if(dist_f_sq < mindist_f)
			{
				mindist_f = dist_f_sq;
				idx_f = idx_vert;
			}
		}
	}

#ifdef DEBUG
	std::cout << "Nearest voronoi vertices: " << idx_i << ", " << idx_f << "." << std::endl;
#endif


	// find the shortest path between the voronoi vertices
	const auto& voro_graph = m_voro_results.GetVoronoiGraph();

	// are the graph vertex indices valid?
	if(idx_i >= voro_graph.GetNumVertices() || idx_f >= voro_graph.GetNumVertices())
		return path;

	using t_weight = typename t_graph::t_weight;

	// callback function with which the graph's edge weights can be modified
	auto weight_func = [this, &voro_graph, &voro_vertices, pathstrategy](
		std::size_t idx1, std::size_t idx2) -> std::optional<t_weight>
	{
		// get original graph edge weight
		auto _weight = voro_graph.GetWeight(idx1, idx2);
		if(!_weight)
			return std::nullopt;

		// shortest path -> just use original edge weights
		if(pathstrategy == PathStrategy::SHORTEST)
			return _weight;


		t_weight weight = *_weight;

		// get voronoi vertices of the current edge
		const t_vec2& vertex1 = voro_vertices[idx1];
		const t_vec2& vertex2 = voro_vertices[idx2];

		// get the distances to the wall vertices that are closest to the current voronoi vertices
		t_real dist1 = GetDistToNearestWall(vertex1);
		t_real dist2 = GetDistToNearestWall(vertex2);
		t_real min_dist = std::min(dist1, dist2);

		// modify edge weights using the minimum distance to the next wall
		if(pathstrategy == PathStrategy::PENALISE_WALLS)
			return weight / min_dist;

		return weight;
	};


	// execute dijkstra's algorithm
	auto find_shortest_path = [&weight_func, &voro_graph](
		std::size_t idx_initial, std::size_t idx_final)
			-> std::pair<bool, std::vector<std::size_t>>
	{
		const std::string& ident_initial = voro_graph.GetVertexIdent(idx_initial);

		// find shortest path given the above weight function
	#if TASPATHS_SSSP_IMPL==1
		auto predecessors = geo::dijk(voro_graph, ident_initial, &weight_func);
	#elif TASPATHS_SSSP_IMPL==2
		auto predecessors = geo::dijk_mod(voro_graph, ident_initial, &weight_func);
	#elif TASPATHS_SSSP_IMPL==3
		auto [distvecs, predecessors] = geo::bellman(
			voro_graph, ident_initial, &weight_func);
	#else
		#error No suitable value for TASPATHS_SSSP_IMPL has been set!
		return std::make_pair(false, {});
	#endif

		std::vector<std::size_t> voro_indices;
		voro_indices.reserve(predecessors.size());

		// index of final voronoi vertex
		std::size_t cur_vertidx = idx_final;
		bool ok = false;

		while(true)
		{
			voro_indices.push_back(cur_vertidx);

			// found full path?
			if(cur_vertidx == idx_initial)
			{
				ok = true;
				break;
			}

			auto next_vertidx = predecessors[cur_vertidx];
			if(!next_vertidx)
			{
				ok = false;
				break;
			}

			cur_vertidx = *next_vertidx;
		}

		std::reverse(voro_indices.begin(), voro_indices.end());
		return std::make_pair(ok, voro_indices);
	};


	// find shortest path from initial to final voronoi vertex
	std::tie(path.ok, path.voronoi_indices) = find_shortest_path(idx_i, idx_f);


#ifdef DEBUG
	std::cout << "Path ok: " << std::boolalpha << path.ok << std::endl;
	for(std::size_t idx=0; idx<path.voronoi_indices.size(); ++idx)
	{
		std::size_t voro_idx = path.voronoi_indices[idx];
		const t_vec2& voro_vertex = voro_vertices[voro_idx];
		const t_vec2 voro_angle = PixelToAngle(voro_vertex[0], voro_vertex[1], true);

		std::cout << "\tvertex index " << voro_idx << ": pixel ("
			<< voro_vertex[0] << ", " << voro_vertex[1] << "), angle ("
			<< voro_angle[0] << ", " << voro_angle[1] << ")"
			<< std::endl;
	}
#endif

	if(!path.ok)
		return path;


	// find the retraction points from the start/end point towards the path mesh
	if(path.voronoi_indices.size() >= 2)
	{
		// find closest start point
		std::size_t vert_idx1_begin = path.voronoi_indices[0];
		std::size_t vert_idx2_begin = path.voronoi_indices[1];

		auto [min_param_begin, bisector_begin, bisector_type_begin, collides_begin] =
			FindClosestBisector(vert_idx1_begin, vert_idx2_begin, path.vec_i);
		if(collides_begin)
		{
			path.ok = false;
			return path;
		}

		// another neighbour edge is closer
		// vert_idx1_begin is still the same
		if(std::get<1>(bisector_begin)==vert_idx1_begin && std::get<0>(bisector_begin)!=vert_idx2_begin)
		{
			path.voronoi_indices.insert(path.voronoi_indices.begin(), std::get<0>(bisector_begin));
		}
		// a completely different bisector has been found, use dijkstra again to find a path
		else if(std::get<1>(bisector_begin)!=vert_idx1_begin && std::get<0>(bisector_begin)!=vert_idx2_begin)
		{
			if(auto[pathseg_ok, pathseg] = find_shortest_path(vert_idx2_begin, std::get<1>(bisector_begin)); pathseg_ok)
			{
				path.voronoi_indices.erase(path.voronoi_indices.begin(), path.voronoi_indices.begin()+2);
				for(std::size_t pathseg_idx=0; pathseg_idx<pathseg.size(); ++pathseg_idx)
					path.voronoi_indices.insert(path.voronoi_indices.begin(), pathseg[pathseg_idx]);
				path.voronoi_indices.insert(path.voronoi_indices.begin(), std::get<0>(bisector_begin));

				geo::remove_path_loops<std::size_t>(path.voronoi_indices);
				if(path.voronoi_indices.size() < 2)
				{
					path.ok = false;
					return path;
				}
			}
		}

		path.param_i = min_param_begin;
		path.is_linear_i = (bisector_type_begin == 1);


		// find closest end point
		std::size_t vert_idx1_end = *path.voronoi_indices.rbegin();
		std::size_t vert_idx2_end = *(path.voronoi_indices.rbegin()+1);

		auto [min_param_end, bisector_end, bisector_type_end, collides_end] =
			FindClosestBisector(vert_idx1_end, vert_idx2_end, path.vec_f);
		if(collides_end)
		{
			path.ok = false;
			return path;
		}

		// another neighbour edge is closer
		// vert_idx1_end is still the same
		if(std::get<1>(bisector_end)==vert_idx1_end && std::get<0>(bisector_end)!=vert_idx2_end)
		{
			path.voronoi_indices.push_back(std::get<0>(bisector_end));
		}
		// a completely different bisector has been found, use dijkstra again to find a path
		else if(std::get<1>(bisector_end)!=vert_idx1_end && std::get<0>(bisector_end)!=vert_idx2_end)
		{
			if(auto[pathseg_ok, pathseg] = find_shortest_path(vert_idx2_end, std::get<1>(bisector_end)); pathseg_ok)
			{
				path.voronoi_indices.erase(path.voronoi_indices.end()-2, path.voronoi_indices.end());
				for(std::size_t pathseg_idx=0; pathseg_idx<pathseg.size(); ++pathseg_idx)
					path.voronoi_indices.push_back(pathseg[pathseg_idx]);
				path.voronoi_indices.push_back(std::get<0>(bisector_end));

				geo::remove_path_loops<std::size_t>(path.voronoi_indices);
				if(path.voronoi_indices.size() < 2)
				{
					path.ok = false;
					return path;
				}
			}
		}

		path.param_f = 1. - min_param_end;
		path.is_linear_f = (bisector_type_end == 1);
	}

	return path;
}


/**
 * get individual vertices on an instrument path
 * (in angular coordinates)
 */
std::vector<t_vec2> PathsBuilder::GetPathVertices(
	const InstrumentPath& path, bool subdivide_lines, bool deg) const
{
	// path vertices in angular coordinates (deg or rad)
	std::vector<t_vec2> path_vertices;

	if(!path.ok)
		return path_vertices;

	// is it a direct path?
	if(path.is_direct)
	{
		path_vertices.push_back(PixelToAngle(path.vec_i, deg));
		path_vertices.push_back(PixelToAngle(path.vec_f, deg));

		// interpolate points
		if(subdivide_lines)
		{
			path_vertices = geo::subdivide_lines<t_vec2>(
				path_vertices, m_subdiv_len);
		}

		return path_vertices;
	}

	const auto& voro_results = GetVoronoiResults();
	const auto& voro_vertices = voro_results.GetVoronoiVertices();

	bool kf_fixed = true;
	if(m_tascalc)
	{
		// move analyser instead of monochromator?
		if(!std::get<1>(m_tascalc->GetKfix()))
			kf_fixed = false;
	}

	InstrumentSpace instrspace_cpy = *this->m_instrspace;

	// convert pixel to angular coordinates and add vertex to path
	auto add_curve_vertex =
		[&path_vertices, &instrspace_cpy, kf_fixed, deg, this]
			(const t_vec2& vertex)
	{
		const t_vec2 angle = PixelToAngle(vertex, deg);
		bool insert_vertex = true;

		// check the generated vertex for collisions, and
		// remove it in that case
		if(this->m_verifypath)
		{
			const t_vec2 _angle = PixelToAngle(vertex, false, true);
			t_real a4 = _angle[0];
			t_real a2 = _angle[1];

			Instrument& instr = instrspace_cpy.GetInstrument();

			// set scattering and crystal angles
			if(kf_fixed)
			{
				instr.GetMonochromator().SetAxisAngleOut(a2);
				instr.GetMonochromator().SetAxisAngleInternal(0.5 * a2);
			}
			else
			{
				instr.GetAnalyser().SetAxisAngleOut(a2);
				instr.GetAnalyser().SetAxisAngleInternal(0.5 * a2);
			}

			instr.GetSample().SetAxisAngleOut(a4);
			//instr.GetSample().SetAxisAngleInternal(a3);

			bool angle_ok = instrspace_cpy.CheckAngularLimits();
			bool colliding = instrspace_cpy.CheckCollision2D();

			if(!angle_ok || colliding)
			{
				insert_vertex = false;

				//std::cout << "Collision at a2/6 = " << a2/tl2::pi<t_real>*180.
				//	<< " and a4 = " << a4/tl2::pi<t_real>*180. << std::endl;
			}
		}

		if(insert_vertex)
		{
			path_vertices.emplace_back(std::move(angle));
		}
	};


	// add starting point
	add_curve_vertex(path.vec_i);

	// iterate voronoi vertices and create path vertices
	for(std::size_t idx=1; idx<path.voronoi_indices.size(); ++idx)
	{
		std::size_t voro_idx = path.voronoi_indices[idx];
		const t_vec2& voro_vertex = voro_vertices[voro_idx];

		// check if the current one is a quadratic bisector
		std::size_t prev_voro_idx = path.voronoi_indices[idx-1];

		// find bisector for the given voronoi vertices
		auto iter_lin = voro_results.GetLinearEdges().find(
			std::make_pair(prev_voro_idx, voro_idx));
		bool has_lin = (iter_lin != voro_results.GetLinearEdges().end());
		auto iter_quadr = voro_results.GetParabolicEdges().find(
			std::make_pair(prev_voro_idx, voro_idx));
		bool has_quadr = (iter_quadr != voro_results.GetParabolicEdges().end());

		// determine if the current voronoi edge is a linear bisector
		bool is_linear_bisector = true;

		if(idx == 1 && path.voronoi_indices.size() > 1)
			is_linear_bisector = path.is_linear_i;
		else if(idx == path.voronoi_indices.size()-1 && idx > 1)
			is_linear_bisector = path.is_linear_f;
		else
			is_linear_bisector = (has_lin && !has_quadr);

		// it's a quadratic bisector
		if(!is_linear_bisector && has_quadr)
		{
			// get the vertices of the parabolic path segment
			if(const std::vector<t_vec2>& vertices = iter_quadr->second; vertices.size())
			{
				// get correct iteration order of bisector,
				// which is stored in an unordered fashion
				bool inverted_iter_order = false;

				if(tl2::equals<t_vec2>(vertices[0], voro_vertex, m_eps))
					inverted_iter_order = true;

				std::ptrdiff_t begin_idx = 0;
				std::ptrdiff_t end_idx = 0;

				// use the closest position on the path for the initial vertex
				if(idx == 1)
				{
					begin_idx = path.param_i * (vertices.size()-1);
					begin_idx = tl2::clamp<std::ptrdiff_t>(begin_idx, 0, vertices.size()-1);
				}

				// use the closest position on the path for the final vertex
				else if(idx == path.voronoi_indices.size()-1)
				{
					end_idx = (1.-path.param_f) * (vertices.size()-1);
					end_idx = tl2::clamp<std::ptrdiff_t>(end_idx, 0, vertices.size()-1);
				}

				if(inverted_iter_order)
				{
					for(auto iter_vert = vertices.rbegin()+begin_idx;
						iter_vert != vertices.rend()-end_idx;
						++iter_vert)
					{
						add_curve_vertex(*iter_vert);
					}
				}
				else
				{
					for(auto iter_vert = vertices.begin()+begin_idx;
						iter_vert != vertices.end()-end_idx;
						++iter_vert)
					{
						add_curve_vertex(*iter_vert);
					}
				}
			}
		}

		// just connect the voronoi vertices for linear bisectors
		else if(is_linear_bisector)
		{
			// use the closest position on the path for the initial vertex
			if(idx == 1 && path.voronoi_indices.size() > 1)
			{
				const t_vec2& voro_vertex1 = voro_vertices[path.voronoi_indices[0]];
				add_curve_vertex(voro_vertex1 + path.param_i*(voro_vertex-voro_vertex1));
			}
			// use the closest position on the path for the final vertex
			else if(idx == path.voronoi_indices.size()-1 && idx > 1)
			{
				const t_vec2& voro_vertex1 = voro_vertices[path.voronoi_indices[idx-1]];
				add_curve_vertex(voro_vertex1 + path.param_f*(voro_vertex-voro_vertex1));
			}
			else
			{
				add_curve_vertex(voro_vertex);
			}
		}
	}

	// add target point
	add_curve_vertex(path.vec_f);
	path_vertices = geo::simplify_path<t_vec2>(path_vertices);


	// try to find direct-path shortcuts around "loops" in the path
	if(m_directpath)
	{
		// test if a shortcut between the first and any other vertex on the path is possible
		RemovePathLoops(path_vertices, deg, false);

		// test if a shortcut between the last and any other vertex on the path is possible
		RemovePathLoops(path_vertices, deg, true);
	}


	// interpolate points on path line segments
	if(subdivide_lines)
	{
		path_vertices = geo::subdivide_lines<t_vec2>(path_vertices, m_subdiv_len);
		path_vertices = geo::remove_close_vertices<t_vec2>(path_vertices, m_subdiv_len);
	}


	// final verification (this only works with path subdivision also active)
	if(m_verifypath)
	{
		// check if we get closer to the walls than the minimum allowed distance
		/*std::vector<t_real> min_dists = GetDistancesToNearestWall(path_vertices, deg);
		if(auto min_elem = std::min_element(min_dists.begin(), min_dists.end()); min_elem != min_dists.end())
		{
			// ignore too close distances if the start or target point are already too close to a wall
			// (because in this case we want to move away from the walls)
			std::size_t min_idx = min_elem - min_dists.begin();

			if(min_idx > 0 && min_idx < min_dists.size()-1 &&
				*min_elem < m_min_angular_dist_to_walls)
				return {};
		}*/

		// check if we're colliding with the walls
		for(const t_vec2& pos : path_vertices)
		{
			if(DoesPositionCollide(pos, deg))
				return {};
		}
	}

	return path_vertices;
}


/**
 * get the angular distances to the nearest walls for each point of a given path
 * @arg path in angular coordinates (deg or rad)
 * @return vector of angular distances in rad
 */
std::vector<t_real> PathsBuilder::GetDistancesToNearestWall(
	const std::vector<t_vec2>& path, bool deg) const
{
	std::vector<t_real> dists{};
	dists.reserve(path.size());

	for(const t_vec2& pos : path)
	{
		t_vec2 pix = AngleToPixel(pos, deg, false);
		t_real dist = GetDistToNearestWall(pix);

		dists.push_back(dist);
	}

	return dists;
}


/**
 * get individual vertices on an instrument path
 * helper function for the scripting interface
 */
std::vector<std::pair<t_real, t_real>> PathsBuilder::GetPathVerticesAsPairs(
	const InstrumentPath& path, bool subdivide_lines, bool deg) const
{
	std::vector<t_vec2> vertices = GetPathVertices(path, subdivide_lines, deg);

	std::vector<std::pair<t_real, t_real>> pairs;
	pairs.reserve(vertices.size());

	for(const t_vec2& vec : vertices)
		pairs.emplace_back(std::make_pair(vec[0], vec[1]));

	return pairs;
}


/**
 * find the closest point on a bisector path segment
 * @arg vec starting position, in pixel coordinates
 * @returns [param, distance, 0:quadratic, 1:linear, -1:neither, retraction point]
 */
std::tuple<t_real, t_real, int, t_vec2>
PathsBuilder::FindClosestPointOnBisector(
	std::size_t idx1, std::size_t idx2, const t_vec2& vec) const
{
	// voronoi vertices at the bisector endpoints
	const auto& voro_vertices = m_voro_results.GetVoronoiVertices();
	const t_vec2& vert1 = voro_vertices[idx1];
	const t_vec2& vert2 = voro_vertices[idx2];


	// if the voronoi vertices belong to a linear bisector,
	// find the closest point by projecting 'vec' onto it
	t_real param_lin = -1;
	t_real dist_lin = std::numeric_limits<t_real>::max();
	t_vec2 pt_on_segment_lin{};

	const auto& lin_edges = m_voro_results.GetLinearEdges();
	auto lin_result = lin_edges.find({idx1, idx2});
	bool has_lin = false;

	if(lin_result != lin_edges.end())
	{
		t_vec2 dir = vert2 - vert1;
		t_real dir_len = tl2::norm<t_vec2>(dir);

		// the two voronoi vertices coincide
		if(dir_len < m_eps_angular)
		{
			return std::make_tuple(
				0, tl2::norm<t_vec2>(vec-vert1), 1, vec);
		}

		dir /= dir_len;

		auto [ptProj, _dist_lin, paramProj] =
			tl2::project_line<t_vec2, t_real>(
				vec, vert1, dir, true);

		param_lin = paramProj / dir_len;
		dist_lin = _dist_lin;
		pt_on_segment_lin = ptProj;
		has_lin = true;


		// look for another parameter if the projected vertex is too close to a wall
		t_real new_param_lin = param_lin;
		bool new_param_found = false;
		const t_real delta_param = 0.025;

		// initial distance to walls
		//t_vec2 vertex_1 = vert1 + dir*new_param_lin*dir_len;
		t_real dist_to_walls_1 = GetDistToNearestWall(/*vertex_1*/ ptProj);

		// direction for parameter search
		const t_real param_range[2] = { -1., 1. };
		bool increase_param = true;

		if(new_param_lin < param_range[0])
			increase_param = true;
		else if(new_param_lin > param_range[1])
			increase_param = false;
		else
		{
			// find direction for parameter search which decreases the distance to the walls
			t_vec2 vertex_2 = vert1 + dir*(new_param_lin + delta_param)*dir_len;
			t_real dist_to_walls_2 = GetDistToNearestWall(vertex_2);

			increase_param = (dist_to_walls_2 > dist_to_walls_1);
		}

		while(dist_to_walls_1 < m_min_angular_dist_to_walls)
		{
			if(increase_param)
				new_param_lin += delta_param;
			else
				new_param_lin -= delta_param;

			// vertex is far enough from any wall?
			t_vec2 new_vertex = vert1 + dir*new_param_lin*dir_len;
			t_real dist_to_walls = GetDistToNearestWall(new_vertex);

			// found a better position?
			if(dist_to_walls > dist_to_walls_1)
			{
				new_param_found = true;

				// out of critical distance?
				if(dist_to_walls >= m_min_angular_dist_to_walls)
					break;
			}

			// not yet in target range?
			if((increase_param && new_param_lin < param_range[0]) ||
				(!increase_param && new_param_lin > param_range[1]))
				continue;

			// end of parameter search?
			if(new_param_lin > param_range[1] || new_param_lin < param_range[0])
				break;
		}

		// a new parameter farther from the walls has been found
		if(new_param_found)
		{
			new_param_lin = tl2::clamp<t_real>(new_param_lin, param_range[0], param_range[1]);
			t_vec2 new_vertex = vert1 + dir*new_param_lin*dir_len;

			param_lin = new_param_lin;
			dist_lin = tl2::norm<t_vec2>(new_vertex-vec);
			pt_on_segment_lin = new_vertex;
		}
	}


	// if the voronoi vertices belong to a quadratic bisector,
	// find the closest vertex along its segment
	t_real param_quadr = -1;
	t_real dist_quadr = std::numeric_limits<t_real>::max();
	t_vec2 pt_on_segment_quadr{};

	const auto& para_edges = m_voro_results.GetParabolicEdges();
	auto para_result = para_edges.find({idx1, idx2});
	bool has_quadr = false;

	if(para_result != para_edges.end())
	{
		// get correct iteration order of the bisector,
		// which is stored in an unordered fashion
		bool inverted_iter_order = false;
		const auto& path_vertices = para_result->second;
		if(path_vertices.size() && tl2::equals<t_vec2>(path_vertices[0], vert2, m_eps))
			inverted_iter_order = true;

		t_real min_dist2 = std::numeric_limits<t_real>::max();
		std::size_t min_idx = 0;
		for(std::size_t vertidx=0; vertidx<path_vertices.size(); ++vertidx)
		{
			const auto& path_vertex = path_vertices[vertidx];
			t_real dist2 = tl2::inner<t_vec2>(path_vertex-vec, path_vertex-vec);
			if(dist2 < min_dist2)
			{
				// reject vertex if the minimum distance to the walls is undercut
				t_real dist_to_walls = GetDistToNearestWall(path_vertex);
				if(dist_to_walls < m_min_angular_dist_to_walls)
					continue;

				min_dist2 = dist2;
				min_idx = vertidx;
				pt_on_segment_quadr = path_vertex;
			}
		}

		// use the vertex index as curve parameter
		param_quadr = t_real(min_idx)/t_real(path_vertices.size()-1);
		dist_quadr = std::sqrt(min_dist2);
		if(inverted_iter_order)
			param_quadr = 1. - param_quadr;
		has_quadr = true;
	}


	// only a linear bisector segment was found
	if(has_lin && !has_quadr)
	{
		return std::make_tuple(param_lin, dist_lin, 1, pt_on_segment_lin);
	}
	// only a quadratic bisector segment was found
	else if(has_quadr && !has_lin)
	{
		return std::make_tuple(param_quadr, dist_quadr, 0, pt_on_segment_quadr);
	}
	// neither bisector segment was found
	else if(!has_lin && !has_quadr)
	{
		return std::make_tuple(param_quadr, dist_quadr, -1, t_vec2{});
	}
	// both bisector segment types were found
	else
	{
		// firstly prefer the one with the parameters in the [0..1] range
		if((param_quadr < 0. || param_quadr > 1.) && (param_lin >= 0. && param_lin <= 1.))
			return std::make_tuple(param_lin, dist_lin, 1, pt_on_segment_lin);
		else if((param_lin < 0. || param_lin > 1.) && (param_quadr >= 0. && param_quadr <= 1.))
			return std::make_tuple(param_quadr, dist_quadr, 0, pt_on_segment_quadr);

		// secondly prefer the one which is closest
		if(dist_lin < dist_quadr)
		{
			//std::cout << "linear bisector segment: dist=" << dist_lin << ", param=" << param_lin << std::endl;
			return std::make_tuple(param_lin, dist_lin, 1, pt_on_segment_lin);
		}
		else
		{
			//std::cout << "quadratic bisector segment: dist=" << dist_quadr << ", param=" << param_quadr << std::endl;
			return std::make_tuple(param_quadr, dist_quadr, 0, pt_on_segment_quadr);
		}
	}
}


/**
 * find a neighbour bisector which is closer to the given vertex than the given one
 * @arg vert given vertex in pixel coordinates
 * @returns [param, min dist bisector, bisector_type, collides]
 */
std::tuple<t_real, std::pair<std::size_t, std::size_t>, int, bool>
PathsBuilder::FindClosestBisector(
	std::size_t vert_idx_end, std::size_t vert_idx_before_end,
	const t_vec& vert) const
{
	bool use_min_dist = false;

	const auto& voro_graph = m_voro_results.GetVoronoiGraph();
	const auto& voro_vertices = m_voro_results.GetVoronoiVertices();

	// invalid indices
	if(vert_idx_end >= voro_vertices.size() || vert_idx_before_end >= voro_vertices.size())
		return std::make_tuple(0, std::make_pair(0, 0), -1, true);

	auto min_bisector = std::make_pair(vert_idx_before_end, vert_idx_end);

	auto [min_param, min_dist, bisector_type, pt_on_segment] =
		FindClosestPointOnBisector(vert_idx_end, vert_idx_before_end, vert);
	bool collides = DoesDirectPathCollidePixel(vert, pt_on_segment, use_min_dist);
	//std::cout << "bisector " << vert_idx_end << " " << vert_idx_before_end
	//	<< " collides: " << std::boolalpha << collides << std::endl;

	using t_bisector = std::pair<std::size_t, std::size_t>;

	// check if any neighbour bisector connecting to this one is even closer
	std::vector<std::size_t> neighbour_indices_end =
		voro_graph.GetNeighbours(vert_idx_end);
	std::vector<std::size_t> neighbour_indices_before_end =
		voro_graph.GetNeighbours(vert_idx_before_end);

	// add all bisector edges connected to the two vertices of the original bisector
	std::vector<t_bisector> next_bisectors;
	next_bisectors.reserve(neighbour_indices_end.size() + neighbour_indices_before_end.size());

	for(std::size_t neighbour_idx : neighbour_indices_end)
	{
		if(neighbour_idx < voro_vertices.size() && neighbour_idx!=vert_idx_end)
			next_bisectors.emplace_back(std::make_pair(neighbour_idx, vert_idx_end));
	}
	for(std::size_t neighbour_idx : neighbour_indices_before_end)
	{
		if(neighbour_idx < voro_vertices.size() && neighbour_idx!=vert_idx_before_end)
			next_bisectors.emplace_back(std::make_pair(vert_idx_before_end, neighbour_idx));
	}

	std::size_t num_first_order_neighbours = next_bisectors.size();


	// set of already visited bisectors
	std::unordered_set<t_bisector,
		geo::t_bisector_hash<t_bisector>,
		geo::t_bisector_equ<t_bisector>> seen_bisectors;
	seen_bisectors.insert(std::make_pair(vert_idx_end, vert_idx_before_end));

	for(std::size_t bisector_idx=0; bisector_idx<next_bisectors.size(); ++bisector_idx)
	{
		// can't take reference as this vector is modified in the loop
		t_bisector bisector = next_bisectors[bisector_idx];
		//std::cout << "visiting bisector " << std::get<0>(bisector) << " " << std::get<1>(bisector) << std::endl;

		if(seen_bisectors.find(bisector) != seen_bisectors.end())
		{
			//std::cout << "bisector already visited" << std::endl;
			continue;
		}
		seen_bisectors.insert(bisector);

		// TODO: add newly discovered neighbours
		// only consider first-order nearest neighbours
		// (except when the current path collides)
		if(bisector_idx < num_first_order_neighbours || collides)
		{
			for(std::size_t new_neighbour_idx :
				voro_graph.GetNeighbours(std::get<0>(bisector)))
			{
				if(new_neighbour_idx < voro_vertices.size() && new_neighbour_idx!=std::get<0>(bisector))
					next_bisectors.push_back(std::make_pair(std::get<0>(bisector), new_neighbour_idx));
			}

			for(std::size_t new_neighbour_idx :
				voro_graph.GetNeighbours(std::get<1>(bisector)))
			{
				if(new_neighbour_idx < voro_vertices.size() && new_neighbour_idx!=std::get<1>(bisector))
					next_bisectors.push_back(std::make_pair(new_neighbour_idx, std::get<1>(bisector)));
			}
		}

		auto [neighbour_param, neighbour_dist, neighbour_bisector_type, neighbour_pt_on_segment] =
			FindClosestPointOnBisector(std::get<0>(bisector), std::get<1>(bisector), vert);
		bool neighbour_collides = DoesDirectPathCollidePixel(vert, neighbour_pt_on_segment, use_min_dist);

		if(neighbour_bisector_type == -1 || neighbour_collides)
			continue;

		bool old_parameters_in_range = (min_param >= 0. && min_param <= 1.);
		bool new_parameters_in_range = (neighbour_param >= 0. && neighbour_param <= 1.);
		bool neighbour_closer = (neighbour_dist < min_dist);

		// choose a new position on the adjacent edge if it's either
		// closer or if the former parameters had been out of bounds
		// and are now within [0, 1] or if the old path collides
		if( ((!old_parameters_in_range && !new_parameters_in_range && neighbour_closer) ||
			(!old_parameters_in_range && new_parameters_in_range) ||
			(new_parameters_in_range && neighbour_closer)) || collides)
		{
			min_dist = neighbour_dist;
			min_param = neighbour_param;
			min_bisector = bisector;
			collides = neighbour_collides;
			bisector_type = neighbour_bisector_type;

			//std::cout << "neighbour bisector " << std::get<0>(bisector) << " " << std::get<1>(bisector)
			//	<< ", collides: " << std::boolalpha << neighbour_collides
			//	<< ", dist: " << neighbour_dist
			//	<< ", vert: " << vert[0] << " " << vert[1]
			//	<< ", point on bisector: " << neighbour_pt_on_segment[0] << " " << neighbour_pt_on_segment[1]
			//	<< std::endl;
		}
	}
	//std::cout << std::endl;

	min_param = tl2::clamp<t_real>(min_param, 0., 1.);
	return std::make_tuple(min_param, min_bisector, bisector_type, collides);
}


/**
 * get the angular distance of a vertex to the nearest wall
 * @arg vertex in pixel coordinates
 * @return angular distance in rad
 */
t_real PathsBuilder::GetDistToNearestWall(const t_vec2& vertex) const
{
	// get the wall vertices that are closest to the given vertex
	if(auto nearest_walls = m_wallsindextree.Query(vertex, 1); nearest_walls.size() >= 1)
	{
		// get angular distance to wall
		t_vec2 angle = PixelToAngle(vertex, false, false);
		t_vec2 nearest_wall_angle = PixelToAngle(nearest_walls[0], false, false);
		t_real dist = GetPathLength(nearest_wall_angle - angle);

		return dist;
	}

	// no wall found
	return std::numeric_limits<t_real>::max();
}


/**
 * find and remove loops near the retraction points in the path
 * @arg path_vertices in deg or rad
 */
void PathsBuilder::RemovePathLoops(std::vector<t_vec2>& path_vertices, bool deg, bool reverse) const
{
	std::size_t N = path_vertices.size();
	if(N <= 2)
		return;

	// maximum angular search radius
	t_real max_radius = m_directpath_search_radius;
	if(deg)
		max_radius = max_radius / tl2::pi<t_real> * t_real(180);

	const std::size_t first_pt_idx = reverse ? N - 1 : 0;
	const std::size_t second_pt_idx = reverse ? first_pt_idx-1 : first_pt_idx+1;

	t_real min_dist_to_start = GetPathLength(
		path_vertices[second_pt_idx] -
		path_vertices[first_pt_idx]);
	std::size_t min_idx = second_pt_idx;
	bool minimum_found = false;

	// get path lengths from start point
	std::vector<t_real> dists;
	std::vector<t_real> path_pos;
	std::vector<std::size_t> path_indices;
	dists.reserve(path_vertices.size());
	path_pos.reserve(path_vertices.size());
	path_indices.reserve(path_vertices.size());

	t_real cur_path_pos = 0.;
	std::size_t iter_idx = second_pt_idx;
	while(true)
	{
		t_real dist = GetPathLength(
			path_vertices[iter_idx] -
			path_vertices[first_pt_idx]);

		path_indices.push_back(iter_idx);
		path_pos.push_back(cur_path_pos);
		dists.push_back(dist);

		if(reverse)
		{
			if(iter_idx == 0)
				break;
			--iter_idx;
		}
		else
		{
			if(iter_idx >= N-1)
				break;
			++iter_idx;
		}

		cur_path_pos += 1.;
	}

	//std::ofstream ofstr(reverse ? "dists1.dat" : "dists0.dat");
	//for(std::size_t i=0; i<dists.size(); ++i)
	//	ofstr << path_pos[i] << " " << dists[i] << "\n";
	//ofstr << std::endl;

	// find local minima in the distances
	std::vector<t_real> peaks_x, peaks_sizes, peaks_widths;
	std::vector<bool> peaks_minima;

	tl2::find_peaks(dists.size(), path_pos.data(), dists.data(), 3,
		peaks_x, peaks_sizes, peaks_widths, peaks_minima,
		256, m_eps);

	// look for the largest minimum within the search radius
	for(std::size_t peak_idx=0; peak_idx<peaks_x.size(); ++peak_idx)
	{
		// try to move to minimum distance
		if(peaks_minima[peak_idx])
		{
			std::size_t peak_min_idx = (std::size_t)peaks_x[peak_idx];
			if(peak_min_idx >= path_indices.size())
				peak_min_idx = path_indices.size()-1;

			// within the search radius?
			if(dists[peak_min_idx] <= max_radius &&
				dists[peak_min_idx] < min_dist_to_start)
			{
				min_idx = path_indices[peak_min_idx];
				if(min_idx < dists.size())
				{
					min_dist_to_start = dists[peak_min_idx];
					minimum_found = true;
				}
			}
		}

		// try to skip over maximum
		else
		{
			std::size_t peak_max_idx = (std::size_t)peaks_x[peak_idx];

			std::size_t beyond_peak_idx = peak_max_idx;
			if(!reverse /*peak_max_idx > first_pt_idx*/)
			{
				std::size_t delta = peak_max_idx-first_pt_idx;
				beyond_peak_idx = first_pt_idx + 2*delta;
			}
			else
			{
				std::size_t delta = first_pt_idx-peak_max_idx;

				if(2*delta <= first_pt_idx)
					beyond_peak_idx = first_pt_idx - 2*delta;
				else
					beyond_peak_idx = 0;
			}

			if(beyond_peak_idx >= path_indices.size())
				beyond_peak_idx = path_indices.size()-1;

			// within the search radius?
			if(dists[beyond_peak_idx] <= max_radius &&
				dists[beyond_peak_idx] < min_dist_to_start)
			{
				min_idx = path_indices[beyond_peak_idx];
				if(beyond_peak_idx < dists.size())
				{
					min_dist_to_start = dists[beyond_peak_idx];
					minimum_found = true;
				}
			}
		}
	}

	if(minimum_found &&
		!DoesDirectPathCollide(
			path_vertices[first_pt_idx],
			path_vertices[min_idx], deg))
	{
		std::size_t range_start = first_pt_idx;
		std::size_t range_end = min_idx;

		//ofstr << "# minimum index: " << min_idx << std::endl;

		if(reverse)
			std::swap(range_start, range_end);

		// a shortcut was found
		if(range_start+1 < range_end)
		{
			path_vertices.erase(
				path_vertices.begin()+range_start+1,
				path_vertices.begin()+range_end);
		}
	}
}


/**
 * check if a position leads to a collision
 */
bool PathsBuilder::DoesPositionCollide(const t_vec2& pos, bool deg) const
{
	t_vec2 pix = AngleToPixel(pos, deg, false);

	t_int x = (t_int)pix[0];
	t_int y = (t_int)pix[1];

	if(x<0 || x>=(t_int)m_img.GetWidth() || y<0 || y>=(t_int)m_img.GetHeight())
		return true;

	// TODO: test if collision happens inside epsilon-circles, not just for the pixels
	if(m_img.GetPixel(x, y) != PATHSBUILDER_PIXEL_VALUE_NOCOLLISION)
		return true;

	return false;
}


/**
 * check if a direct path between the two vertices leads to a collision
 * @arg vert1 starting angular position of the path, in deg or rad
 * @arg vert2 ending angular position of the path, in deg or rad
 */
bool PathsBuilder::DoesDirectPathCollide(const t_vec2& vert1, const t_vec2& vert2, bool deg, bool use_min_dist) const
{
	t_vec2 pix1 = AngleToPixel(vert1, deg, false);
	t_vec2 pix2 = AngleToPixel(vert2, deg, false);

	return DoesDirectPathCollidePixel(pix1, pix2, use_min_dist);
}


/**
 * check if a direct path between the two vertices leads to a collision
 * @arg vert1 starting position of the path, in pixels
 * @arg vert2 ending position of the path, in pixels
 */
bool PathsBuilder::DoesDirectPathCollidePixel(const t_vec2& vert1, const t_vec2& vert2, bool use_min_dist) const
{
	t_int last_x = -1;
	t_int last_y = -1;

	for(t_real t=0.; t<=1.; t+=m_eps_angular)
	{
		t_int x = (t_int)std::lerp(vert1[0], vert2[0], t);
		t_int y = (t_int)std::lerp(vert1[1], vert2[1], t);

		// don't check the same pixel again
		if(last_x == x && last_y == y)
			continue;

		if(x<0 || x>=(t_int)m_img.GetWidth() || y<0 || y>=(t_int)m_img.GetHeight())
			return true;

		// TODO: test if collision happens inside epsilon-circles, not just for the pixels
		if(m_img.GetPixel(x, y) != PATHSBUILDER_PIXEL_VALUE_NOCOLLISION)
			return true;

		if(use_min_dist)
		{
			// look for the closest wall
			t_vec2 pix = tl2::create<t_vec2>({t_real(x), t_real(y)});
			t_real dist_to_walls = GetDistToNearestWall(pix);

			// reject path if the minimum distance to the walls is undercut
			if(dist_to_walls < m_min_angular_dist_to_walls)
				return true;
		}

		last_x = x;
		last_y = y;
	}

	return false;
}
// ----------------------------------------------------------------------------
