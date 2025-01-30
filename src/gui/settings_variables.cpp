/**
 * TAS paths tool -- global settings variables
 * @author Tobias Weber <tweber@ill.fr>
 * @date apr-2021
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

#include "settings_variables.h"
#include "tlibs2/libs/maths.h"

#include <boost/predef.h>


// ----------------------------------------------------------------------------
// global settings variables
// ----------------------------------------------------------------------------

// resource manager
Resources g_res{};

// application binary, root, and home directory path
std::string g_apppath = ".";
std::optional<std::string> g_appdirpath;
std::string g_homepath = "~/";
std::string g_desktoppath = g_homepath;
std::string g_docpath = g_homepath;
std::string g_imgpath = g_homepath;

#if BOOST_OS_MACOS
	int g_use_taspaths_subdir = 1;
#else
	int g_use_taspaths_subdir = 0;
#endif


// maximum number of threads
unsigned int g_maxnum_threads = 4;

// maximum number of recent files
unsigned int g_maxnum_recents = 16;


// epsilons and precisions
int g_prec = 6;
int g_prec_gui = 4;
t_real g_eps = 1e-6;
t_real g_eps_angular = 0.01 / 180. * tl2::pi<t_real>;
t_real g_eps_gui = 1e-4;
t_real g_eps_voronoiedge = 2e-2;

t_real g_line_subdiv_len = 0.025;

t_real g_a3_offs = tl2::pi<t_real>*0.5;

t_real g_a2_delta = 0.5 / 180. * tl2::pi<t_real>;
t_real g_a4_delta = 1. / 180. * tl2::pi<t_real>;


// which polygon intersection method should be used?
// 0: sweep, 1: half-plane test
int g_poly_intersection_method = 1;

// which backend to use for contour calculation?
// 0: internal, 1: opencv
int g_contour_backend = 0;

// which backend to use for voronoi diagram calculation?
// 0: boost.polygon, 1: cgal
int g_voronoi_backend = 0;

// use region calculation function
int g_use_region_function = 1;

// use bisector verification function
int g_remove_bisectors_below_min_wall_dist = 0;


// path-finding options
int g_pathstrategy = 0;
int g_try_direct_path = 1;
int g_verifypath = 1;

// number of closest voronoi vertices to consider for retraction point search
unsigned int g_num_closest_voronoi_vertices = 64;

// maximum angular search radius for direct paths
t_real g_directpath_search_radius = 20. / t_real(180.) * tl2::pi<t_real>;


// minimum distance to keep from the walls
t_real g_min_dist_to_walls = 5. / t_real(180.) * tl2::pi<t_real>;



// path-tracker FPS and render timer TPS
unsigned int g_pathtracker_fps = 30;
unsigned int g_timer_tps = 30;

// path-tracker interpolation factor
unsigned int g_pathtracker_interpolation = 1;


// renderer options
tl2::t_real_gl g_move_scale = tl2::t_real_gl(1./75.);
tl2::t_real_gl g_zoom_scale = 0.0025;
tl2::t_real_gl g_wheel_zoom_scale = tl2::t_real_gl(1./64.);
tl2::t_real_gl g_rotation_scale = tl2::t_real_gl(0.02);
/*tl2::t_real_gl*/ t_real g_light_height = 7.5;

int g_light_follows_cursor = 0;
int g_enable_shadow_rendering = 1;

int g_draw_bounding_rectangles = 0;

// screenshots
int g_combined_screenshots = 0;
int g_automatic_screenshots = 0;


// gui theme
QString g_theme = "Fusion";

// gui font
QString g_font = "";

// use native menubar?
int g_use_native_menubar = 1;

// use native dialogs?
int g_use_native_dialogs = 1;

// use gui animations?
int g_use_animations = 0;

// allow tabbed dock widgets?
int g_tabbed_docks = 0;

// allow nested dock widgets?
int g_nested_docks = 0;

// allow flashing buttons?
//int g_allow_gui_flashing = 0;
// ----------------------------------------------------------------------------
