/**
 * the paths builder comprises two steps:
 *   (1) it calculates the path mesh (i.e. the roadmap) of possible instrument
 *       paths (file: PathsMeshBuilder.cpp, this file).
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

#include "PathsBuilder.h"

#include <iostream>
#include <thread>
#include <future>
#include <cmath>
#include <cstdint>

#include "mingw_hacks.h"
#include <boost/asio.hpp>
namespace asio = boost::asio;

using t_task = std::packaged_task<void()>;
using t_taskptr = std::shared_ptr<t_task>;



// ----------------------------------------------------------------------------
// paths builder -- path mesh calculation part
// ----------------------------------------------------------------------------
/**
 * constructor
 */
PathsBuilder::PathsBuilder()
	: m_sigProgress{std::make_shared<t_sig_progress>()}
{ }


/**
 * destructor
 */
PathsBuilder::~PathsBuilder()
{ }


void PathsBuilder::Clear()
{
	//m_img.Clear();
	m_wallsindextree.Clear();

	m_wallcontours.clear();
	m_fullwallcontours.clear();

	m_lines.clear();
	m_linegroups.clear();

	m_voro_results.Clear();
}


// ------------------------------------------------------------------------
// progress status handler
// ------------------------------------------------------------------------
/**
 * show progress messages on the console
 */
void PathsBuilder::AddConsoleProgressHandler()
{
	auto handler = []
		(CalculationState state, t_real progress,
		const std::string& msg) -> bool
	{
		std::string statedescr{};
		if(state == CalculationState::STARTED)
			statedescr = " -- START";
		else if(state == CalculationState::FAILED)
			statedescr = " -- FAIL";
		else if(state == CalculationState::SUCCEEDED)
			statedescr = " -- SUCCESS";

		std::cout << std::fixed << "["
			<< std::setw(3) << (int)(progress * 100.)
			<< "%" << statedescr << "] " << msg << std::endl;
		return true;
	};

	AddProgressSlot(handler);
}
// ------------------------------------------------------------------------



/**
 * convert a pixel of the plot image into the angular range of the plot
 */
t_vec2 PathsBuilder::PixelToAngle(const t_vec2& pix, bool deg, bool inc_sense) const
{
	return PixelToAngle(pix[0], pix[1], deg, inc_sense);
}


/**
 * convert angular coordinates to a pixel in the plot image
 */
t_vec2 PathsBuilder::AngleToPixel(const t_vec2& angle, bool deg, bool inc_sense) const
{
	return AngleToPixel(angle[0], angle[1], deg, inc_sense);
}


/**
 * convert a pixel of the plot image into the angular range of the plot
 */
t_vec2 PathsBuilder::PixelToAngle(t_real img_x, t_real img_y, bool deg, bool inc_sense) const
{
	t_real x = std::lerp(m_sampleScatteringRange[0], m_sampleScatteringRange[1],
		img_x / t_real(m_img.GetWidth()));
	t_real y = std::lerp(m_monoScatteringRange[0], m_monoScatteringRange[1],
		img_y / t_real(m_img.GetHeight()));

	if(deg)
	{
		x *= t_real(180) / tl2::pi<t_real>;
		y *= t_real(180) / tl2::pi<t_real>;
	}

	const t_real *sensesCCW = nullptr;
	if(m_tascalc)
		sensesCCW = m_tascalc->GetScatteringSenses();

	if(inc_sense && sensesCCW)
	{
		// move analysator instead of monochromator?
		std::size_t mono_idx = 0;
		if(!std::get<1>(m_tascalc->GetKfix()))
			mono_idx = 2;

		x *= sensesCCW[1];
		y *= sensesCCW[mono_idx];
	}

	return tl2::create<t_vec2>({x, y});
}


/**
 * convert angular coordinates to a pixel in the plot image
 */
t_vec2 PathsBuilder::AngleToPixel(t_real angle_x, t_real angle_y, bool deg, bool inc_sense) const
{
	if(deg)
	{
		angle_x *= tl2::pi<t_real> / t_real(180);
		angle_y *= tl2::pi<t_real> / t_real(180);
	}

	const t_real *sensesCCW = nullptr;
	if(m_tascalc)
		sensesCCW = m_tascalc->GetScatteringSenses();

	if(inc_sense && sensesCCW)
	{
		// move analysator instead of monochromator?
		std::size_t mono_idx = 0;
		if(!std::get<1>(m_tascalc->GetKfix()))
			mono_idx = 2;

		angle_x *= sensesCCW[1];
		angle_y *= sensesCCW[mono_idx];
	}


	t_real x = std::lerp(t_real(0.), t_real(m_img.GetWidth()),
		(angle_x - m_sampleScatteringRange[0]) / (m_sampleScatteringRange[1] - m_sampleScatteringRange[0]));
	t_real y = std::lerp(t_real(0.), t_real(m_img.GetHeight()),
		(angle_y - m_monoScatteringRange[0]) / (m_monoScatteringRange[1] - m_monoScatteringRange[0]));

	return tl2::create<t_vec2>({x, y});
}


/**
 * returns the full or the simplified wall contours
 */
const std::vector<std::vector<PathsBuilder::t_contourvec>>&
PathsBuilder::GetWallContours(bool full) const
{
	if(full)
		return m_fullwallcontours;

	return m_wallcontours;
}


/**
 * indicate that a new workflow starts
 */
void PathsBuilder::StartPathMeshWorkflow()
{
	(*m_sigProgress)(CalculationState::STARTED, 1, "Workflow starting.");
}


/**
 * indicate that the current workflow has ended
 */
void PathsBuilder::FinishPathMeshWorkflow(bool success)
{
	CalculationState state = success ? CalculationState::SUCCEEDED : CalculationState::FAILED;
	(*m_sigProgress)(state, 1, "Workflow has finished.");
}


/**
 * calculate the obstacle regions in the angular configuration space
 * the monochromator a1/a2 variables can alternatively refer to the analyser a5/a6 in case kf is not fixed
 */
bool PathsBuilder::CalculateConfigSpace(
	t_real da2, t_real da4,
	t_real starta2, t_real enda2,
	t_real starta4, t_real enda4)
{
	if(!m_instrspace)
		return false;

	m_sampleScatteringRange[0] = starta4;
	m_sampleScatteringRange[1] = enda4;
	m_monoScatteringRange[0] = starta2;
	m_monoScatteringRange[1] = enda2;

	std::ostringstream ostrmsg;
	ostrmsg << "Calculating configuration space in " << m_maxnum_threads << " threads...";
	(*m_sigProgress)(CalculationState::STEP_STARTED, 0, ostrmsg.str());

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

	/*if(kf_fixed)
		std::cout << "a2 range: ";
	else
		std::cout << "a6 range: ";
	std::cout << starta2/tl2::pi<t_real>*180.
		<< " .. " << enda2/tl2::pi<t_real>*180.
		<< std::endl;*/

	const Instrument& instr = m_instrspace->GetInstrument();

	// analyser angle (alternatively monochromator angle if kf is not fixed)
	t_real a6 = kf_fixed
		? instr.GetAnalyser().GetAxisAngleOut()	      // a6 or
		: instr.GetMonochromator().GetAxisAngleOut(); // a2

	// include scattering senses
	if(sensesCCW)
	{
		da4 *= sensesCCW[1];
		starta4 *= sensesCCW[1];
		enda4 *= sensesCCW[1];

		da2 *= sensesCCW[mono_idx];
		starta2 *= sensesCCW[mono_idx];
		enda2 *= sensesCCW[mono_idx];
	}

	// create colour map and image
	std::size_t img_w = (enda4-starta4) / da4;
	std::size_t img_h = (enda2-starta2) / da2;
	//std::cout << "Image size: " << img_w << " x " << img_h << "." << std::endl;
	m_img.Init(img_w, img_h);

	// create thread pool
	asio::thread_pool pool(m_maxnum_threads);

	std::vector<t_taskptr> tasks;
	tasks.reserve(img_h);


	// set image pixels
	std::atomic<std::size_t> num_pixels = 0;
	for(std::size_t img_row=0; img_row<img_h; ++img_row)
	{
		auto task = [this, img_w, img_row, a6, kf_fixed, &num_pixels]()
		{
			InstrumentSpace instrspace_cpy = *this->m_instrspace;

			for(std::size_t img_col=0; img_col<img_w; ++img_col)
			{
				t_vec2 angle = PixelToAngle(img_col, img_row, false, true);
				t_real a4 = angle[0];
				t_real a2 = angle[1];
				t_real a3 = a4 * 0.5;

				Instrument& instr = instrspace_cpy.GetInstrument();

				// set scattering angles (a2 and a6 are flipped in case kf is not fixed)
				instr.GetMonochromator().SetAxisAngleOut(kf_fixed ? a2 : a6);
				instr.GetSample().SetAxisAngleOut(a4);
				instr.GetAnalyser().SetAxisAngleOut(kf_fixed ? a6 : a2);

				// set crystal angles (a1 and a5 are flipped in case kf is not fixed)
				instr.GetMonochromator().SetAxisAngleInternal(kf_fixed ? 0.5*a2 : 0.5*a6);
				instr.GetSample().SetAxisAngleInternal(a3);
				instr.GetAnalyser().SetAxisAngleInternal(kf_fixed ? 0.5*a6 : 0.5*a2);

				// set image value
				bool angle_ok = instrspace_cpy.CheckAngularLimits();

				if(!angle_ok)
				{
					m_img.SetPixel(img_col, img_row, PATHSBUILDER_PIXEL_VALUE_FORBIDDEN_ANGLE);
				}
				else
				{
					bool colliding = instrspace_cpy.CheckCollision2D();
					m_img.SetPixel(img_col, img_row, colliding ? PATHSBUILDER_PIXEL_VALUE_COLLISION : PATHSBUILDER_PIXEL_VALUE_NOCOLLISION);
				}

				++num_pixels;
			}

			//std::cout << a2/tl2::pi<t_real>*180. << " finished" << std::endl;
		};

		t_taskptr taskptr = std::make_shared<t_task>(task);
		tasks.push_back(taskptr);
		asio::post(pool, [taskptr]() { (*taskptr)(); });
	}


	// get results
	std::size_t num_tasks = tasks.size();
	// send no more than (100/25) percent update signals
	std::size_t signal_skip = num_tasks / 25;

	for(std::size_t taskidx=0; taskidx<num_tasks; ++taskidx)
	{
		// prevent sending too many progress signals
		if(signal_skip && (taskidx % signal_skip == 0))
		{
			if(!(*m_sigProgress)(CalculationState::RUNNING, t_real(taskidx) / t_real(num_tasks), ostrmsg.str()))
			{
				pool.stop();
				break;
			}
		}

		tasks[taskidx]->get_future().get();
		//std::cout << taskidx << " / " << num_tasks << " finished" << std::endl;
	}

	pool.join();
	(*m_sigProgress)(CalculationState::STEP_SUCCEEDED, 1, ostrmsg.str());

	//std::cout << "pixels total: " << img_h*img_w << ", calculated: " << num_pixels << std::endl;
	return num_pixels == img_h*img_w;
}


/**
 * save all wall position in an index tree for more efficient position lookup
 */
bool PathsBuilder::CalculateWallsIndexTree()
{
	m_wallsindextree = geo::build_closest_pixel_tree<t_contourvec, decltype(m_img)>(m_img);
	return true;
}


/**
 * calculate the contour lines of the obstacle regions
 */
bool PathsBuilder::CalculateWallContours(bool simplify, bool convex_split)
{
	std::string message{"Calculating obstacle contours..."};
	(*m_sigProgress)(CalculationState::STEP_STARTED, 0, message);

	m_wallcontours = geo::trace_boundary<t_contourvec, decltype(m_img)>(m_img);
	m_fullwallcontours = m_wallcontours;

	(*m_sigProgress)(CalculationState::RUNNING, 0.33, message);

	if(simplify)
	{
		// iterate and simplify contour groups
		for(auto& contour : m_wallcontours)
		{
			// replace contour with its convex hull
			//std::vector<t_vec2> contour_real = tl2::convert<t_vec2, t_contourvec, std::vector>(contour);
			//auto [hull_verts, hull_lines, hull_indices]
			//	= geo::calc_delaunay<t_vec2>(2, contour_real, true);
			//contour = tl2::convert<t_contourvec, t_vec2, std::vector>(hull_verts);

			// simplify hull contour
			geo::simplify_contour<t_contourvec, t_real>(contour, m_simplify_mindist, m_eps_angular, m_eps);
		}
	}

	(*m_sigProgress)(CalculationState::RUNNING, 0.66, message);

	if(convex_split)
	{
		// convex split
		std::vector<std::vector<t_contourvec>> splitcontours;
		splitcontours.reserve(m_wallcontours.size()*2);

		for(auto& contour : m_wallcontours)
		{
			//std::reverse(contour.begin(), contour.end());
			auto splitcontour = geo::convex_split<t_contourvec, t_real>(contour, m_eps);
			if(splitcontour.size())
			{
				for(auto&& poly : splitcontour)
				{
					//std::reverse(poly.begin(), poly.end());
					splitcontours.emplace_back(std::move(poly));
				}
			}
			else
			{
				// no split, use original contour
				splitcontours.push_back(contour);
			}
		}

		m_wallcontours = std::move(splitcontours);
	}

	(*m_sigProgress)(CalculationState::STEP_SUCCEEDED, 1, message);
	return true;
}


/**
 * calculate lines segments and groups
 */
bool PathsBuilder::CalculateLineSegments(bool use_region_function)
{
	std::string message{"Calculating obstacle line segments..."};
	(*m_sigProgress)(CalculationState::STEP_STARTED, 0, message);

	m_lines.clear();
	m_linegroups.clear();
	m_points_outside_regions.clear();
	m_inverted_regions.clear();

	// find an arbitrary point outside all obstacles
	auto find_point_outside_regions = [this]
	(std::size_t x_start = 0, std::size_t y_start = 0,
		bool skip_search = false) -> t_vec2
	{
		t_vec2 point_outside_regions = tl2::create<t_vec2>({-50, -40});
		bool found_point = false;

		if(!skip_search)
		{
			for(std::size_t y=y_start; y<m_img.GetHeight(); ++y)
			{
				for(std::size_t x=x_start; x<m_img.GetWidth(); ++x)
				{
					if(m_img.GetPixel(x, y) == PATHSBUILDER_PIXEL_VALUE_NOCOLLISION)
					{
						point_outside_regions =
							tl2::create<t_vec2>({
								static_cast<double>(x),
								static_cast<double>(y)
							});
						found_point = true;
						break;
					}
				}
				if(found_point)
					break;
			}
		}

		return point_outside_regions;
	};

	std::size_t totalverts = 0;
	for(const auto& contour : m_wallcontours)
		totalverts += contour.size();

	m_lines.reserve(totalverts/2 + 1);
	m_linegroups.reserve(m_wallcontours.size());

	if(!use_region_function)
	{
		m_points_outside_regions.reserve(m_wallcontours.size());
		m_inverted_regions.reserve(m_wallcontours.size());
	}

	// contour vertices
	std::size_t linectr = 0;
	for(std::size_t contouridx = 0; contouridx < m_wallcontours.size(); ++contouridx)
	{
		const auto& contour = m_wallcontours[contouridx];
		std::size_t groupstart = linectr;
		t_contourvec contour_mean = tl2::zero<t_vec2>(2);

		for(std::size_t vert1 = 0; vert1 < contour.size(); ++vert1)
		{
			std::size_t vert2 = (vert1 + 1) % contour.size();

			const t_contourvec& vec1 = contour[vert1];
			const t_contourvec& vec2 = contour[vert2];
			contour_mean += vec1;

			t_vec2 linevec1 = vec1;
			t_vec2 linevec2 = vec2;

			m_lines.emplace_back(
				std::make_pair(std::move(linevec1), std::move(linevec2)));

			++linectr;
		}

		contour_mean /= contour.size();

		// move a point on the contour in the direction of the contour mean
		// to get a point inside the contour
		//t_contourvec inside_contour = contour[0] + (contour_mean-contour[0]) / 8;

		// find a point outside the contour by moving a pixel away from the minimum vertex
		auto [contour_min, contour_max] = tl2::minmax(contour);
		t_contourvec outside_contour = contour_min;
		for(int i = 0; i < 2; ++i)
			outside_contour[i] -= 1;

		// mark line group start and end index
		std::size_t groupend = linectr;

		// don't include outer bounding region
		// TODO: test if such a region is there
		if(contouridx > 0)
		{
			m_linegroups.emplace_back(std::make_pair(groupstart, groupend));

			if(!use_region_function)
			{
				t_vec2 point_outside_regions =
					find_point_outside_regions(contour[0][0], contour[0][1], true);
				m_points_outside_regions.emplace_back(
					std::move(point_outside_regions));

				//auto pix_incontour = m_img.GetPixel(inside_contour[0], inside_contour[1]);
				auto pix_outcontour = m_img.GetPixel(outside_contour[0], outside_contour[1]);

#ifdef DEBUG
				std::cout << "contour " << std::dec << contouridx
					<< ", pixel inside " << inside_contour[0] << ", " << inside_contour[1]
					<< ": " << std::hex << int(pix_incontour) << std::dec
					<< "; pixel outside " << outside_contour[0] << ", " << outside_contour[1]
					<< ": " << std::hex << int(pix_outcontour) << std::endl;
#endif

				// normal regions encircle forbidden coordinate points
				// inverted regions encircle allowed coordinate points
				//m_inverted_regions.push_back(pix_incontour == PATHSBUILDER_PIXEL_VALUE_NOCOLLISION);
				m_inverted_regions.push_back(pix_outcontour != PATHSBUILDER_PIXEL_VALUE_NOCOLLISION);
			}
		}
	}

	(*m_sigProgress)(CalculationState::STEP_SUCCEEDED, 1, message);
	return true;
}


/**
 * calculate the voronoi diagram
 */
bool PathsBuilder::CalculateVoronoi(bool group_lines, VoronoiBackend backend,
	bool use_region_function)
{
	std::string message{"Calculating Voronoi diagram..."};
	(*m_sigProgress)(CalculationState::STEP_STARTED, 0, message);

	// is the vertex in a forbidden region?
	std::function<bool(const t_vec2&)> region_func = [this](const t_vec2& vec) -> bool
	{
		if(vec[0] < 0 || vec[1] < 0)
			return true;

		std::size_t x = vec[0];
		std::size_t y = vec[1];

		if(x >= m_img.GetWidth() || y >= m_img.GetHeight())
			return true;

		// an occupied pixel signifies a forbidden region
		if(m_img.GetPixel(x, y) != PATHSBUILDER_PIXEL_VALUE_NOCOLLISION)
			return true;

		return false;
	};

	// validation function that checks if (voronoi) vertices are far enough from any wall
	std::function<bool(const t_vec2&)> validation_func = [this](const t_vec2& vec) -> bool
	{
		t_real dist_to_walls = GetDistToNearestWall(vec);
		return dist_to_walls >= m_min_angular_dist_to_walls;
	};

	geo::VoronoiLinesRegions<t_vec2, t_line> regions{};
	regions.SetGroupLines(group_lines);
	regions.SetRemoveVoronoiVertices(true);
	regions.SetLineGroups(&m_linegroups);
	regions.SetPointsOutsideRegions(&m_points_outside_regions);
	regions.SetInvertedRegions(&m_inverted_regions);
	regions.SetRegionFunc(use_region_function ? &region_func : nullptr);
	regions.SetValidateFunc(m_remove_bisectors_below_min_wall_dist ? &validation_func : nullptr);

	if(backend == VoronoiBackend::BOOST)
	{
		m_voro_results
			= geo::calc_voro<t_vec2, t_line, t_graph>(
				m_lines, m_eps, m_voroedge_eps, &regions);
	}
#ifdef USE_CGAL
	else if(backend == VoronoiBackend::CGAL)
	{
		m_voro_results
			= geo::calc_voro_cgal<t_vec2, t_line, t_graph>(
				m_lines, m_eps, m_voroedge_eps, &regions);
	}
#endif
	else
	{
		// invalid backend selected
		(*m_sigProgress)(CalculationState::FAILED, 1, message);
		return false;
	}

	(*m_sigProgress)(CalculationState::STEP_SUCCEEDED, 1, message);
	return true;
}


/**
 * get a line segment group
 * helper function for the scripting interface
 */
std::vector<std::array<t_real, 4>>
PathsBuilder::GetLineSegmentRegionAsArray(std::size_t groupidx) const
{
	std::vector<std::array<t_real, 4>> lines;

	if(groupidx >= GetNumberOfLineSegmentRegions())
		return lines;
	auto[startidx, endidx] = m_linegroups[groupidx];

	lines.reserve(endidx - startidx);

	for(std::size_t lineidx=startidx; lineidx<endidx; ++lineidx)
	{
		const t_line& line = m_lines[lineidx];

		t_vec2 pt1 = PixelToAngle(std::get<0>(line), true, false);
		t_vec2 pt2 = PixelToAngle(std::get<1>(line), true, false);

		std::array<t_real, 4> arr{{ pt1[0], pt1[1], pt2[0], pt2[1] }};
		lines.emplace_back(std::move(arr));
	}

	return lines;
}

// ------------------------------------------------------------------------



// ------------------------------------------------------------------------
// exporting of data
// ------------------------------------------------------------------------

/**
 * save the contour line segments to the lines tool
 */
bool PathsBuilder::SaveToLinesTool(std::ostream& ostr)
{
	ostr << "<lines2d>\n";
	std::vector<std::pair<std::size_t, std::size_t>> group_indices;
	group_indices.reserve(m_linegroups.size());

	// contour vertices
	std::size_t vertctr = 0;
	ostr << "<vertices>\n";
	for(std::size_t contouridx = 0; contouridx < m_linegroups.size(); ++contouridx)
	{
		const auto& contour = m_linegroups[contouridx];
		ostr << "\t<!-- contour " << contouridx << " -->\n";

		std::size_t group_begin = vertctr;

		for(std::size_t lineidx=std::get<0>(contour); lineidx<std::get<1>(contour); ++lineidx)
		{
			const t_line& line = m_lines[lineidx];

			ostr << "\t<" << vertctr;
			ostr << " x=\"" << std::get<0>(line)[0] << "\"";
			ostr << " y=\"" << std::get<0>(line)[1] << "\"";
			ostr << "/>\n";
			++vertctr;

			ostr << "\t<" << vertctr;
			ostr << " x=\"" << std::get<1>(line)[0] << "\"";
			ostr << " y=\"" << std::get<1>(line)[1] << "\"";
			ostr << "/>\n\n";
			++vertctr;
		}

		std::size_t group_end = vertctr;
		group_indices.emplace_back(std::make_pair(group_begin, group_end));
	}
	ostr << "</vertices>\n";

	// contour groups
	ostr << "\n<groups>\n";
	for(std::size_t groupidx = 0; groupidx < group_indices.size(); ++groupidx)
	{
		const auto& group = group_indices[groupidx];
		ostr << "\t<!-- contour " << groupidx << " -->\n";
		ostr << "\t<" << groupidx << ">\n";

		ostr << "\t\t<begin>" << std::get<0>(group) << "</begin>\n";
		ostr << "\t\t<end>" << std::get<1>(group) << "</end>\n";

		ostr << "\t</" << groupidx << ">\n\n";
	}
	ostr << "</groups>\n";

	// alternatively: contour regions (obsolete)
	/*ostr << "\n<regions>\n";
	for(std::size_t contouridx = 0; contouridx<m_wallcontours.size(); ++contouridx)
	{
		const auto& contour = m_wallcontours[contouridx];
		ostr << "\t<!-- contour " << contouridx << " -->\n";
		ostr << "\t<" << contouridx << ">\n";

		for(std::size_t vertidx=0; vertidx<contour.size(); ++vertidx)
		{
			const t_contourvec& vec = contour[vertidx];

			ostr << "\t\t<" << vertidx;
			ostr << " x=\"" << vec[0] << "\"";
			ostr << " y=\"" << vec[1] << "\"";
			ostr << "/>\n";
		}

		ostr << "\t</" << contouridx << ">\n\n";
	}
	ostr << "</regions>\n";*/

	ostr << "</lines2d>" << std::endl;
	return true;
}


/**
 * save the contour line segments to the lines tool
 */
bool PathsBuilder::SaveToLinesTool(const std::string& filename)
{
	std::ofstream ofstr(filename);
	if(!ofstr)
		return false;

	return SaveToLinesTool(ofstr);
}

// ------------------------------------------------------------------------
