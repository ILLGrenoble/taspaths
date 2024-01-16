/**
 * the paths builder comprises two steps:
 *   (1) it calculates the path mesh (i.e. the roadmap) of possible instrument
 *       paths (file: PathsMeshBuilder.cpp).
 *   (2) it calculates a specific path on the path mesh from the current to
 *       the target instrument position (file: PathsBuilder.cpp)
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

#ifndef __GEO_PATHS_BUILDER_H__
#define __GEO_PATHS_BUILDER_H__

#include <vector>
#include <memory>
#include <iostream>

#include <boost/signals2/signal.hpp>

#include "src/libs/voronoi_lines.h"
#include "src/libs/voronoi.h"
#include "src/libs/img.h"
#include "src/libs/graphs.h"
#include "src/libs/hull.h"
#include "tlibs2/libs/maths.h"

#include "types.h"
#include "InstrumentSpace.h"
#include "TasCalculator.h"
#include "PathsExporter.h"


struct InstrumentPath
{
	// path mesh ok?
	bool ok = false;

	// initial and final vertices on path (in pixel coordinates)
	t_vec2 vec_i;
	t_vec2 vec_f;

	// is it a direct path from vec_i to vec_f?
	bool is_direct = false;

	// are the initial and final vertices on a linear or a quadratic bisector?
	bool is_linear_i = true;
	bool is_linear_f = true;

	// indices of the voronoi vertices on the path mesh
	std::vector<std::size_t> voronoi_indices;

	// position parameter along the entry and exit path
	t_real param_i = 0;
	t_real param_f = 1;
};


/**
 * strategy for finding the path
 */
enum class PathStrategy
{
	// find shortest path
	SHORTEST,

	// avoid paths close to walls
	PENALISE_WALLS,
};


/**
 * backend to use for contour calculation
 */
enum class ContourBackend
{
	// internal function
	INTERNAL,

	// opencv
	OCV,
};


/**
 * backend to use for voronoi diagram calculation
 */
enum class VoronoiBackend
{
	// boost.polygon
	BOOST,

	// cgal
	CGAL,
};


// pixel values for various configuration space conditions
#define PATHSBUILDER_PIXEL_VALUE_FORBIDDEN_ANGLE  0xf0
#define PATHSBUILDER_PIXEL_VALUE_COLLISION        0xff
#define PATHSBUILDER_PIXEL_VALUE_NOCOLLISION      0x00


class PathsBuilder
{
public:
	// contour point
	using t_contourvec = t_vec2_int;

	// line segment
	using t_line = std::pair<t_vec2, t_vec2>;

	// voronoi edges
	using t_voronoiedge_linear =
		std::tuple<t_line,
		std::optional<std::size_t>,
		std::optional<std::size_t>>;
	using t_voronoiedge_parabolic =
		std::tuple<std::vector<t_vec2>,
		std::size_t, std::size_t>;

	// voronoi graph
	using t_graph = geo::AdjacencyList<t_real>;
	//using t_graph = geo::AdjacencyMatrix<t_real>;


protected:
	// get path length, taking into account the motor speeds
	t_real GetPathLength(const t_vec2& vec) const;

	// check if a position (in angular coordinates) leads to a collision
	bool DoesPositionCollide(const t_vec2& pos, bool deg = false) const;

	// check if a direct path between the two vertices leads to a collision
	bool DoesDirectPathCollide(const t_vec2& vert1, const t_vec2& vert2,
		bool deg = false, bool use_min_dist = true) const;
	bool DoesDirectPathCollidePixel(const t_vec2& vert1, const t_vec2& vert2,
		bool use_min_dist = true) const;

	// get the angular distance of a vertex to the nearest wall from pixel coordinates
	t_real GetDistToNearestWall(const t_vec2& vertex) const;

	// find the closest point on a path segment
	std::tuple<t_real, t_real, int, t_vec2>
	FindClosestPointOnBisector(std::size_t idx1, std::size_t idx2, const t_vec2& vec) const;

	// find a neighbour bisector which is closer to the given vertex than the given one
	std::tuple<t_real, std::pair<std::size_t, std::size_t>, int, bool>
	FindClosestBisector(std::size_t vert_idx_1, std::size_t vert_idx_2, const t_vec& vert) const;

	// find and remove loops near the retraction points in the path
	void RemovePathLoops(std::vector<t_vec2>& path_vertices, bool deg = false, bool reverse = false) const;


public:
	PathsBuilder();
	~PathsBuilder();

	PathsBuilder(const PathsBuilder&) = default;
	PathsBuilder& operator=(const PathsBuilder&) = default;

	// ------------------------------------------------------------------------
	// input instrument
	// ------------------------------------------------------------------------
	void SetInstrumentSpace(const InstrumentSpace* instr) { m_instrspace = instr; }
	const InstrumentSpace* GetInstrumentSpace() const { return m_instrspace; }

	void SetTasCalculator(const TasCalculator* tascalc) { m_tascalc = tascalc; }
	const TasCalculator* GetTasCalculator() const { return m_tascalc; }
	// ------------------------------------------------------------------------

	// ------------------------------------------------------------------------
	// conversion functions
	// ------------------------------------------------------------------------
	t_vec2 PixelToAngle(const t_vec2& pix, bool deg = true, bool inc_sense = false) const;
	t_vec2 AngleToPixel(const t_vec2& angle, bool deg = true, bool inc_sense = false) const;
	t_vec2 PixelToAngle(t_real x, t_real y, bool deg = true, bool inc_sense = false) const;
	t_vec2 AngleToPixel(t_real x, t_real y, bool deg = true, bool inc_sense = false) const;
	// ------------------------------------------------------------------------

	// clear all data
	void Clear();

	// get contour image and wall contour points
	const geo::Image<std::uint8_t>& GetImage() const { return m_img; }
	const std::vector<std::vector<t_contourvec>>& GetWallContours(bool full = false) const;

	// get voronoi vertices, edges and graph
	const geo::VoronoiLinesResults<t_vec2, t_line, t_graph>& GetVoronoiResults() const
	{ return m_voro_results; }

	// ------------------------------------------------------------------------
	// path mesh calculation workflow
	// ------------------------------------------------------------------------
	void StartPathMeshWorkflow();
	void FinishPathMeshWorkflow(bool successful = true);

	bool CalculateConfigSpace(t_real da2, t_real da4,
		t_real starta2 = 0., t_real enda2 = tl2::pi<t_real>,
		t_real starta4 = 0., t_real enda4 = tl2::pi<t_real>);
	bool CalculateWallsIndexTree();
	bool CalculateWallContours(bool simplify = true, bool convex_split = false,
		ContourBackend backend = ContourBackend::INTERNAL);
	bool CalculateLineSegments(bool use_region_function = false);
	bool CalculateVoronoi(bool group_lines = true,
		VoronoiBackend backend = VoronoiBackend::BOOST,
		bool use_region_function = true);

	// number of line segment groups -- for scripting interface
	std::size_t GetNumberOfLineSegmentRegions() const { return m_linegroups.size(); }

	// test if the region is inverted -- for scripting interface
	bool IsRegionInverted(std::size_t groupidx) { return m_inverted_regions[groupidx]; }

	// get line segment group -- for scripting interface
	std::vector<std::array<t_real, 4>> GetLineSegmentRegionAsArray(std::size_t groupidx) const;
	// ------------------------------------------------------------------------

	// ------------------------------------------------------------------------
	// path calculation
	// ------------------------------------------------------------------------
	// find a path from an initial (a2, a4) to a final (a2, a4)
	InstrumentPath FindPath(t_real a2_i, t_real a4_i, t_real a2_f, t_real a4_f,
		PathStrategy pathstrategy = PathStrategy::SHORTEST);

	// get individual vertices on an instrument path
	std::vector<t_vec2> GetPathVertices(const InstrumentPath& path,
		bool subdivide_lines = false, bool deg = false) const;

	// get the distances to the nearest walls for each point of a given path
	std::vector<t_real> GetDistancesToNearestWall(const std::vector<t_vec2>& path, bool deg = false) const;

	// get individual vertices on an instrument path -- for scripting interface
	std::vector<std::pair<t_real, t_real>>
		GetPathVerticesAsPairs(const InstrumentPath& path,
			bool subdivide_lines = false, bool deg = false) const;
	// ------------------------------------------------------------------------

	// ------------------------------------------------------------------------
	// options
	// ------------------------------------------------------------------------
	t_real GetEpsilon() const { return m_eps; }
	void SetEpsilon(t_real eps) { m_eps = eps; }

	t_real GetAngularEpsilon() const { return m_eps_angular; }
	void SetAngularEpsilon(t_real eps) { m_eps_angular = eps; }

	t_real GetVoronoiEdgeEpsilon() const { return m_voroedge_eps; }
	void SetVoronoiEdgeEpsilon(t_real eps) { m_voroedge_eps = eps; }

	t_real GetSubdivisionLength() const { return m_subdiv_len; }
	void SetSubdivisionLength(t_real len) { m_subdiv_len = len; }

	t_real GetMinDistToWalls() const { return m_min_angular_dist_to_walls; }
	void SetMinDistToWalls(t_real dist) { m_min_angular_dist_to_walls = dist; }

	void SetRemoveBisectorsBelowMinWallDist(bool b) { m_remove_bisectors_below_min_wall_dist = b; }
	bool GetRemoveBisectorsBelowMinWallDist() const { return m_remove_bisectors_below_min_wall_dist; }

	unsigned int GetMaxNumThreads() const { return m_maxnum_threads; }
	void SetMaxNumThreads(unsigned int n) { m_maxnum_threads = n; }

	bool GetTryDirectPath() const { return m_directpath; }
	void SetTryDirectPath(bool directpath) { m_directpath = directpath; }

	t_real GetMaxDirectPathRadius() const { return m_directpath_search_radius; }
	void SetMaxDirectPathRadius(t_real dist) { m_directpath_search_radius = dist; }

	unsigned int GetNumClosestVoronoiVertices() const { return m_num_closest_voronoi_vertices; }
	void SetNumClosestVoronoiVertices(unsigned int num) { m_num_closest_voronoi_vertices = num; }

	bool GetVerifyPath() const { return m_verifypath; }
	void SetVerifyPath(bool verify) { m_verifypath = verify; }

	bool GetUseMotorSpeeds() const { return m_use_motor_speeds; }
	void SetUseMotorSpeeds(bool b) { m_use_motor_speeds = b; }
	// ------------------------------------------------------------------------

	// ------------------------------------------------------------------------
	// progress status handler
	// ------------------------------------------------------------------------
	// connection to progress signal
	template<class t_slot>
	boost::signals2::connection AddProgressSlot(const t_slot& slot)
	{ return m_sigProgress->connect(slot); }

	// show progress messages on the console
	void AddConsoleProgressHandler();
	// ------------------------------------------------------------------------

	// ------------------------------------------------------------------------
	// exporting of data
	// ------------------------------------------------------------------------
	// save contour line segments to lines test tools
	bool SaveToLinesTool(std::ostream& ostr);
	bool SaveToLinesTool(const std::string& filename);

	// export the path to various formats using a visitor
	bool AcceptExporter(const PathsExporterBase *exporter,
		const std::vector<t_vec2>& path, bool path_in_rad = false)
	{ return exporter->Export(this, path, path_in_rad); }
	// ------------------------------------------------------------------------


private:
	const InstrumentSpace *m_instrspace{};
	const TasCalculator *m_tascalc{};

	// and-combine return values for calculation progress signal
	struct combine_sigret
	{
		using result_type = bool;

		template<class t_iter>
		bool operator()(t_iter begin, t_iter end) const
		{
			result_type ret = true;

			for(t_iter iter = begin; iter != end; iter = std::next(iter))
				ret = ret && *iter;

			return ret;
		}
	};

	// calculation progress signal
	using t_sig_progress =
		boost::signals2::signal<
			bool(CalculationState, t_real progress, const std::string& message),
			combine_sigret>;
	std::shared_ptr<t_sig_progress> m_sigProgress{};

	// angular ranges
	t_real m_monoScatteringRange[2]{0, tl2::pi<t_real>};
	t_real m_sampleScatteringRange[2]{0, tl2::pi<t_real>};

	// index tree for wall positions (in pixel coordinates)
	geo::ClosestPixelTreeResults<t_contourvec> m_wallsindextree{};

	// wall contours in configuration space
	geo::Image<std::uint8_t> m_img{};
	std::vector<std::vector<t_contourvec>> m_wallcontours = {};
	std::vector<std::vector<t_contourvec>> m_fullwallcontours = {};

	// line segments (in pixel coordinates) and groups from the wall contours
	std::vector<t_line> m_lines{};
	std::vector<std::pair<std::size_t, std::size_t>> m_linegroups{};

	// arbitrary points outside the regions
	std::vector<t_vec2> m_points_outside_regions{};
	std::vector<bool> m_inverted_regions{};

	// voronoi vertices, edges and graph from the line segments
	geo::VoronoiLinesResults<t_vec2, t_line, t_graph> m_voro_results{};

	// general, angular and voronoi edge calculation epsilon
	t_real m_eps = 1e-3;
	t_real m_eps_angular = 1e-3;
	t_real m_voroedge_eps = 1e-2;

	// minimum distance to keep from the walls (e.g. for direct path calculation)
	t_real m_min_angular_dist_to_walls = 5. / t_real(180.) * tl2::pi<t_real>;

	// remove bisectors that are below the minimum distance given above
	bool m_remove_bisectors_below_min_wall_dist = true;

	// minimum distance to consider "staircase artefacts"
	t_real m_simplify_mindist = 3.;

	bool m_use_motor_speeds = true;

	// line segment length for subdivisions
	t_real m_subdiv_len = 0.1;

	// try to find a direct path if possible
	bool m_directpath = true;

	// radius inside with to search for direct paths
	t_real m_directpath_search_radius = 20. / t_real(180.) * tl2::pi<t_real>;

	// number of closest voronoi vertices to look at for retraction point search
	unsigned int m_num_closest_voronoi_vertices = 64;

	// check the generated path for collisions
	bool m_verifypath = true;

	// maximum number of threads to use in calculations
	unsigned int m_maxnum_threads = 4;
};

#endif
