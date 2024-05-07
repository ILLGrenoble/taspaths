/**
 * geometric calculations, line segment intersections
 * @author Tobias Weber <tweber@ill.fr>
 * @date October/November 2020, April 2021
 * @note Forked on 19-apr-2021 from my privately developed "geo" project (https://github.com/t-weber/geo).
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

#ifndef __GEO_ALGOS_LINES_H__
#define __GEO_ALGOS_LINES_H__

#include <vector>
#include <queue>
#include <tuple>
#include <algorithm>
#include <limits>
#include <iostream>

#include <boost/math/quaternion.hpp>

#include "graphs.h"
#include "trees.h"
//#include "hull.h"
#include "tlibs2/libs/maths.h"


namespace geo {

// from hull.h
template<class t_vec> requires tl2::is_vec<t_vec>
std::tuple<bool, std::size_t, std::size_t> is_vert_in_hull(
	const std::vector<t_vec>& hull,
	const t_vec& newvert,
	const t_vec *vert_in_hull = nullptr,
	bool check_vert_segment = true);



// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

/**
 * calculate circumcentre
 * @see https://de.wikipedia.org/wiki/Umkreis
 */
template<class t_vec> requires tl2::is_vec<t_vec>
t_vec calc_circumcentre(const std::vector<t_vec>& triag)
{
	using namespace tl2_ops;
	using t_real = typename t_vec::value_type;

	if(triag.size() < 3)
		return t_vec{};

	const t_vec& v0 = triag[0];
	const t_vec& v1 = triag[1];
	const t_vec& v2 = triag[2];

	// formula, see: https://de.wikipedia.org/wiki/Umkreis
	const t_real x =
		(v0[0]*v0[0]+v0[1]*v0[1]) * (v1[1]-v2[1]) +
		(v1[0]*v1[0]+v1[1]*v1[1]) * (v2[1]-v0[1]) +
		(v2[0]*v2[0]+v2[1]*v2[1]) * (v0[1]-v1[1]);

	const t_real y =
		(v0[0]*v0[0]+v0[1]*v0[1]) * (v2[0]-v1[0]) +
		(v1[0]*v1[0]+v1[1]*v1[1]) * (v0[0]-v2[0]) +
		(v2[0]*v2[0]+v2[1]*v2[1]) * (v1[0]-v0[0]);

	const t_real n =
		t_real{2}*v0[0] * (v1[1]-v2[1]) +
		t_real{2}*v1[0] * (v2[1]-v0[1]) +
		t_real{2}*v2[0] * (v0[1]-v1[1]);

	return tl2::create<t_vec>({x/n, y/n});
}


/**
 * angle of a line
 */
template<class t_vec, class t_real = typename t_vec::value_type>
t_real line_angle(const t_vec& pt1, const t_vec& pt2)
requires tl2::is_vec<t_vec>
{
	t_vec dir = pt2 - pt1;
	t_real angle = std::atan2(t_real(dir[1]), t_real(dir[0]));
	//std::cout << "angle of [" << dir[0] << " " << dir[1] << "]: " << angle/tl2::pi<t_real>*180. << std::endl;
	return angle;
}


/**
 * angle between two lines
 */
template<class t_vec, class t_real = typename t_vec::value_type>
t_real line_angle(const t_vec& line1vert1, const t_vec& line1vert2,
	const t_vec& line2vert1, const t_vec& line2vert2)
requires tl2::is_vec<t_vec>
{
	return line_angle<t_vec, t_real>(line2vert1, line2vert2)
		- line_angle<t_vec, t_real>(line1vert1, line1vert2);
}


/**
 * output a line
 */
template<class t_vec, class t_line = std::pair<t_vec, t_vec>>
requires tl2::is_vec<t_vec>
std::ostream& print_line(std::ostream& ostr, const t_line& line)
{
	const auto& pt0 = std::get<0>(line);
	const auto& pt1 = std::get<1>(line);

	ostr << "(";
	tl2_ops::operator<<(ostr, pt0) << "), (";
	tl2_ops::operator<<(ostr, pt1) << ")";
	return ostr;
};


/**
 * returns > 0 if point is on the left-hand side of line
 */
template<class t_vec, class t_real = typename t_vec::value_type>
t_real side_of_line(const t_vec& vec1a, const t_vec& vec1b, const t_vec& pt)
requires tl2::is_vec<t_vec>
{
	using namespace tl2_ops;

	t_vec dir1 = vec1b - vec1a;
	t_vec dir2 = pt - vec1a;

	return dir1[0]*dir2[1] - dir1[1]*dir2[0];
}


/**
 * checks if two line segments intersect and calculates the intersection point
 */
template<class t_vec, class t_real = typename t_vec::value_type>
requires tl2::is_vec<t_vec>
std::pair<bool, t_vec> intersect_lines(
	const t_vec& pos1a, const t_vec& pos1b,
	const t_vec& pos2a, const t_vec& pos2b,
	bool only_segments = true,
	t_real eps = std::numeric_limits<t_real>::epsilon(),
	bool eps_ranges = false, bool check = true)
{
	// first check if line segment bounding boxes intersect
	if(only_segments)
	{
		auto bb1 = tl2::bounding_box<t_vec, std::vector>({pos1a, pos1b});
		auto bb2 = tl2::bounding_box<t_vec, std::vector>({pos2a, pos2b});
		if(!tl2::collide_bounding_boxes<t_vec>(bb1, bb2))
			return std::make_pair(false, tl2::create<t_vec>({}));
	}

	// check for line intersections
	t_vec dir1 = pos1b - pos1a;
	t_vec dir2 = pos2b - pos2a;

	auto[pt1, pt2, valid, dist, param1, param2] =
		tl2::intersect_line_line<t_vec, t_real>(pos1a, dir1, pos2a, dir2, eps);

	if(!valid)
		return std::make_pair(false, tl2::create<t_vec>({}));

	// include epsilon in parameter range check?
	t_real minparam = eps_ranges ? -eps : t_real(0);
	t_real maxparam = eps_ranges ? t_real(1)+eps : t_real(1);

	if(only_segments && (param1<minparam || param1>=maxparam || param2<minparam || param2>=maxparam))
		return std::make_pair(false, tl2::create<t_vec>({}));

	if(check)
	{
		// check if the intersection points on the two lines are the same
		// to rule out numeric instability
		bool intersects = tl2::equals<t_vec>(pt1, pt2, eps);
		return std::make_pair(intersects, pt1);
	}

	return std::make_pair(true, pt1);
}


/**
 * only check if two 2d lines intersect without calculating the point of intersection
 */
template<class t_vec, class t_real = typename t_vec::value_type>
bool intersect_lines_check(const t_vec& line1a, const t_vec& line1b,
	const t_vec& line2a, const t_vec& line2b, t_real eps_range = 0.)
requires tl2::is_vec<t_vec>
{
	bool on_lhs_1 = (side_of_line<t_vec, t_real>(line1a, line1b, line2a) >= eps_range);
	bool on_lhs_2 = (side_of_line<t_vec, t_real>(line1a, line1b, line2b) >= eps_range);

	// both points of line2 on the same side of line1?
	if(on_lhs_1 == on_lhs_2)
		return false;

	on_lhs_1 = (side_of_line<t_vec, t_real>(line2a, line2b, line1a) >= eps_range);
	on_lhs_2 = (side_of_line<t_vec, t_real>(line2a, line2b, line1b) >= eps_range);

	// both points of line1 on the same side of line2?
	if(on_lhs_1 == on_lhs_2)
		return false;

	return true;
}


/**
 * only check if two 2d lines intersect without calculating the point of intersection
 */
template<class t_vec, class t_line = std::pair<t_vec, t_vec>, class t_real = typename t_vec::value_type>
bool intersect_lines_check(const t_line& line1, const t_line& line2, t_real eps_range = 0.)
requires tl2::is_vec<t_vec>
{
	return intersect_lines_check<t_vec, t_real>(
		std::get<0>(line1), std::get<1>(line1),
		std::get<0>(line2), std::get<1>(line2), eps_range);
}


/**
 * intersection of a line and polygon line segments
 */
template<class t_vec, class t_real=typename t_vec::value_type,
	template<class...> class t_cont=std::vector>
t_cont<t_vec> intersect_line_polylines(
	const t_vec& linePt1, const t_vec& linePt2,
	const t_cont<t_vec>& poly, bool only_segment = false,
	t_real eps = std::numeric_limits<t_real>::epsilon())
requires tl2::is_vec<t_vec>
{
	t_cont<t_vec> inters;
	inters.reserve(poly.size());

	for(std::size_t idx=0; idx<poly.size(); ++idx)
	{
		std::size_t idx2 = (idx+1) % poly.size();
		const t_vec& pt1 = poly[idx];
		const t_vec& pt2 = poly[idx2];

		if(auto [has_inters, inters_pt] =
			intersect_lines<t_vec>(linePt1, linePt2, pt1, pt2, only_segment, eps, true, true);
			has_inters)
		{
			inters.emplace_back(std::move(inters_pt));
		}
	}

	// sort intersections by x
	std::sort(inters.begin(), inters.end(),
		[](const t_vec& vec1, const t_vec& vec2) -> bool
		{
			return vec1[0] < vec2[0];
		});
	return inters;
}


/**
 * intersection of a circle and polygon line segments
 */
template<class t_vec, template<class...> class t_cont=std::vector>
t_cont<t_vec> intersect_circle_polylines(
	const t_vec& circleOrg, typename t_vec::value_type circleRad,
	const t_cont<t_vec>& poly, bool only_segment = true)
requires tl2::is_vec<t_vec>
{
	t_cont<t_vec> inters;
	inters.reserve(poly.size());

	for(std::size_t idx=0; idx<poly.size(); ++idx)
	{
		std::size_t idx2 = (idx+1) % poly.size();
		const t_vec& pt1 = poly[idx];
		const t_vec& pt2 = poly[idx2];

		auto theinters = tl2::intersect_line_sphere<t_vec>(
			pt1, pt2-pt1, circleOrg, circleRad, false, only_segment);

		for(const t_vec& vec : theinters)
			inters.emplace_back(std::move(vec));
	}

	// sort intersections by x
	std::sort(inters.begin(), inters.end(),
		[](const t_vec& vec1, const t_vec& vec2) -> bool
		{
			return vec1[0] < vec2[0];
		});

	return inters;
}


/**
 * get the slope and y axis offset of a line
 */
template<class t_vec, class t_line=std::pair<t_vec, t_vec>,
	class t_real=typename t_vec::value_type>
requires tl2::is_vec<t_vec>
std::pair<t_real, t_real> get_line_slope_offs(const t_line& line)
{
	const t_vec& pt1 = std::get<0>(line);
	const t_vec& pt2 = std::get<1>(line);

	if(pt1.size() < 2 || pt2.size() < 2)
		return std::make_pair(0, 0);

	t_real slope = (pt2[1] - pt1[1]) / (pt2[0] - pt1[0]);
	t_real offs = pt1[1] - pt1[0]*slope;

	return std::make_pair(slope, offs);
}


/**
 * get the y coordinate component of a line
 */
template<class t_vec, class t_line=std::pair<t_vec, t_vec>,
	class t_real=typename t_vec::value_type>
requires tl2::is_vec<t_vec>
t_real get_line_y(const t_line& line, t_real x)
{
	auto [slope, offs] = get_line_slope_offs<t_vec>(line);
	return slope*x + offs;
}


/**
 * are two lines equal?
 */
template<class t_vec, class t_line=std::pair<t_vec, t_vec>,
	class t_real=typename t_vec::value_type>
requires tl2::is_vec<t_vec>
bool is_line_equal(const t_line& line1, const t_line& line2,
	t_real eps=std::numeric_limits<t_real>::epsilon())
{
	auto [slope1, offs1] = get_line_slope_offs<t_vec>(line1);
	auto [slope2, offs2] = get_line_slope_offs<t_vec>(line2);

	return tl2::equals<t_real>(slope1, slope2, eps) && tl2::equals<t_real>(offs1, offs2, eps);
}


/**
 * do two lines intersect?
 */
template<class t_line, class t_vec=typename std::tuple_element<0, t_line>::type,
	class t_real = typename t_vec::value_type>
std::tuple<bool, t_vec>
intersect_lines(const t_line& line1, const t_line& line2,
	t_real eps = std::numeric_limits<t_real>::epsilon(),
	bool eps_ranges = false, bool check = true)
requires tl2::is_vec<t_vec>
{
	return intersect_lines<t_vec>(
		std::get<0>(line1), std::get<1>(line1),
		std::get<0>(line2), std::get<1>(line2),
		true, eps, eps_ranges, check);
}


/**
 * check if the given point is inside the hull
 */
/*template<class t_vec> requires tl2::is_vec<t_vec>
bool pt_inside_hull(const std::vector<t_vec>& hull, const t_vec& pt)
{
	// iterate vertices
	for(std::size_t idx1=0; idx1<hull.size(); ++idx1)
	{
		std::size_t idx2 = (idx1+1) % hull.size();

		const t_vec& vert1 = hull[idx1];
		const t_vec& vert2 = hull[idx2];

		// outside?
		if(side_of_line<t_vec>(vert1, vert2, pt) < 0.)
			return false;
	}

	return true;
}*/


/**
 * get barycentric coordinates of a point
 * @see https://en.wikipedia.org/wiki/Barycentric_coordinate_system
 */
template<class t_vec> requires tl2::is_vec<t_vec>
std::optional<t_vec> get_barycentric(
	const t_vec& tri1, const t_vec& tri2, const t_vec& tri3, const t_vec& pt)
{
	using t_real = typename t_vec::value_type;
	using t_mat = tl2::mat<t_real, std::vector>;

	t_mat trafo = tl2::create<t_mat, t_vec>({tri1-tri3, tri2-tri3});
	auto [inv_trafo, ok] = tl2::inv<t_mat>(trafo);
	if(!ok)
		return std::nullopt;

	t_vec vecBary = inv_trafo * (pt-tri3);
	return vecBary;
}


/**
 * check if a point is inside a triangle
 * @see https://en.wikipedia.org/wiki/Barycentric_coordinate_system#Barycentric_coordinates_on_triangles
 */
template<class t_vec> requires tl2::is_vec<t_vec>
bool pt_inside_triag(
	const t_vec& tri1, const t_vec& tri2, const t_vec& tri3, const t_vec& pt)
{
	using t_real = typename t_vec::value_type;
	auto vecBary = get_barycentric<t_vec>(tri1, tri2, tri3, pt);
	if(!vecBary)
		return false;

	t_real x = (*vecBary)[0];
	t_real y = (*vecBary)[1];
	t_real z = t_real{1} - x - y;

	return x>=t_real{0} && x<t_real{1} &&
		y>=t_real{0} && y<t_real{1} &&
		z>=t_real{0} && z<t_real{1};
}


/**
 * tests if a point is inside a polygon using the raycasting algo:
 * @see https://en.wikipedia.org/wiki/Point_in_polygon#Ray_casting_algorithm
 */
template<class t_vec, class t_line = std::tuple<t_vec, t_vec>,
	class t_real = typename t_vec::value_type>
requires tl2::is_vec<t_vec>
bool pt_inside_poly(
	const std::vector<t_vec>& poly, const t_vec& pt,
	const t_vec* pt_outside = nullptr, t_real eps = 1e-6)
{
	// check if the point coincides with one of the polygon vertices
	for(const auto& vert : poly)
	{
		if(tl2::equals<t_vec>(vert, pt, eps))
			return false;
	}


	// check if the point is inside the polygon bounding box
	auto bbox = tl2::bounding_box<t_vec>(poly);
	if(!tl2::in_bounding_box<t_vec>(pt, bbox))
		return false;


	// some point outside the polygon
	t_vec pt2 = pt_outside ? *pt_outside : tl2::zero<t_vec>(pt.size());

	// choose a point outside the polygon if none is given
	if(!pt_outside)
	{
		for(const t_vec& vec : poly)
		{
			pt2[0] = std::abs(std::max(vec[0], pt2[0]));
			pt2[1] = std::abs(std::max(vec[1], pt2[1]));
		}

		// some arbitrary scales
		pt2[0] *= t_real{4};
		pt2[1] *= t_real{2};
	}

	// TODO: several runs with different line slopes, as the line can hit a vertex within epsilon
	t_line line(pt, pt2);
	//print_line<t_vec, t_line>(std::cout, line); std::cout << std::endl;


	// number of intersections
	std::size_t num_inters = 0;

	// check intersection with polygon line segments
	for(std::size_t vert1=0; vert1<poly.size(); ++vert1)
	{
		std::size_t vert2 = (vert1 + 1) % poly.size();

		const t_vec& vec1 = poly[vert1];
		const t_vec& vec2 = poly[vert2];

		t_line polyline(vec1, vec2);

		if(auto [intersects, inters_pt] =
			intersect_lines<t_line>(line, polyline, eps, false, false); intersects)
		{
			++num_inters;
		}
	}

	// odd number of intersections?
	bool odd = (num_inters % 2);
	return odd;
}


/**
 * tests if a point is inside a polygon using the raycasting algo:
 * @see https://en.wikipedia.org/wiki/Point_in_polygon#Ray_casting_algorithm
 */
template<class t_vec, class t_line = std::tuple<t_vec, t_vec>,
	class t_real = typename t_vec::value_type>
requires tl2::is_vec<t_vec>
bool pt_inside_poly(
	const std::vector<t_line>& polylines, const t_vec& pt,
	std::size_t lineidx_begin = 0, std::size_t lineidx_end = 0,
	const t_vec* pt_outside = nullptr, t_real eps = 1e-6)
{
	// bounding box
	t_vec bboxmin, bboxmax;
	bboxmin = tl2::create<t_vec>(2);
	bboxmin[0] = bboxmin[1] = std::numeric_limits<t_real>::max();
	bboxmax = -bboxmin;


	// some point outside the polygon
	t_vec pt2 = pt_outside ? *pt_outside : tl2::zero<t_vec>(pt.size());

	for(const auto& pair : polylines)
	{
		const t_vec& vec1 = std::get<0>(pair);
		const t_vec& vec2 = std::get<1>(pair);

		//std::tie(bboxmin, bboxmax) = tl2::bounding_box<t_vec>({vec1, vec2}, &bboxmin, &bboxmax);
		bboxmin[0] = std::min(bboxmin[0], vec1[0]);
		bboxmax[0] = std::max(bboxmax[0], vec1[0]);
		bboxmin[1] = std::min(bboxmin[1], vec1[1]);
		bboxmax[1] = std::max(bboxmax[1], vec1[1]);
		bboxmin[0] = std::min(bboxmin[0], vec2[0]);
		bboxmax[0] = std::max(bboxmax[0], vec2[0]);
		bboxmin[1] = std::min(bboxmin[1], vec2[1]);
		bboxmax[1] = std::max(bboxmax[1], vec2[1]);

		// choose a point outside the polygon if none is given
		if(!pt_outside)
		{
			pt2[0] = std::abs(std::max(vec1[0], pt2[0]));
			pt2[1] = std::abs(std::max(vec1[1], pt2[1]));
		}
	}


	// check if the point is inside the polygon bounding box
	auto bbox = std::make_tuple(std::move(bboxmin), std::move(bboxmax));
	if(!tl2::in_bounding_box<t_vec>(pt, bbox))
		return false;


	// some arbitrary scales
	if(!pt_outside)
	{
		pt2[0] *= t_real{4};
		pt2[1] *= t_real{2};
	}

	// TODO: several runs with different line slopes, as the line can hit a vertex within epsilon
	t_line line(pt, pt2);
	//print_line<t_vec, t_line>(std::cout, line); std::cout << std::endl;


	// if the given line indices are invalid, test all line segments
	if(lineidx_end <= lineidx_begin)
		lineidx_end = polylines.size();

	// number of intersections
	std::size_t num_inters = 0;

	// check intersection with polygon line segments
	for(std::size_t lineidx = lineidx_begin; lineidx < lineidx_end; ++lineidx)
	{
		const t_line& polyline = polylines[lineidx];

		if(auto [intersects, inters_pt] =
			intersect_lines<t_line>(line, polyline, eps, false, false); intersects)
		{
			++num_inters;
		}
	}

	// odd number of intersections?
	bool odd = (num_inters % 2);
	return odd;
}


/**
 * get triangle containing point pt
 */
template<class t_vec> requires tl2::is_vec<t_vec>
std::optional<std::size_t> get_containing_triag(
	const std::vector<std::vector<t_vec>>& triags, const t_vec& pt)
{
	for(std::size_t idx=0; idx<triags.size(); ++idx)
	{
		const auto& triag = triags[idx];
		if(pt_inside_triag<t_vec>(triag[0], triag[1], triag[2], pt))
			return idx;
	}

	return std::nullopt;
}


/**
 * remove duplicate points
 */
template<class t_vec, class t_real = typename t_vec::value_type>
std::vector<t_vec> _remove_duplicates(
	const std::vector<t_vec>& _verts,
	t_real eps=std::numeric_limits<t_real>::epsilon())
requires tl2::is_vec<t_vec>
{
	std::vector<t_vec> verts = _verts;

	verts.erase(std::unique(verts.begin(), verts.end(),
		[eps](const t_vec& vec1, const t_vec& vec2) -> bool
		{ return tl2::equals<t_vec>(vec1, vec2, eps); }
		), verts.end());

	return verts;
}


/**
 * sort vertices by their x and y coordinate components
 */
template<class t_vec, class t_real = typename t_vec::value_type>
std::vector<t_vec> _sort_vertices(
	const std::vector<t_vec>& _verts,
	t_real eps=std::numeric_limits<t_real>::epsilon())
requires tl2::is_vec<t_vec>
{
	std::vector<t_vec> verts = _verts;

	std::stable_sort(verts.begin(), verts.end(),
		[eps](const t_vec& vert1, const t_vec& vert2) -> bool
		{
			if(tl2::equals<t_real>(vert1[0], vert2[0], eps))
				return vert1[1] < vert2[1];
			return vert1[0] < vert2[0];
		});

	// remove unnecessary points
	auto iterCurX = verts.begin();
	for(auto iter = iterCurX; iter != verts.end(); std::advance(iter, 1))
	{
		// next x?
		if(!tl2::equals<t_real>((*iter)[0], (*iterCurX)[0], eps))
		{
			std::size_t num_same_x = iter - iterCurX;
			if(num_same_x >= 3)
				iter = verts.erase(std::next(iterCurX, 1), std::prev(iter, 1));
			iterCurX = iter;
		}
	}

	return verts;
}


/**
 * sort vertices by angle
 */
template<class t_vec,
	class t_real = typename t_vec::value_type,
	class t_vec_real = tl2::vec<t_real, std::vector>,
	class t_quat = boost::math::quaternion<t_real>>
std::tuple<std::vector<t_vec>, t_vec_real>
sort_vertices_by_angle(const std::vector<t_vec>& _verts)
requires tl2::is_vec<t_vec> && tl2::is_quat<t_quat>
{
	if(_verts.size() == 0)
		return std::make_tuple(_verts, t_vec());

	std::vector<t_vec> verts = _verts;
	std::size_t dim = verts[0].size();

	// sort by angle
	t_vec_real mean = std::accumulate(verts.begin(), verts.end(), tl2::zero<t_vec>(dim));
	if(_verts.size() < 2)
		return std::make_tuple(verts, mean);
	mean /= t_real(verts.size());

	t_quat rot001 = tl2::unit_quat<t_quat>();
	bool rot_to_001 = (dim == 3 && verts.size() >= 3);
	if(rot_to_001)
	{
		t_vec norm = tl2::cross(verts[2] - verts[0], verts[1] - verts[0]);
		//norm /= tl2::norm<t_vec>(norm);

		// rotate the vertices so that their normal points to [001]
		t_vec dir001 = tl2::create<t_vec>({ 0, 0, 1 });
		rot001 = tl2::rotation_quat<t_vec, t_quat>(norm, dir001);

		for(t_vec& vert : verts)
			vert = tl2::quat_vec_prod<t_quat, t_vec>(rot001, vert);
	}

	std::stable_sort(verts.begin(), verts.end(),
		[&mean](const t_vec& vec1, const t_vec& vec2)->bool
		{
			return line_angle<t_vec_real, t_real>(mean, vec1)
				< line_angle<t_vec_real, t_real>(mean, vec2);
		});

	// rotate back
	if(rot_to_001)
	{
		t_quat invrot001 = tl2::inv<t_quat>(rot001);
		for(t_vec& vert : verts)
			vert = tl2::quat_vec_prod<t_quat, t_vec>(invrot001, vert);
	}

	return std::make_tuple(verts, mean);
}


/**
 * test if point is on the circle border
 */
template<class t_vec, class t_real = typename t_vec::value_type>
requires tl2::is_vec<t_vec>
bool is_on_circle(
	const t_vec& org, t_real rad,
	const t_vec& pos,
	t_real eps = std::numeric_limits<typename t_vec::value_type>::epsilon())
{
	t_real val = tl2::inner<t_vec>(org-pos, org-pos);
	return tl2::equals<t_real>(val, rad*rad, eps);
};


/**
 * intersection of two circles
 * <x-mid_{1,2} | x-mid_{1,2}> = r_{1,2}^2
 *
 * circle 1:
 * trafo to mid_1 = (0,0)
 * x^2 + y^2 = r_1^2  ->  y = +-sqrt(r_1^2 - x^2)
 *
 * circle 2:
 * (x-m_1)^2 + (y-m_2)^2 = r_2^2
 * (x-m_1)^2 + (+-sqrt(r_1^2 - x^2)-m_2)^2 = r_2^2
 *
 * @see https://mathworld.wolfram.com/Circle-CircleIntersection.html
 */
template<class t_vec, template<class...> class t_cont = std::vector,
	class t_real = typename t_vec::value_type>
requires tl2::is_vec<t_vec>
t_cont<t_vec> intersect_circle_circle(
	const t_vec& org1, t_real r1,
	const t_vec& org2, t_real r2,
	t_real eps = std::sqrt(std::numeric_limits<t_real>::epsilon()))
{
	using T = t_real;

	T m1 = org2[0] - org1[0];
	T m2 = org2[1] - org1[1];

	T r1_2 = r1*r1;
	T r2_2 = r2*r2;
	T m1_2 = m1*m1;
	T m2_2 = m2*m2;
	T m2_4 = m2_2*m2_2;

	T rt = T(2)*m2_2 * (r1_2*r2_2 + m1_2*(r1_2 + r2_2) + m2_2*(r1_2 + r2_2))
		- m2_2 * (r1_2*r1_2 + r2_2*r2_2)
		- (T(2)*m1_2*m2_4 + m1_2*m1_2*m2_2 + m2_4*m2_2);

	t_cont<t_vec> inters;
	if(rt < T(0))
		return inters;

	rt = std::sqrt(rt);
	T factors = m1*(r1_2 - r2_2) + m1*m1_2 + m1*m2_2;
	T div = T(2)*(m1_2 + m2_2);

	// first intersection
	T x1 = (factors - rt) / div;
	T y1a = std::sqrt(r1_2 - x1*x1);
	T y1b = -std::sqrt(r1_2 - x1*x1);

	t_vec pos1a = tl2::create<t_vec>({x1, y1a}) + org1;
	t_vec pos1b = tl2::create<t_vec>({x1, y1b}) + org1;

	if(is_on_circle<t_vec>(org1, r1, pos1a, eps)
		&& is_on_circle<t_vec>(org2, r2, pos1a, eps))
	{
		inters.emplace_back(std::move(pos1a));
	}

	if(!tl2::equals<t_vec>(pos1a, pos1b, eps)
		&& is_on_circle<t_vec>(org1, r1, pos1b, eps)
		&& is_on_circle<t_vec>(org2, r2, pos1b, eps))
	{
		inters.emplace_back(std::move(pos1b));
	}

	// second intersection
	if(!tl2::equals<T>(rt, T(0), eps))
	{
		T x2 = (factors + rt) / div;
		T y2a = std::sqrt(r1_2 - x2*x2);
		T y2b = -std::sqrt(r1_2 - x2*x2);

		t_vec pos2a = tl2::create<t_vec>({x2, y2a}) + org1;
		t_vec pos2b = tl2::create<t_vec>({x2, y2b}) + org1;

		if(is_on_circle<t_vec>(org1, r1, pos2a, eps)
			&& is_on_circle<t_vec>(org2, r2, pos2a, eps))
		{
			inters.emplace_back(std::move(pos2a));
		}

		if(!tl2::equals<t_vec>(pos2a, pos2b, eps)
			&& is_on_circle<t_vec>(org1, r1, pos2b, eps)
			&& is_on_circle<t_vec>(org2, r2, pos2b, eps))
		{
			inters.emplace_back(std::move(pos2b));
		}
	}

	// sort intersections by x
	std::sort(inters.begin(), inters.end(),
		[](const t_vec& vec1, const t_vec& vec2) -> bool
		{ return vec1[0] < vec2[0]; });

	return inters;
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// line segment intersections
// @see (Klein 2005), ch. 2.3.2, pp. 64f
// @see (FUH 2020), ch. 2.3.2, pp. 69-80
// ----------------------------------------------------------------------------

/**
 * inefficient check for intersections by testing every line segment against each other
 */
template<class t_vec, class t_line = std::pair<t_vec, t_vec>, class t_real = typename t_vec::value_type>
std::vector<std::tuple<std::size_t, std::size_t, t_vec>>
intersect_ineff(const std::vector<t_line>& lines, t_real eps = 1e-6)
requires tl2::is_vec<t_vec>
{
	std::vector<std::tuple<std::size_t, std::size_t, t_vec>> intersections;
	intersections.reserve(lines.size()*lines.size()/2);

	for(std::size_t i=0; i<lines.size(); ++i)
	{
		for(std::size_t j=i+1; j<lines.size(); ++j)
		{
			const t_line& line1 = lines[i];
			const t_line& line2 = lines[j];

			if(auto [intersects, pt] = intersect_lines<t_line>(line1, line2, eps);
				intersects)
			{
				intersections.emplace_back(std::make_tuple(i, j, pt));
			}
		}
	}

	return intersections;
}


/**
 * check if line 1 is below line 2
 */
template<class t_vec, class t_line = std::pair<t_vec, t_vec>>
requires tl2::is_vec<t_vec>
bool cmp_line(const t_line& line1, const t_line& line2,
	typename t_vec::value_type x, typename t_vec::value_type eps)
{
	using t_real = typename t_vec::value_type;

	auto [slope1, offs1] = get_line_slope_offs<t_vec>(line1);
	auto [slope2, offs2] = get_line_slope_offs<t_vec>(line2);
	t_real line1_y = slope1*x + offs1;
	t_real line2_y = slope2*x + offs2;

	// equal y -> compare by slope
	if(tl2::equals<t_real>(line1_y, line2_y, eps))
		return slope1 < slope2;

	// compare by y
	return line1_y < line2_y;
}


template<class t_vec, class t_line = std::pair<t_vec, t_vec>>
requires tl2::is_vec<t_vec>
struct IntersTreeNode : public CommonTreeNode<IntersTreeNode<t_vec, t_line>>
{
	using t_real = typename t_vec::value_type;
	using t_balance = std::int64_t;

	t_balance balance{0};
	const t_real *curX{nullptr};
	const std::vector<t_line> *lines{nullptr};
	std::size_t line_idx{0};

	t_real eps = std::numeric_limits<t_real>::epsilon();


	IntersTreeNode<t_vec, t_line>&
	operator=(const IntersTreeNode<t_vec, t_line>& other)
	{
		static_cast<CommonTreeNode<IntersTreeNode<t_vec, t_line>>*>(this)->operator=(
			static_cast<const CommonTreeNode<IntersTreeNode<t_vec, t_line>>&>(other));

		balance = other.balance;
		curX = other.curX;
		lines = other.lines;
		line_idx = other.line_idx;
		eps = other.eps;

		return *this;
	}


	friend std::ostream& operator<<(std::ostream& ostr,
		const IntersTreeNode<t_vec, t_line>& e)
	{
		ostr << std::get<0>((*e.lines)[e.line_idx])
			<< ", " << std::get<1>((*e.lines)[e.line_idx]);
		return ostr;
	}

	friend bool operator<(const IntersTreeNode<t_vec, t_line>& e1,
		const IntersTreeNode<t_vec, t_line>& e2)
	{
		const t_line& line1 = (*e1.lines)[e1.line_idx];
		const t_line& line2 = (*e2.lines)[e2.line_idx];

		return cmp_line<t_vec, t_line>(line1, line2, *e1.curX, e1.eps);
	}
};


/**
 * line segment intersection via sweep
 * @returns a vector of [line_idx1, line_idx2, intersection point]
 * @see (FUH 2020), ch. 2.3.2, pp. 69-80
 */
template<class t_vec, class t_line = std::tuple<t_vec, t_vec>,
	class t_real = typename t_vec::value_type>
requires tl2::is_vec<t_vec>
std::vector<std::tuple<std::size_t, std::size_t, t_vec>> intersect_sweep(
	const std::vector<t_line>& _lines, t_real eps = 1e-6)
{
	// additional parameter in t_line tuple serves as line group identifier
	constexpr bool use_line_groups = (std::tuple_size_v<t_line> > 2);
	using t_mat = tl2::mat<t_real, std::vector>;

	// look for vertical lines
	bool has_vert_line = 0;
	t_real min_angle_to_y{std::numeric_limits<t_real>::max()};
	std::vector<t_line> lines = _lines;

	for(const t_line& line : lines)
	{
		if(tl2::equals<t_real>(std::get<0>(line)[0], std::get<1>(line)[0], eps))
		{
			has_vert_line = 1;
		}
		else
		{
			// get angles relative to y axis
			t_real angle_to_y = line_angle<t_vec, t_real>(
				std::get<0>(line), std::get<1>(line)) + tl2::pi<t_real>/t_real(2);
			angle_to_y = tl2::mod_pos<t_real>(angle_to_y, t_real(2)*tl2::pi<t_real>);

			if(angle_to_y > tl2::pi<t_real>/t_real(2))
				angle_to_y -= tl2::pi<t_real>;
			if(std::abs(angle_to_y) < std::abs(min_angle_to_y))
				min_angle_to_y = angle_to_y;
		}
	}

	// rotate all lines
	std::optional<t_mat> rotmat;
	if(has_vert_line)
	{
		rotmat = tl2::rotation_2d<t_mat>(-min_angle_to_y * t_real(0.5));

		for(t_line& line : lines)
		{
			std::get<0>(line) = *rotmat * std::get<0>(line);
			std::get<1>(line) = *rotmat * std::get<1>(line);
		}
	}


	// order line vertices by x
	for(t_line& line : lines)
	{
		if(std::get<0>(line)[0] > std::get<1>(line)[0])
			std::swap(std::get<0>(line), std::get<1>(line));
	}


	enum class SweepEventType
	{
		LEFT_VERTEX,
		RIGHT_VERTEX,
		INTERSECTION
	};


	struct SweepEvent
	{
		t_real x{};
		SweepEventType ty{SweepEventType::LEFT_VERTEX};

		std::size_t line_idx{};
		std::optional<std::size_t> lower_idx{}, upper_idx{};
		std::optional<t_vec> intersection{};

		void print(std::ostream& ostr)
		{
			std::string strty;
			if(ty == SweepEventType::LEFT_VERTEX)
				strty = "left_vertex";
			else if(ty == SweepEventType::RIGHT_VERTEX)
				strty = "right_vertex";
			else if(ty == SweepEventType::INTERSECTION)
				strty = "intersection";

			ostr << "x=" << std::setw(6) << x << ", type=" << std::setw(12)
				<< strty << ", line " << line_idx;
			if(lower_idx)
				ostr << ", lower=" << *lower_idx;
			if(upper_idx)
				ostr << ", upper=" << *upper_idx;

			if(intersection)
			{
				ostr << ", ";
				tl2_ops::operator<<<t_vec>(ostr, *intersection);
			}
		}
	};


	// events
	auto events_comp = [](const SweepEvent& evt1, const SweepEvent& evt2) -> bool
	{
		return evt1.x > evt2.x;
	};

	std::priority_queue<SweepEvent, std::vector<SweepEvent>, decltype(events_comp)>
		events(events_comp);

	for(std::size_t line_idx=0; line_idx<lines.size(); ++line_idx)
	{
		const t_line& line = lines[line_idx];

		SweepEvent evtLeft
		{
			.x = std::get<0>(line)[0],
			.ty = SweepEventType::LEFT_VERTEX,
			.line_idx = line_idx,
			.intersection = std::nullopt,
		};

		SweepEvent evtRight
		{
			.x = std::get<1>(line)[0],
			.ty = SweepEventType::RIGHT_VERTEX,
			.line_idx = line_idx,
			.intersection = std::nullopt,
		};

		// wrong order of vertices?
		if(evtLeft.x > evtRight.x)
		{
			std::swap(evtLeft.ty, evtRight.ty);

			events.emplace(std::move(evtRight));
			events.emplace(std::move(evtLeft));
		}
		else
		{
			events.emplace(std::move(evtLeft));
			events.emplace(std::move(evtRight));
		}
	}


	auto add_intersection = [&events, &lines, eps]
		(std::size_t lower_idx, std::size_t upper_idx, t_real curX) -> void
		{
			const t_line& line1 = lines[lower_idx];
			const t_line& line2 = lines[upper_idx];

			if(auto [intersects, pt] = intersect_lines<t_line>(line1, line2, eps);
				intersects && !tl2::equals<t_real>(curX, pt[0], eps))
			{
				SweepEvent evtNext;
				evtNext.x = pt[0];
				evtNext.ty = SweepEventType::INTERSECTION;
				evtNext.lower_idx = lower_idx;
				evtNext.upper_idx = upper_idx;
				evtNext.intersection = pt;
				events.emplace(std::move(evtNext));
			}
		};

	// status
	namespace intr = boost::intrusive;
	using t_node = IntersTreeNode<t_vec, t_line>;
	using t_treealgos = intr::avltree_algorithms<BinTreeNodeTraits<t_node>>;
	t_node status{};
	t_treealgos::init_header(&status);

	// results
	std::vector<std::tuple<std::size_t, std::size_t, t_vec>> intersections{};
	intersections.reserve(lines.size() * lines.size() / 2);

	t_real curX = 0.;
	while(events.size())
	{
		SweepEvent evt{std::move(events.top())};
		events.pop();
		curX = evt.x;

		/*std::cout << "Event: ";
		evt.print(std::cout);
		std::cout << std::endl;*/

		switch(evt.ty)
		{
			/*
			 * arrived at left vertex
			 */
			case SweepEventType::LEFT_VERTEX:
			{
				// activate line
				t_node *leaf = new t_node;
				leaf->curX = &curX;
				leaf->lines = &lines;
				leaf->line_idx = evt.line_idx;
				leaf->eps = eps;

				t_node* iter = t_treealgos::insert_equal(&status,
					t_treealgos::root_node(&status), leaf,
					[](const t_node* node1, const t_node* node2) -> bool
					{
						return *node1 < *node2;
					});


				t_node* status_begin = t_treealgos::begin_node(&status);
				t_node* status_end = t_treealgos::end_node(&status);

				t_node* iterPrev = (iter == status_begin ? status_end : t_treealgos::prev_node(iter));
				t_node* iterNext = (iter == status_end ? status_end : t_treealgos::next_node(iter));

				// add possible intersection events
				if(iterPrev != iter && iterPrev != status_end)
					add_intersection(iterPrev->line_idx, evt.line_idx, curX);
				if(iterNext != iter && iterNext != status_end)
					add_intersection(evt.line_idx, iterNext->line_idx, curX);

				break;
			}

			/*
			 * arrived at right vertex
			 */
			case SweepEventType::RIGHT_VERTEX:
			{
				t_node* status_begin = t_treealgos::begin_node(&status);
				t_node* status_end = t_treealgos::end_node(&status);

				// find current line
				t_node* iter = status_begin;
				for(; iter!=status_end; iter = t_treealgos::next_node(iter))
				{
					if(iter->line_idx == evt.line_idx)
						break;
				}
				if(iter == status_end)
					continue;

				t_node* iterPrev = (iter == status_begin ? status_end : t_treealgos::prev_node(iter));
				t_node* iterNext = (iter == status_end ? status_end : t_treealgos::next_node(iter));

				// inactivate current line
				t_node* cur_line_ptr = iter;
				iter = t_treealgos::erase(&status, iter);
				delete cur_line_ptr;

				// add possible intersection event
				status_begin = t_treealgos::begin_node(&status);
				status_end = t_treealgos::end_node(&status);
				if(iterPrev != iterNext && iterPrev != status_end && iterNext != status_end)
					add_intersection(iterPrev->line_idx, iterNext->line_idx, curX);

				break;
			}

			/*
			 * arrived at intersection
			 */
			case SweepEventType::INTERSECTION:
			{
				if(std::find_if(intersections.begin(), intersections.end(),
					[&evt, eps](const auto& inters) -> bool
						{ return tl2::equals<t_vec>(std::get<2>(inters), *evt.intersection, eps); })
					== intersections.end())
				{
					if constexpr(use_line_groups)
					{
						// if the lines belong to the same group, don't report the intersection
						if(std::get<2>(lines[*evt.lower_idx])
							!= std::get<2>(lines[*evt.upper_idx]))
						{
							// report an intersection
							intersections.emplace_back(
								std::make_tuple(
									*evt.lower_idx, *evt.upper_idx, *evt.intersection));
						}
					}
					else
					{
						// report an intersection
						intersections.emplace_back(
							std::make_tuple(
								*evt.lower_idx, *evt.upper_idx, *evt.intersection));
					}
				}
				else
				{
					// intersection already reported
					continue;
				}

				t_node* status_begin = t_treealgos::begin_node(&status);
				t_node* status_end = t_treealgos::end_node(&status);

				// find upper line
				t_node* iterUpper = status_begin;
				for(; iterUpper != status_end; iterUpper = t_treealgos::next_node(iterUpper))
				{
					if(iterUpper->line_idx == evt.upper_idx)
						break;
				}

				// find lower line
				t_node* iterLower = status_begin;
				for(; iterLower != status_end; iterLower = t_treealgos::next_node(iterLower))
				{
					if(iterLower->line_idx == evt.lower_idx)
						break;
				}

				if(iterUpper == status_end || iterLower == status_end)
					continue;

				if(!cmp_line<t_vec, t_line>(lines[iterLower->line_idx],
					lines[iterUpper->line_idx], curX, eps))
				{
					std::swap(iterUpper->line_idx, iterLower->line_idx);
					std::swap(iterUpper, iterLower);
				}

				t_node* iterPrev = (iterUpper == status_begin
					? status_end : t_treealgos::prev_node(iterUpper));
				t_node* iterNext = (iterLower == status_end
					? status_end : t_treealgos::next_node(iterLower));

				// add possible intersection events
				if(iterPrev != iterUpper && iterPrev != status_end && iterUpper != status_end)
					add_intersection(iterPrev->line_idx, iterUpper->line_idx, curX);
				if(iterNext != iterLower && iterNext != status_end && iterLower != status_end)
					add_intersection(iterLower->line_idx, iterNext->line_idx, curX);

				break;
			}
		}
	}


	// rotate intersection points back
	if(rotmat)
	{
		*rotmat = tl2::trans<t_mat>(*rotmat);
		for(auto& inters : intersections)
			std::get<2>(inters) = *rotmat * std::get<2>(inters);
	}

	return intersections;
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
/**
 * check circles for collision
 */
template<class t_vec> requires tl2::is_vec<t_vec>
bool collide_circle_circle(
	const t_vec& org1, typename t_vec::value_type r1,
	const t_vec& org2, typename t_vec::value_type r2)
{
	using t_real = typename t_vec::value_type;

	t_real dot = tl2::inner<t_vec>(org2-org1, org2-org1);
	return dot < (r2+r1) * (r2+r1);
}


/**
 * check for a collision between a circle and a polygon
 */
template<class t_vec, template<class...> class t_cont=std::vector>
bool collide_circle_poly(
	const t_vec& circleOrg, typename t_vec::value_type circleRad,
	const t_cont<t_vec>& poly)
requires tl2::is_vec<t_vec>
{
	using t_real = typename t_vec::value_type;

	// check for intersections
	auto vecs = intersect_circle_polylines<t_vec, t_cont>(
		circleOrg, circleRad, poly, true);

	// intersecting
	if(vecs.size())
		return true;

	// check case when the cirlce is completely inside the polygon
	if(pt_inside_poly<t_vec>(poly, circleOrg))
		return true;

	// check case when the polygon is completely inside the circle
	std::size_t num_inside = 0;
	std::size_t num_outside = 0;

	for(const t_vec& vec : poly)
	{
		t_real dot = tl2::inner<t_vec>(vec-circleOrg, vec-circleOrg);
		bool inside = dot < circleRad*circleRad;

		if(inside)
			++num_inside;
		else
			++num_outside;
	}

	if(num_outside == 0 && num_inside != 0)
		return true;

	return false;
}


/**
 * checks if a polygon is completely contained within another polygon
 */
template<class t_vec, template<class...> class t_cont = std::vector>
bool poly_inside_poly(
	const t_cont<t_vec>& polyOuter,
	const t_cont<t_vec>& polyInner)
requires tl2::is_vec<t_vec>
{
	if(!polyInner.size() || !polyOuter.size())
		return false;

	bool all_inside = std::all_of(
		polyInner.begin(), polyInner.end(),
		[&polyOuter](const t_vec& vec) -> bool
	{
		return std::get<0>(is_vert_in_hull<t_vec>(
			polyOuter, vec, nullptr, false));
	});

	return all_inside;
}



/**
 * check for a collision of two polygons using a line sweep
 */
template<class t_vec, template<class...> class t_cont = std::vector>
bool collide_poly_poly(
	const t_cont<t_vec>& poly1, const t_cont<t_vec>& poly2,
	typename t_vec::value_type eps = 1e-6)
requires tl2::is_vec<t_vec>
{
	if(!poly1.size() || !poly2.size())
		return false;

	using t_line = std::tuple<t_vec, t_vec, int>;

	/*using namespace tl2_ops;
	std::cout << "polygon 1:" << std::endl;
	for(const t_vec& vec : poly1)
		std::cout << vec << std::endl;
	std::cout << "polygon 2:" << std::endl;
	for(const t_vec& vec : poly2)
		std::cout << vec << std::endl;*/

	std::vector<t_line> lines;
	lines.reserve(poly1.size() + poly2.size());

	// add first polygon line segments
	for(std::size_t idx1=0; idx1<poly1.size(); ++idx1)
	{
		std::size_t idx2 = (idx1+1) % poly1.size();
		auto tup = std::make_tuple(poly1[idx1], poly1[idx2], 0);
		lines.emplace_back(std::move(tup));
	}

	// add second polygon line segments
	for(std::size_t idx1=0; idx1<poly2.size(); ++idx1)
	{
		std::size_t idx2 = (idx1+1) % poly2.size();
		auto tup = std::make_tuple(poly2[idx1], poly2[idx2], 1);
		lines.emplace_back(std::move(tup));
	}

	// check for intersection
	auto inters = intersect_sweep<t_vec, t_line>(lines, eps);
	if(inters.size())
		return true;

	// check cases when one object is completely contained in the other
	return poly_inside_poly<t_vec, t_cont>(poly1, poly2) ||
		poly_inside_poly<t_vec, t_cont>(poly2, poly1);
}


/**
 * check for a collision of two polygons using a simpler, but O(n^2) check
 */
template<class t_vec, template<class...> class t_cont = std::vector>
bool collide_poly_poly_simplified(
	const t_cont<t_vec>& poly1, const t_cont<t_vec>& poly2)
requires tl2::is_vec<t_vec>
{
	if(!poly1.size() || !poly2.size())
		return false;

	for(std::size_t idx1=0; idx1<poly1.size(); ++idx1)
	{
		std::size_t idx1b = (idx1+1) % poly1.size();
		const t_vec& vec1a = poly1[idx1];
		const t_vec& vec1b = poly1[idx1b];

		for(std::size_t idx2=0; idx2<poly2.size(); ++idx2)
		{
			std::size_t idx2b = (idx2+1) % poly2.size();
			const t_vec& vec2a = poly2[idx2];
			const t_vec& vec2b = poly2[idx2b];

			if(intersect_lines_check<t_vec>(vec1a, vec1b, vec2a, vec2b))
				return true;
		}
	}


	// check cases when one object is completely contained in the other
	return poly_inside_poly<t_vec, t_cont>(poly1, poly2) ||
		poly_inside_poly<t_vec, t_cont>(poly2, poly1);
}
// ----------------------------------------------------------------------------


/**
 * simplify path
 */
template<class t_vec,
class t_real = typename t_vec::value_type,
template<class...> class t_cont = std::vector>
t_cont<t_vec> simplify_path(const t_cont<t_vec>& _vertices)
requires tl2::is_vec<t_vec>
{
	if(_vertices.size() <= 2)
		return _vertices;

	t_cont<t_vec> vertices = _vertices;

	// find the vertex on the path closest to the start vertex
	const t_vec& start = *vertices.begin();
	t_real distToStart_sq = std::numeric_limits<t_real>::max();
	std::size_t idxStart = 1;
	for(std::size_t idx=1; idx<vertices.size(); ++idx)
	{
		const t_vec& vert = vertices[idx];
		t_real len_sq = tl2::inner<t_vec>(vert-start, vert-start);
		if(len_sq < distToStart_sq)
		{
			distToStart_sq = len_sq;
			idxStart = idx;
		}
	}

	// remove vertices between start and the closest vertex to it
	if(idxStart > 1)
		vertices.erase(vertices.begin()+1, vertices.begin()+idxStart);


	// find the vertex on the path closest to the end vertex
	const t_vec& end = *vertices.rbegin();
	t_real distToEnd_sq = std::numeric_limits<t_real>::max();
	std::size_t idxEnd = vertices.size()-1;
	for(std::size_t idx=vertices.size()-1; idx>0; --idx)
	{
		const t_vec& vert = vertices[idx];
		t_real len_sq = tl2::inner<t_vec>(vert-end, vert-end);
		if(len_sq < distToEnd_sq)
		{
			distToEnd_sq = len_sq;
			idxEnd = idx;
		}
	}

	// remove vertices between end and the closest vertex to it
	if(idxEnd < vertices.size()-1)
		vertices.erase(vertices.begin()+idxEnd+1, vertices.end());

	return vertices;
}


/**
 * subdivide line segments on a path
 */
template<class t_vec,
	class t_real = typename t_vec::value_type,
	template<class...> class t_cont = std::vector>
t_cont<t_vec> subdivide_lines(
	const t_cont<t_vec>& vertices, t_real dist)
requires tl2::is_vec<t_vec>
{
	t_cont<t_vec> newverts;
	newverts.reserve(vertices.size() * 25); // some guess

	for(std::size_t idx0 = 0; idx0 < vertices.size(); ++idx0)
	{
		const t_vec& vert0 = vertices[idx0];

		newverts.push_back(vert0);
		if(idx0 == vertices.size()-1)
			break;

		std::size_t idx1 = idx0 + 1;
		const t_vec& vert1 = vertices[idx1];

		t_real len = tl2::norm<t_vec>(vert1 - vert0);

		// if length is greater than requested length, subdivide
		if(len > dist)
		{
			t_real div = std::ceil(len / dist);
			for(t_real param=1./div; param<1.; param += 1./div)
			{
				t_vec vertBetween = vert0 + param*(vert1 - vert0);
				newverts.emplace_back(std::move(vertBetween));
			}
		}
	}

	return newverts;
}


/**
 * remove path vertices that are closer than the given distance
 */
template<class t_vec,
	class t_real = typename t_vec::value_type,
	template<class...> class t_cont = std::vector>
t_cont<t_vec> remove_close_vertices(
	const t_cont<t_vec>& vertices, t_real dist)
requires tl2::is_vec<t_vec>
{
	if(vertices.size() <= 2)
		return vertices;

	t_cont<t_vec> newverts;
	newverts.reserve(vertices.size());

	// first vertex
	newverts.push_back(*vertices.begin());

	const t_vec* cur_vert = &vertices[0];

	for(std::size_t idx = 1; idx < vertices.size()-1; ++idx)
	{
		// find first vertex with is further away than "dist"
		const t_vec& next_vert = vertices[idx];
		t_real len = tl2::norm<t_vec>(next_vert - *cur_vert);
		if(len >= dist)
		{
			newverts.push_back(next_vert);
			cur_vert = &next_vert;
		}
	}

	// last vertex
	newverts.push_back(*vertices.rbegin());
	return newverts;
}


/**
 * arc length of a path
 */
template<class t_vec,
class t_real = typename t_vec::value_type,
template<class...> class t_cont = std::vector>
t_real path_length(const t_cont<t_vec>& vertices)
requires tl2::is_vec<t_vec>
{
	t_real len{0};

	for(std::size_t vertidx=0; vertidx<vertices.size()-1; ++vertidx)
		len += tl2::norm(vertices[vertidx+1] - vertices[vertidx]);

	return len;
}


/**
 * visibility kernel of a polygon
 * @see (Klein 2005), ch. 4.4, pp. 195ff
 * @see (FUH 2020), ch. 3.3, pp. 141ff
 */
template<class t_vec, class t_edge = std::pair<t_vec, t_vec>,
	class t_real = typename t_vec::value_type>
std::vector<t_vec> ker_from_edges(const std::vector<t_edge>& edges,
	t_real eps = std::numeric_limits<t_real>::epsilon())
requires tl2::is_vec<t_vec>
{
	const std::size_t num_edges = edges.size();

	std::vector<t_vec> intersections;
	intersections.reserve(num_edges*num_edges / 2);

	for(std::size_t i=0; i<num_edges; ++i)
	{
		for(std::size_t j=i+1; j<num_edges; ++j)
		{
			if(auto [ok, inters] = intersect_lines<t_vec>(
				edges[i].first, edges[i].second,
				edges[j].first, edges[j].second, false, eps); ok)
			{
				intersections.emplace_back(std::move(inters));
			}
		}
	}


	std::vector<t_vec> ker;
	ker.reserve(intersections.size());

	for(const t_vec& inters : intersections)
	{
		bool in_ker = true;
		for(const t_edge& edge : edges)
		{
			if(side_of_line<t_vec>(edge.first, edge.second, inters) < -eps)
			{
				in_ker = false;
				break;
			}
		}

		if(in_ker)
			ker.push_back(inters);
	}

	std::tie(ker, std::ignore) = sort_vertices_by_angle<t_vec>(ker);
	return ker;
}


/**
 * visibility kernel of a polygon
 * (still O(n^2), TODO: only check contributing edges)
 * vertices have to be sorted in ccw order
 */
template<class t_vec, class t_real = typename t_vec::value_type>
std::vector<t_vec> calc_ker(const std::vector<t_vec>& verts,
	t_real eps = std::numeric_limits<t_real>::epsilon())
requires tl2::is_vec<t_vec>
{
	using t_edge = std::pair<t_vec, t_vec>;
	const std::size_t num_verts = verts.size();

	if(num_verts < 3)
		return std::vector<t_vec>({});

	std::vector<t_edge> edges;
	edges.reserve(num_verts);

	for(std::size_t vertidx=0; vertidx<num_verts; ++vertidx)
	{
		std::size_t vertidxNext = (vertidx+1) % num_verts;
		edges.emplace_back(std::make_pair(verts[vertidx], verts[vertidxNext]));
	}

	return ker_from_edges<t_vec, t_edge>(edges, eps);
}


/**
 * generate potentially intersecting random line segments
 */
template<class t_line, class t_vec, class t_mat,
	class t_real = typename t_vec::value_type>
std::vector<t_line> random_lines(std::size_t num_lines,
	t_real max_range = 1e4, t_real min_seg_len = 1., t_real max_seg_len = 100.,
	bool round_vec = false)
requires tl2::is_vec<t_vec> && tl2::is_mat<t_mat>
{
	std::vector<t_line> lines;
	lines.reserve(num_lines);

	for(std::size_t i=0; i<num_lines; ++i)
	{
		// create first point of the line segment
		t_real x = tl2::get_rand<t_real>(-max_range, max_range);
		t_real y = tl2::get_rand<t_real>(-max_range, max_range);
		t_vec pt1 = tl2::create<t_vec>({ x, y });
		if(round_vec)
		{
			pt1[0] = std::round(pt1[0]);
			pt1[1] = std::round(pt1[1]);
		}

		// create second point of the line segment
		t_real len = tl2::get_rand<t_real>(min_seg_len, max_seg_len);
		t_real angle = tl2::get_rand<t_real>(0., 2.*tl2::pi<t_real>);
		t_mat rot = tl2::rotation_2d<t_mat>(angle);
		t_vec pt2 = pt1 + rot * tl2::create<t_vec>({ len, 0. });
		if(round_vec)
		{
			pt2[0] = std::round(pt2[0]);
			pt2[1] = std::round(pt2[1]);
		}

		lines.emplace_back(std::make_pair(std::move(pt1), std::move(pt2)));
	}

	return lines;
}


/**
 * generate non-intersecting random line segments
 */
template<class t_line, class t_vec, class t_mat,
	class t_real = typename t_vec::value_type>
std::vector<t_line> random_nonintersecting_lines(std::size_t num_lines,
	t_real max_range = 1e4, t_real min_seg_len = 1., t_real max_seg_len = 100.,
	bool round_vec = false)
requires tl2::is_vec<t_vec> && tl2::is_mat<t_mat>
{
	std::vector<t_line> lines;

	do
	{
		lines = random_lines<t_line, t_vec, t_mat, t_real>
			(num_lines, max_range,
			min_seg_len, max_seg_len,
			round_vec);
	}
	while(intersect_sweep<t_vec, t_line, t_real>(lines).size());

	return lines;
}


}
#endif
