/**
 * voronoi diagrams / delaunay triangulation
 * @author Tobias Weber <tweber@ill.fr>
 * @date October/November 2020
 * @note Forked on 19-apr-2021 and 3-jun-2021 from my privately developed "geo" project (https://github.com/t-weber/geo).
 * @license GPLv3, see 'LICENSE' file
 *
 * References for the algorithms:
 *   - (Klein 2005) R. Klein, "Algorithmische Geometrie" (2005),
 *                  ISBN: 978-3540209560 (http://dx.doi.org/10.1007/3-540-27619-X).
 *   - (FUH 2020) R. Klein, C. Icking, "Algorithmische Geometrie" (2020),
 *                Kurs 1840, Fernuni Hagen (https://vu.fernuni-hagen.de/lvuweb/lvu/app/Kurs/1840).
 *   - (Berg 2008) M. de Berg, O. Cheong, M. van Kreveld, M. Overmars, "Computational Geometry" (2008),
 *                 ISBN: 978-3-642-09681-5 (http://dx.doi.org/10.1007/978-3-540-77974-2).
 *
 * ----------------------------------------------------------------------------
 * TAS-Paths (part of the Takin software suite)
 * Copyright (C) 2021       Tobias WEBER (Institut Laue-Langevin (ILL),
 *                          Grenoble, France).
 * "geo" project
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

#ifndef __GEO_ALGOS_VORONOI_H__
#define __GEO_ALGOS_VORONOI_H__

#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullFacet.h>
#include <libqhullcpp/QhullRidge.h>
#include <libqhullcpp/QhullFacetList.h>
#include <libqhullcpp/QhullFacetSet.h>
#include <libqhullcpp/QhullVertexSet.h>

#include <optional>
#include <cstdio>

#include <boost/math/quaternion.hpp>

#include "tlibs2/libs/maths.h"
#include "hull.h"
#include "lines.h"
#include "graphs.h"
#include "circular_iterator.h"


namespace geo {

// ----------------------------------------------------------------------------
// delaunay triangulation
// @see (Klein 2005), ch. 6, pp. 269f
// @see (FUH 2020), ch. 5.3, pp. 228-232
// ----------------------------------------------------------------------------

/**
 * delaunay triangulation and voronoi vertices
 * @returns [ voronoi vertices, triangles, neighbour triangle indices ]
 *
 * @see http://www.qhull.org/html/qh-code.htm#cpp
 * @see https://github.com/qhull/qhull/tree/master/src/libqhullcpp
 * @see https://github.com/qhull/qhull/blob/master/src/qhulltest/Qhull_test.cpp
 */
template<class t_vec, class t_quat = boost::math::quaternion<typename t_vec::value_type>>
requires tl2::is_vec<t_vec> && tl2::is_quat<t_quat>
std::tuple<std::vector<t_vec>, std::vector<std::vector<t_vec>>, std::vector<std::set<std::size_t>>>
calc_delaunay(int dim, const std::vector<t_vec>& verts,
	bool only_hull, bool triangulate = true,
	std::optional<std::size_t> onlysite_idx = std::nullopt)
{
	using namespace tl2_ops;
	namespace qh = orgQhull;

	using t_real = typename t_vec::value_type;
	using t_real_qhull = coordT;

	const t_real eps = 1e-5;
	std::vector<t_vec> voronoi;			// voronoi vertices
	std::vector<std::vector<t_vec>> triags;		// delaunay triangles
	std::vector<std::set<std::size_t>> neighbours;	// neighbour triangle indices

	try
	{
		std::vector<t_real_qhull> _verts;
		_verts.reserve(verts.size() * dim);
		for(const t_vec& vert : verts)
			for(int i=0; i<dim; ++i)
				_verts.push_back(t_real_qhull{vert[i]});

		std::ostringstream options;
		if(only_hull)
			options << "Qt";
		else
			options << "v Qu Qbb";
		if(triangulate)
			options << " QJ";
		// suppress some errors
		//options << " Pp";

		qh::Qhull qh{};

		// workaround because qhull seems to call the qh_fprintf function
		// in libqhull_r instead of the correct one in libqhullcpp
		std::FILE *ferr = std::fopen("/dev/null", "w");
		if(!ferr)
			ferr = stderr;
		qh.qh()->ferr = ferr;
		qh.setOutputStream(nullptr);
		qh.setErrorStream(nullptr);
		qh.setFactorEpsilon(eps);

		qh.runQhull("triag", dim, int(_verts.size()/dim),
			_verts.data(), options.str().c_str());
		if(ferr && ferr != stderr)
			std::fclose(ferr);

		if(qh.hasQhullMessage())
			std::cerr << qh.qhullMessage() << std::endl;


		qh::QhullFacetList facets{qh.facetList()};
		std::vector<void*> facetHandles{};
		facetHandles.reserve(facets.size());
		voronoi.reserve(facets.size());
		triags.reserve(facets.size());
		neighbours.reserve(facets.size());


		// use "voronoi" array for hull vertices, if not needed otherwise
		if(only_hull)
		{
			qh::QhullVertexList hull_vertices{qh.vertexList()};
			for(auto iterVert=hull_vertices.begin(); iterVert!=hull_vertices.end(); ++iterVert)
			{
				qh::QhullPoint pt = iterVert->point();

				t_vec vec = tl2::create<t_vec>(dim);
				for(int i=0; i<dim; ++i)
					vec[i] = t_real{pt[i]};

				voronoi.emplace_back(std::move(vec));
			}

			if(dim == 2 || dim == 3)
			{
				std::tie(voronoi, std::ignore)
					= sort_vertices_by_angle<t_vec, t_real, t_vec, t_quat>(
						voronoi);
			}
		}


		// get all triangles
		for(auto iterFacet=facets.begin(); iterFacet!=facets.end(); ++iterFacet)
		{
			if(iterFacet->isUpperDelaunay())
				continue;

			// get triangle vertices
			std::vector<t_vec> thetriag;
			qh::QhullVertexSet vertices = iterFacet->vertices();

			for(auto iterVertex=vertices.begin(); iterVertex!=vertices.end(); ++iterVertex)
			{
				qh::QhullPoint pt = (*iterVertex).point();

				t_vec vec = tl2::create<t_vec>(dim);
				for(int i=0; i<dim; ++i)
					vec[i] = t_real{pt[i]};

				thetriag.emplace_back(std::move(vec));
			}

			// limit to the voronoi region of only one site?
			if(onlysite_idx)
			{
				const t_vec& site = verts[*onlysite_idx];

				// does the delaunay triangle contain this site?
				bool found_site = false;
				for(const t_vec& vec : thetriag)
				{
					if(tl2::equals<t_vec>(site, vec, eps))
					{
						found_site = true;
						break;
					}
				}

				if(!found_site)
					continue;
			}

			// get voronoi vertices
			if(!only_hull)
			{
				qh::QhullPoint pt = iterFacet->voronoiVertex();

				t_vec vec = tl2::create<t_vec>(dim);
				for(int i=0; i<dim; ++i)
					vec[i] = t_real{pt[i]};

				voronoi.emplace_back(std::move(vec));
			}

			// sort triangle vertices
			if(dim == 2 || dim == 3)
			{
				std::tie(thetriag, std::ignore)
					= sort_vertices_by_angle<t_vec, t_real, t_vec, t_quat>(
						thetriag);
			}

			facetHandles.push_back(iterFacet->getBaseT());
			triags.emplace_back(std::move(thetriag));
		}


		// find neighbouring triangles
		if(!only_hull)
		{
			neighbours.resize(triags.size());

			std::size_t facetIdx = 0;
			for(auto iterFacet=facets.begin(); iterFacet!=facets.end(); ++iterFacet)
			{
				if(iterFacet->isUpperDelaunay())
					continue;

				qh::QhullFacetSet neighbourFacets{iterFacet->neighborFacets()};
				for(auto iterNeighbour=neighbourFacets.begin(); iterNeighbour!=neighbourFacets.end();
					++iterNeighbour)
				{
					void* handle = (*iterNeighbour).getBaseT();
					auto iterHandle = std::find(facetHandles.begin(), facetHandles.end(), handle);
					if(iterHandle != facetHandles.end())
					{
						std::size_t handleIdx = iterHandle - facetHandles.begin();
						neighbours[facetIdx].insert(handleIdx);
					}
				}

				if(++facetIdx >= triags.size())
					break;
			}
		}
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
	}

	return std::make_tuple(voronoi, triags, neighbours);
}


/**
 * @returns [triangle index, shared index 1, shared index 2, non-shared index]
 * @see (FUH 2020), ch. 6.2, pp. 269-282
 */
template<class t_vec, class t_real = typename t_vec::value_type>
std::optional<std::tuple<std::size_t, std::size_t, std::size_t, std::size_t>>
get_triag_sharing_edge(
	std::vector<std::vector<t_vec>>& triags,
	const t_vec& vert1, const t_vec& vert2,
	std::size_t curtriagidx, t_real eps = 1e-5)
requires tl2::is_vec<t_vec>
{
	for(std::size_t i=0; i<triags.size(); ++i)
	{
		if(i == curtriagidx)
			continue;

		const auto& triag = triags[i];

		// test all edge combinations
		if(tl2::equals<t_vec>(triag[0], vert1, eps) && tl2::equals<t_vec>(triag[1], vert2, eps))
			return std::make_tuple(i, 0, 1, 2);
		if(tl2::equals<t_vec>(triag[1], vert1, eps) && tl2::equals<t_vec>(triag[0], vert2, eps))
			return std::make_tuple(i, 1, 0, 2);
		if(tl2::equals<t_vec>(triag[0], vert1, eps) && tl2::equals<t_vec>(triag[2], vert2, eps))
			return std::make_tuple(i, 0, 2, 1);
		if(tl2::equals<t_vec>(triag[2], vert1, eps) && tl2::equals<t_vec>(triag[0], vert2, eps))
			return std::make_tuple(i, 2, 0, 1);
		if(tl2::equals<t_vec>(triag[1], vert1, eps) && tl2::equals<t_vec>(triag[2], vert2, eps))
			return std::make_tuple(i, 1, 2, 0);
		if(tl2::equals<t_vec>(triag[2], vert1, eps) && tl2::equals<t_vec>(triag[1], vert2, eps))
			return std::make_tuple(i, 2, 1, 0);
	}

	// no shared edge found
	return std::nullopt;
}


/**
 * does delaunay triangle conflict with point pt?
 * @see (FUH 2020), ch. 6.2, pp. 269-282
 */
template<class t_vec> requires tl2::is_vec<t_vec>
bool is_conflicting_triag(const std::vector<t_vec>& triag, const t_vec& pt)
{
	using t_real = typename t_vec::value_type;

	// circumscribed circle radius
	t_vec center = calc_circumcentre<t_vec>(triag);
	t_real rad = tl2::norm<t_vec>(triag[0] - center);
	t_real dist = tl2::norm<t_vec>(pt - center);

	// point in circumscribed circle?
	return dist < rad;
}


/**
 * flip an edge in the iterative method
 * @see (FUH 2020), ch. 6.2, pp. 269-282
 */
template<class t_vec, class t_real = typename t_vec::value_type>
void flip_edge(std::vector<std::vector<t_vec>>& triags,
	std::size_t triagidx, std::size_t nonsharedidx, t_real eps = 1e-5)
requires tl2::is_vec<t_vec>
{
	std::size_t sharedidx1 = (nonsharedidx+1) % triags[triagidx].size();
	std::size_t sharedidx2 = (nonsharedidx+2) % triags[triagidx].size();

	// get triangle on other side of shared edge
	auto optother = get_triag_sharing_edge(
		triags, triags[triagidx][sharedidx1], triags[triagidx][sharedidx2], triagidx, eps);
	if(!optother)
		return;
	const auto [othertriagidx, othersharedidx1, othersharedidx2, othernonsharedidx] = *optother;

	if(is_conflicting_triag<t_vec>(triags[othertriagidx], triags[triagidx][nonsharedidx]))
	{
		triags[triagidx] = std::vector<t_vec>
		{{
			triags[triagidx][nonsharedidx],
			triags[othertriagidx][othernonsharedidx],
			triags[othertriagidx][othersharedidx1]
		}};

		triags[othertriagidx] = std::vector<t_vec>
		{{
			triags[triagidx][nonsharedidx],
			triags[othertriagidx][othernonsharedidx],
			triags[othertriagidx][othersharedidx2]
		}};

		// also check neighbours of newly created triangles for conflicts
		flip_edge(triags, othertriagidx, othernonsharedidx, eps);
		flip_edge(triags, othertriagidx, othersharedidx1, eps);
		flip_edge(triags, othertriagidx, othersharedidx2, eps);

		flip_edge(triags, triagidx, nonsharedidx, eps);
		flip_edge(triags, triagidx, sharedidx1, eps);
		flip_edge(triags, triagidx, sharedidx2, eps);
	}
}


/**
 * iterative delaunay triangulation
 * @see (FUH 2020), ch. 6.2, pp. 269-282
 */
template<class t_vec, class t_real = typename t_vec::value_type>
std::tuple<std::vector<t_vec>, std::vector<std::vector<t_vec>>, std::vector<std::set<std::size_t>>>
calc_delaunay_iterative(const std::vector<t_vec>& verts , t_real eps = 1e-5)
requires tl2::is_vec<t_vec>
{
	using namespace tl2_ops;

	std::vector<t_vec> voronoi;			// voronoi vertices
	std::vector<std::vector<t_vec>> triags;		// delaunay triangles
	std::vector<std::set<std::size_t>> neighbours;	// neighbour triangle indices

	if(verts.size() < 3)
		return std::make_tuple(voronoi, triags, neighbours);

	// first triangle
	triags.emplace_back(std::vector<t_vec>{{ verts[0], verts[1], verts[2] }});

	// currently inserted vertices
	std::vector<t_vec> curverts;
	curverts.reserve(verts.size());

	curverts.push_back(verts[0]);
	curverts.push_back(verts[1]);
	curverts.push_back(verts[2]);

	// insert vertices iteratively
	for(std::size_t newvertidx=3; newvertidx<verts.size(); ++newvertidx)
	{
		const t_vec& newvert = verts[newvertidx];
		//std::cout << "newvert " << newvertidx-3 << ": " << newvert << std::endl;

		// find triangle containing the new vertex
		if(auto optidx = get_containing_triag<t_vec>(triags, newvert); optidx)
		{
			//std::cout << "inside" << std::endl;

			auto conttriag = std::move(triags[*optidx]);
			triags.erase(triags.begin() + *optidx);

			// new delaunay edges connecting to newvert
			triags.emplace_back(std::vector<t_vec>{{ newvert, conttriag[0], conttriag[1] }});
			triags.emplace_back(std::vector<t_vec>{{ newvert, conttriag[0], conttriag[2] }});
			triags.emplace_back(std::vector<t_vec>{{ newvert, conttriag[1], conttriag[2] }});

			flip_edge(triags, triags.size()-3, 0, eps);
			flip_edge(triags, triags.size()-2, 0, eps);
			flip_edge(triags, triags.size()-1, 0, eps);
		}

		// new vertex is outside of any triangle
		else
		{
			auto hull = calc_hull_iterative_bintree<t_vec>(curverts, eps);
			std::tie(hull, std::ignore) = sort_vertices_by_angle<t_vec>(hull);

			// find the points in the hull visible from newvert
			std::vector<t_vec> visible;
			{
				// start indices
				auto [already_in_hull, hullvertidx1, hullvertidx2] =
					is_vert_in_hull<t_vec>(hull, newvert);
				if(already_in_hull)
					continue;

				// find visible vertices like in calc_hull_iterative
				circular_wrapper circularverts(hull);
				auto iterLower = circularverts.begin() + hullvertidx1;
				auto iterUpper = circularverts.begin() + hullvertidx2;

				// correct cycles
				if(hullvertidx1 > hullvertidx2 && iterLower.GetRound()==iterUpper.GetRound())
					iterUpper.SetRound(iterLower.GetRound()+1);

				for(; iterLower.GetRound()>=-2; --iterLower)
				{
					if(side_of_line<t_vec>(*iterLower, newvert, *(iterLower-1)) >= 0.)
						break;
				}

				for(; iterUpper.GetRound()<=2; ++iterUpper)
				{
					if(side_of_line<t_vec>(*iterUpper, newvert, *(iterUpper+1)) <= 0.)
						break;
				}

				for(auto iter=iterLower; iter<=iterUpper; ++iter)
					visible.push_back(*iter);
			}

			for(std::size_t visidx=0; visidx<visible.size()-1; ++visidx)
			{
				triags.emplace_back(std::vector<t_vec>{{ newvert, visible[visidx], visible[visidx+1] }});
				flip_edge(triags, triags.size()-1, 0, eps);
			}
		}

		curverts.push_back(newvert);
	}


	// find neighbouring triangles and voronoi vertices
	neighbours.resize(triags.size());
	voronoi.reserve(triags.size());

	for(std::size_t triagidx=0; triagidx<triags.size(); ++triagidx)
	{
		// sort vertices
		auto& triag = triags[triagidx];
		std::tie(triag, std::ignore) = sort_vertices_by_angle<t_vec>(triag);

		// voronoi vertices
		voronoi.emplace_back(calc_circumcentre<t_vec>(triag));

		// neighbouring triangle indices
		auto optother1 = get_triag_sharing_edge(triags, triags[triagidx][0], triags[triagidx][1], triagidx, eps);
		auto optother2 = get_triag_sharing_edge(triags, triags[triagidx][0], triags[triagidx][2], triagidx, eps);
		auto optother3 = get_triag_sharing_edge(triags, triags[triagidx][1], triags[triagidx][2], triagidx, eps);

		if(optother1) neighbours[triagidx].insert(std::get<0>(*optother1));
		if(optother2) neighbours[triagidx].insert(std::get<0>(*optother2));
		if(optother3) neighbours[triagidx].insert(std::get<0>(*optother3));
	}


	return std::make_tuple(voronoi, triags, neighbours);
}


/**
 * delaunay triangulation using parabolic trafo
 * @see (Berg 2008), pp. 254-256 and p. 168
 * @see (FUH 2020), ch. 6.5, pp. 298-300
 *
 * @see http://www.qhull.org/html/qh-code.htm#cpp
 * @see https://github.com/qhull/qhull/tree/master/src/libqhullcpp
 * @see https://github.com/qhull/qhull/blob/master/src/qhulltest/Qhull_test.cpp
 */
template<class t_vec, class t_vec_dyn = t_vec>
requires tl2::is_vec<t_vec> && tl2::is_dyn_vec<t_vec_dyn>
std::tuple<std::vector<t_vec>, std::vector<std::vector<t_vec>>, std::vector<std::set<std::size_t>>>
calc_delaunay_parabolic(const std::vector<t_vec>& verts)
{
	using namespace tl2_ops;
	namespace qh = orgQhull;

	using t_real = typename t_vec::value_type;
	using t_real_qhull = coordT;

	const int dim = 2;

	std::vector<t_vec> voronoi;			// voronoi vertices
	std::vector<std::vector<t_vec>> triags;		// delaunay triangles
	std::vector<std::set<std::size_t>> neighbours;	// neighbour triangle indices

	try
	{
		std::vector<t_real_qhull> _verts;
		_verts.reserve(verts.size()*(dim+1));

		for(const t_vec& vert : verts)
		{
			_verts.push_back(t_real_qhull{vert[0]});
			_verts.push_back(t_real_qhull{vert[1]});
			_verts.push_back(t_real_qhull{vert[0]*vert[0] + vert[1]*vert[1]});
		}

		qh::Qhull qh{};
		// workaround because qhull seems to call the qh_fprintf function
		// in libqhull_r instead of the correct one in libqhullcpp
		std::FILE *ferr = std::fopen("/dev/null", "w");
		if(!ferr)
			ferr = stderr;
		qh.qh()->ferr = ferr;
		qh.setOutputStream(nullptr);
		qh.setErrorStream(nullptr);
		//qh.setFactorEpsilon(eps);
		qh.runQhull("triag", dim+1, int(_verts.size()/(dim+1)), _verts.data(), "Qt");
		if(ferr && ferr != stderr)
			std::fclose(ferr);

		if(qh.hasQhullMessage())
			std::cerr << qh.qhullMessage() << std::endl;


		qh::QhullFacetList facets{qh.facetList()};
		std::vector<void*> facetHandles{};

		facetHandles.reserve(facets.size());
		voronoi.reserve(facets.size());
		triags.reserve(facets.size());
		neighbours.reserve(facets.size());


		auto facetAllowed = [](auto iterFacet) -> bool
		{
			if(iterFacet->isUpperDelaunay())
				return false;

			// filter out non-visible part of hull
			qh::QhullHyperplane plane = iterFacet->hyperplane();
			t_vec_dyn normal = tl2::create<t_vec_dyn>(dim+1);
			for(int i=0; i<dim+1; ++i)
				normal[i] = t_real{plane[i]};
			// normal pointing upwards?
			if(normal[2] > 0.)
				return false;

			return true;
		};


		for(auto iterFacet=facets.begin(); iterFacet!=facets.end(); ++iterFacet)
		{
			if(!facetAllowed(iterFacet))
				continue;

			std::vector<t_vec> thetriag;
			qh::QhullVertexSet vertices = iterFacet->vertices();

			for(auto iterVertex=vertices.begin(); iterVertex!=vertices.end(); ++iterVertex)
			{
				qh::QhullPoint pt = (*iterVertex).point();

				t_vec vec = tl2::create<t_vec>(dim);
				for(int i=0; i<dim; ++i)
					vec[i] = t_real{pt[i]};

				thetriag.emplace_back(std::move(vec));
			}

			voronoi.emplace_back(calc_circumcentre<t_vec>(thetriag));
			std::tie(thetriag, std::ignore) = sort_vertices_by_angle<t_vec>(thetriag);
			triags.emplace_back(std::move(thetriag));
			facetHandles.push_back(iterFacet->getBaseT());
		}


		// find neighbouring triangles
		neighbours.resize(triags.size());

		std::size_t facetIdx = 0;
		for(auto iterFacet=facets.begin(); iterFacet!=facets.end(); ++iterFacet)
		{
			if(!facetAllowed(iterFacet))
				continue;

			qh::QhullFacetSet neighbourFacets{iterFacet->neighborFacets()};
			for(auto iterNeighbour=neighbourFacets.begin(); iterNeighbour!=neighbourFacets.end(); ++iterNeighbour)
			{
				void* handle = (*iterNeighbour).getBaseT();
				auto iterHandle = std::find(facetHandles.begin(), facetHandles.end(), handle);
				if(iterHandle != facetHandles.end())
				{
					std::size_t handleIdx = iterHandle - facetHandles.begin();
					neighbours[facetIdx].insert(handleIdx);
				}
			}

			if(++facetIdx >= triags.size())
				break;
		}
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
	}

	return std::make_tuple(voronoi, triags, neighbours);
}


/**
 * get all edges from a delaunay triangulation
 */
template<class t_vec,
	class t_edge = std::pair<std::size_t, std::size_t>,
	class t_real = typename t_vec::value_type>
std::vector<t_edge> get_edges(
	const std::vector<t_vec>& verts,
	const std::vector<std::vector<t_vec>>& triags,
	t_real eps = 1e-5)
{
	auto get_vert_idx = [&verts, eps](const t_vec& vert) -> std::optional<std::size_t>
	{
		for(std::size_t vertidx=0; vertidx<verts.size(); ++vertidx)
		{
			const t_vec& vert2 = verts[vertidx];
			if(tl2::equals<t_vec>(vert, vert2, eps))
				return vertidx;
		}

		return std::nullopt;
	};


	std::vector<t_edge> edges;
	edges.reserve(triags.size()*3*2);

	for(std::size_t vertidx=0; vertidx<verts.size(); ++vertidx)
	{
		const t_vec& vert = verts[vertidx];

		for(const auto& triag : triags)
		{
			for(std::size_t i=0; i<triag.size(); ++i)
			{
				const t_vec& triagvert = triag[i];

				if(tl2::equals<t_vec>(vert, triagvert, eps))
				{
					const t_vec& vert2 = triag[(i+1) % triag.size()];
					const t_vec& vert3 = triag[(i+2) % triag.size()];

					std::size_t vert2idx = *get_vert_idx(vert2);
					std::size_t vert3idx = *get_vert_idx(vert3);

					edges.push_back(std::make_pair(vertidx, vert2idx));
					edges.push_back(std::make_pair(vertidx, vert3idx));
				}
			}
		}
	}

	return edges;
}

}

#endif
