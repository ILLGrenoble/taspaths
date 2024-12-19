/**
 * instrument space and walls
 * @author Tobias Weber <tweber@ill.fr>
 * @date feb-2021
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

#include <unordered_map>
#include <optional>

#include "InstrumentSpace.h"
#include "src/libs/lines.h"
#include "src/libs/hull.h"
#include "src/libs/ptree.h"
#include "tlibs2/libs/file.h"
#include "tlibs2/libs/str.h"

namespace pt = boost::property_tree;


InstrumentSpace::InstrumentSpace()
	: m_sigUpdate{std::make_shared<t_sig_update>()}
{
}


InstrumentSpace::~InstrumentSpace()
{
	Clear();
}


/**
 * assign data from another instrument space
 */
InstrumentSpace::InstrumentSpace(const InstrumentSpace& instr)
{
	*this = instr;
}


/**
 * assign data from another instrument space
 */
const InstrumentSpace& InstrumentSpace::operator=(const InstrumentSpace& instr)
{
	this->m_floorlen[0] = instr.m_floorlen[0];
	this->m_floorlen[1] = instr.m_floorlen[1];
	this->m_floorcol = instr.m_floorcol;

	this->m_walls = instr.m_walls;
	this->m_instr = instr.m_instr;

	this->m_drag_pos_axis_start = instr.m_drag_pos_axis_start;
	this->m_sigUpdate = std::make_shared<t_sig_update>();

	return *this;
}


/**
 * clear all data in the instrument space
 */
void InstrumentSpace::Clear()
{
	// reset to defaults
	m_floorlen[0] = m_floorlen[1] = 10.;
	m_floorcol = tl2::create<t_vec>({0.5, 0.5, 0.5});

	// clear
	m_walls.clear();
	m_instr.Clear();

	// remove listeners
	m_sigUpdate = std::make_shared<t_sig_update>();
}


/**
 * load instrument and wall configuration from a property tree
 */
bool InstrumentSpace::Load(const pt::ptree& prop)
{
	Clear();
	m_instr.Clear();


	// floor size
	if(auto opt = prop.get_optional<t_real>("floor.len_x"); opt)
		m_floorlen[0] = *opt;
	if(auto opt = prop.get_optional<t_real>("floor.len_y"); opt)
		m_floorlen[1] = *opt;

	// floor colour
	if(auto col = prop.get_optional<std::string>("floor.colour"); col)
	{
		m_floorcol.clear();
		tl2::parse_tokens<t_real>(
			tl2::trimmed(*col), std::string{" \t,;"}, m_floorcol);

		if(m_floorcol.size() < 3)
			m_floorcol.resize(3);
	}


	// walls
	if(auto walls = prop.get_child_optional("walls"); walls)
	{
		// iterate wall segments
		for(const auto &wall : *walls)
		{
			auto id = wall.second.get<std::string>("<xmlattr>.id", "");
			auto geo = wall.second.get_child_optional("geometry");
			if(!geo)
				continue;

			if(auto geoobj = Geometry::load(*geo); std::get<0>(geoobj))
				AddWall(std::get<1>(geoobj), id);
		}
	}

	// instrument
	bool instr_ok = false;
	if(auto instr = prop.get_child_optional("instrument"); instr)
		instr_ok = m_instr.Load(*instr);

	return instr_ok;
}


/**
 * save the instrument and wall configuration into a property tree
 */
pt::ptree InstrumentSpace::Save() const
{
	pt::ptree prop;

	// floor
	prop.put<t_real>(FILE_BASENAME "instrument_space.floor.len_x", m_floorlen[0]);
	prop.put<t_real>(FILE_BASENAME "instrument_space.floor.len_y", m_floorlen[1]);
	prop.put<std::string>(FILE_BASENAME "instrument_space.floor.colour", geo_vec_to_str(m_floorcol));

	// walls
	pt::ptree propwalls;
	for(std::size_t wallidx=0; wallidx<m_walls.size(); ++wallidx)
	{
		const auto& wall = m_walls[wallidx];

		pt::ptree propwall;
		propwall.put<std::string>("<xmlattr>.id", "wall " + std::to_string(wallidx+1));
		propwall.put_child("geometry", wall->Save());

		pt::ptree propwall2;
		propwall2.put_child("wall", propwall);
		propwalls.insert(propwalls.end(), propwall2.begin(), propwall2.end());
	}

	prop.put_child(FILE_BASENAME "instrument_space.walls", propwalls);
	prop.put_child(FILE_BASENAME "instrument_space.instrument", m_instr.Save());

	return prop;
}


/**
 * add a wall to the instrument space
 */
void InstrumentSpace::AddWall(const std::vector<std::shared_ptr<Geometry>>& wallsegs, const std::string& id)
{
	// get individual 3d primitives that comprise this wall
	for(auto& wallseg : wallsegs)
	{
		if(wallseg->GetId() == "")
			wallseg->SetId(id);
		m_walls.push_back(wallseg);
	}
}


/**
 * delete an object (so far only walls)
 */
bool InstrumentSpace::DeleteObject(const std::string& id)
{
	// find the wall with the given id
	if(auto iter = std::find_if(m_walls.begin(), m_walls.end(), [&id](const std::shared_ptr<Geometry>& wall) -> bool
	{
		return wall->GetId() == id;
	}); iter != m_walls.end())
	{
		m_walls.erase(iter);
		return true;
	}

	// TODO: handle other cases besides walls
	return false;
}


/**
 * rename an object (so far only walls)
 */
bool InstrumentSpace::RenameObject(const std::string& oldid, const std::string& newid)
{
	// find the wall with the given id
	if(auto iter = std::find_if(m_walls.begin(), m_walls.end(),
		[&oldid](const std::shared_ptr<Geometry>& wall) -> bool
		{
			return wall->GetId() == oldid;
		}); iter != m_walls.end())
	{
		(*iter)->SetId(newid);
		return true;
	}

	// TODO: handle other cases besides walls
	return false;
}


/**
 * rotate an object by the given angle
 */
std::tuple<bool, std::shared_ptr<Geometry>> InstrumentSpace::RotateObject(const std::string& id, t_real angle)
{
	// find the wall with the given id
	if(auto iter = std::find_if(m_walls.begin(), m_walls.end(), [&id](const std::shared_ptr<Geometry>& wall) -> bool
	{
		return wall->GetId() == id;
	}); iter != m_walls.end())
	{
		(*iter)->Rotate(angle);
		return std::make_tuple(true, *iter);
	}

	// TODO: handle other cases besides walls
	return std::make_tuple(false, nullptr);
}


/**
 * check if the axis angles are within their limits
 */
bool InstrumentSpace::CheckAngularLimits() const
{
	const Axis& mono = GetInstrument().GetMonochromator();
	const Axis& sample = GetInstrument().GetSample();
	const Axis& ana = GetInstrument().GetAnalyser();

	for(const Axis& axis : { mono, sample, ana })
	{
		if(axis.GetAxisAngleIn() < axis.GetAxisAngleInLowerLimit())
			return false;
		if(axis.GetAxisAngleIn() > axis.GetAxisAngleInUpperLimit())
			return false;

		if(axis.GetAxisAngleInternal() < axis.GetAxisAngleInternalLowerLimit())
			return false;
		if(axis.GetAxisAngleInternal() > axis.GetAxisAngleInternalUpperLimit())
			return false;

		if(axis.GetAxisAngleOut() < axis.GetAxisAngleOutLowerLimit())
			return false;
		if(axis.GetAxisAngleOut() > axis.GetAxisAngleOutUpperLimit())
			return false;
	}

	return true;
}


/**
 * check for collisions, using a 2d representation of the instrument space
 */
bool InstrumentSpace::CheckCollision2D() const
{
	// ------------------------------------------------------------------------
	// functions to extract object geometries
	// ------------------------------------------------------------------------
	// extract circle from cylinder and sphere geometry
	auto get_comp_circles = [](
		const std::shared_ptr<Geometry>& comp,
		std::tuple<t_vec, t_real>& circle,
		const t_mat* matAxis = nullptr)
	{
		const t_mat& matGeo = comp->GetTrafo();
		t_mat mat = matAxis ? (*matAxis) * matGeo : matGeo;

		if(comp->GetType() == GeometryType::CYLINDER)
		{
			auto cyl = std::dynamic_pointer_cast<CylinderGeometry>(comp);

			// position already considered in trafo matrix
			t_vec pos = tl2::create<t_vec>({0,0,0,1}); //cyl->GetPos();
			t_real rad = cyl->GetRadius();

			// trafo in homogeneous coordinates
			if(pos.size() < 4)
				pos.push_back(1);
			pos = mat * pos;

			// only two dimensions needed
			pos.resize(2);

			std::get<0>(circle) = pos;
			std::get<1>(circle) = rad;
		}
		else if(comp->GetType() == GeometryType::SPHERE)
		{
			auto sph = std::dynamic_pointer_cast<SphereGeometry>(comp);

			// position already considered in trafo matrix
			t_vec pos = tl2::create<t_vec>({0,0,0,1}); //sph->GetPos();
			t_real rad = sph->GetRadius();

			// trafo in homogeneous coordinates
			if(pos.size() < 4)
				pos.push_back(1);
			pos = mat * pos;

			// only two dimensions needed
			pos.resize(2);

			std::get<0>(circle) = pos;
			std::get<1>(circle) = rad;
		}
	};


	// extract circles from cylinder and sphere geometries
	auto get_comps_circles = [&get_comp_circles](
		const std::vector<std::shared_ptr<Geometry>>& comps,
		std::vector<std::tuple<t_vec, t_real>>& circles,
		const t_mat* matAxis = nullptr)
	{
		circles.reserve(circles.size() + comps.size());

		for(const auto& comp : comps)
		{
			std::tuple<t_vec, t_real> circle;
			get_comp_circles(comp, circle, matAxis);

			if(std::get<0>(circle).size())
				circles.emplace_back(std::move(circle));
		}
	};


	auto get_circles = [&get_comps_circles](
		const Axis& axis,
		std::vector<std::tuple<t_vec, t_real>>& circles,
		bool inc_incoming = true,
		bool inc_internal = true,
		bool inc_outgoing = true)
	{
		std::vector<AxisAngle> axisangles;
		if(inc_incoming)
			axisangles.push_back(AxisAngle::INCOMING);
		if(inc_internal)
			axisangles.push_back(AxisAngle::INTERNAL);
		if(inc_outgoing)
			axisangles.push_back(AxisAngle::OUTGOING);

		// get geometries relative to incoming, internal, and outgoing axis
		for(AxisAngle axisangle : axisangles)
		{
			const t_mat& matAxis = axis.GetTrafo(axisangle);
			get_comps_circles(axis.GetComps(axisangle), circles, &matAxis);
		}
	};


	// extract 2d polygon from box geometry
	auto get_comp_polys = [](
		const std::shared_ptr<Geometry>& comp,
		std::vector<t_vec>& poly,
		const t_mat* matAxis = nullptr)
	{
		const t_mat& matGeo = comp->GetTrafo();
		t_mat mat = matAxis ? (*matAxis) * matGeo : matGeo;

		if(comp->GetType() == GeometryType::BOX)
		{
			auto cyl = std::dynamic_pointer_cast<BoxGeometry>(comp);

			t_real lx = cyl->GetLength() * t_real(0.5);
			t_real ly = cyl->GetDepth() * t_real(0.5);
			t_real lz = cyl->GetHeight() * t_real(0.5);

			std::vector<t_vec> vertices =
			{
				mat * tl2::create<t_vec>({ +lx, -ly, -lz, 1 }),	// vertex 0
				mat * tl2::create<t_vec>({ -lx, -ly, -lz, 1 }),	// vertex 1
				mat * tl2::create<t_vec>({ -lx, +ly, -lz, 1 }),	// vertex 2
				mat * tl2::create<t_vec>({ +lx, +ly, -lz, 1 }),	// vertex 3
			};

			// only two dimensions needed
			for(t_vec& vec : vertices)
				vec.resize(2);

			poly = std::move(vertices);
		}
	};


	// extract 2d polygons from box geometries
	auto get_comps_polys = [&get_comp_polys](
		const std::vector<std::shared_ptr<Geometry>>& comps,
		std::vector<std::vector<t_vec>>& polys,
		const t_mat* matAxis = nullptr)
	{
		polys.reserve(polys.size() + comps.size());

		for(const auto& comp : comps)
		{
			std::vector<t_vec> poly;
			get_comp_polys(comp, poly, matAxis);
			if(poly.size())
				polys.emplace_back(std::move(poly));
		}
	};


	auto get_polys = [&get_comps_polys](
		const Axis& axis,
		std::vector<std::vector<t_vec>>& polys,
		bool inc_incoming = true,
		bool inc_internal = true,
		bool inc_outgoing = true)
	{
		std::vector<AxisAngle> axisangles;
		if(inc_incoming)
			axisangles.push_back(AxisAngle::INCOMING);
		if(inc_internal)
			axisangles.push_back(AxisAngle::INTERNAL);
		if(inc_outgoing)
			axisangles.push_back(AxisAngle::OUTGOING);

		// get geometries relative to incoming, internal, and outgoing axis
		for(AxisAngle axisangle : axisangles)
		{
			const t_mat& matAxis = axis.GetTrafo(axisangle);
			get_comps_polys(axis.GetComps(axisangle), polys, &matAxis);
		}
	};
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// conversion from dynamic vectors to 2d arrays
	// ------------------------------------------------------------------------
	auto convert_circle_2d = [](const std::tuple<t_vec, t_real>& circle)
		-> std::optional<std::tuple<t_vec2, t_real>>
	{
		const t_vec& vec = std::get<0>(circle);

		// invalid vertex
		if(vec.size() < 2)
			return std::nullopt;

		return std::make_tuple(
			tl2::create<t_vec2>({vec[0], vec[1]}),
			std::get<1>(circle));
	};


	auto convert_circles_2d = [&convert_circle_2d]
		(const std::vector<std::tuple<t_vec, t_real>>& circles)
		-> std::vector<std::tuple<t_vec2, t_real>>
	{
		std::vector<std::tuple<t_vec2, t_real>> circles2d;
		circles2d.reserve(circles.size());

		for(const auto& circle : circles)
		{
			auto circle2d = convert_circle_2d(circle);
			if(circle2d)
			circles2d.emplace_back(std::move(*circle2d));
		}

		return circles2d;
	};


	auto convert_poly_2d = [](const std::vector<t_vec> &poly)
		-> std::optional<std::vector<t_vec2>>
	{
		std::vector<t_vec2> poly2d;
		poly2d.reserve(poly.size());

		for(const t_vec& vec : poly)
		{
			// invalid vertex
			if(vec.size() < 2)
				return std::nullopt;

			poly2d.emplace_back(tl2::create<t_vec2>({vec[0], vec[1]}));
		}

		return poly2d;
	};


	auto convert_polys_2d = [&convert_poly_2d]
		(const std::vector<std::vector<t_vec>> &polys)
		-> std::vector<std::vector<t_vec2>>
	{
		std::vector<std::vector<t_vec2>> polys2d;
		polys2d.reserve(polys.size());

		for(const auto& poly : polys)
		{
			auto poly2d = convert_poly_2d(poly);
			if(poly2d)
				polys2d.emplace_back(std::move(*poly2d));
		}

		return polys2d;
	};
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// collision checks
	// ------------------------------------------------------------------------
	// check if two polygonal objects collide
	auto check_collision_poly_poly = [this](
		const std::vector<std::vector<t_vec2>>& polys1,
		const std::vector<std::vector<t_vec2>>& polys2,
		const std::tuple<t_vec2, t_vec2>& bb1,
		const std::tuple<t_vec2, t_vec2>& bb2) -> bool
	{
		if(!tl2::collide_bounding_boxes(bb1, bb2))
			return false;

		for(std::size_t idx1=0; idx1<polys1.size(); ++idx1)
		{
			const auto& poly1 = polys1[idx1];

			for(std::size_t idx2=0; idx2<polys2.size(); ++idx2)
			{
				const auto& poly2 = polys2[idx2];

				switch(m_poly_intersection_method)
				{
					case 0:
						if(geo::collide_poly_poly<t_vec2>(poly1, poly2, m_eps))
							return true;
						break;
					case 1:
						if(geo::collide_poly_poly_simplified<t_vec2>(poly1, poly2))
							return true;
						break;
					default:
						// invalid method selected
						return false;
				}
			}
		}

		return false;
	};


	// check if two circular objects collide
	auto check_collision_circle_circle = [](
		const std::vector<std::tuple<t_vec2, t_real>>& circles1,
		const std::vector<std::tuple<t_vec2, t_real>>& circles2) -> bool
	{
		for(std::size_t idx1 = 0; idx1 < circles1.size(); ++idx1)
		{
			const auto& circle1 = circles1[idx1];

			for(std::size_t idx2 = 0; idx2 < circles2.size(); ++idx2)
			{
				const auto& circle2 = circles2[idx2];

				//using namespace tl2_ops;
				//std::cout << "circle 1: " << std::get<0>(circle1) << " " << std::get<1>(circle1) << std::endl;
				//std::cout << "circle 2: " << std::get<0>(circle2) << " " << std::get<1>(circle2) << std::endl;

				if(geo::collide_circle_circle<t_vec2>(
					std::get<0>(circle1), std::get<1>(circle1),
					std::get<0>(circle2), std::get<1>(circle2)))
					return true;
			}
		}

		return false;
	};


	// check if a circular and a polygonal object collide
	auto check_collision_circle_poly = [](
		const std::vector<std::tuple<t_vec2, t_real>>& circles,
		const std::vector<std::vector<t_vec2>>& polys,
		const std::tuple<t_vec2, t_vec2>& bbCircles,
		const std::tuple<t_vec2, t_vec2>& bbPolys) -> bool
	{
		if(!tl2::collide_bounding_boxes(bbCircles, bbPolys))
			return false;

		for(std::size_t idx1 = 0; idx1 < circles.size(); ++idx1)
		{
			const auto& circle = circles[idx1];

			for(std::size_t idx2 = 0; idx2 < polys.size(); ++idx2)
			{
				const auto& poly = polys[idx2];

				if(geo::collide_circle_poly<t_vec2>(
					std::get<0>(circle), std::get<1>(circle),poly))
					return true;
			}
		}

		return false;
	};
	// ------------------------------------------------------------------------


	// get 2d objects from instrument components
	const Axis& mono = GetInstrument().GetMonochromator();
	const Axis& sample = GetInstrument().GetSample();
	const Axis& ana = GetInstrument().GetAnalyser();
	const auto& walls = GetWalls();

	std::vector<std::tuple<t_vec, t_real>>
		monoCircles, monoCirclesIntOut,
		sampleCircles,
		anaCircles;
	std::vector<std::vector<t_vec>>
		monoPolys, monoPolysIn, monoPolysIntOut,
		samplePolys, samplePolysIn,
		anaPolys, anaPolysOut;

	get_circles(mono, monoCircles);
	get_circles(mono, monoCirclesIntOut, false, true, true);
	get_circles(sample, sampleCircles);
	get_circles(ana, anaCircles);

	get_polys(mono, monoPolys);
	get_polys(mono, monoPolysIn, true, false, false);
	get_polys(mono, monoPolysIntOut, false, true, true);
	get_polys(sample, samplePolys);
	get_polys(sample, samplePolysIn, true, false, false);
	get_polys(ana, anaPolys);
	get_polys(ana, anaPolysOut, false, false, true);


	// convert to fixed 2d vectors for efficiency
	auto monoCircles2d = convert_circles_2d(monoCircles);
	auto monoCirclesIntOut2d = convert_circles_2d(monoCirclesIntOut);
	auto sampleCircles2d = convert_circles_2d(sampleCircles);
	auto anaCircles2d = convert_circles_2d(anaCircles);

	auto monoPolys2d = convert_polys_2d(monoPolys);
	auto monoPolysIn2d = convert_polys_2d(monoPolysIn);
	auto monoPolysIntOut2d = convert_polys_2d(monoPolysIntOut);
	auto samplePolys2d = convert_polys_2d(samplePolys);
	auto samplePolysIn2d = convert_polys_2d(samplePolysIn);
	auto anaPolys2d = convert_polys_2d(anaPolys);
	auto anaPolysOut2d = convert_polys_2d(anaPolysOut);


	// get bounding boxes
	auto monoBB = tl2::bounding_box<t_vec2, std::vector>(monoPolys2d, 2);
	auto monoInBB = tl2::bounding_box<t_vec2, std::vector>(monoPolysIn2d, 2);
	auto monoIntOutBB = tl2::bounding_box<t_vec2, std::vector>(monoPolysIntOut2d, 2);
	auto sampleBB = tl2::bounding_box<t_vec2, std::vector>(samplePolys2d, 2);
	auto sampleInBB = tl2::bounding_box<t_vec2, std::vector>(samplePolysIn2d, 2);
	auto anaBB = tl2::bounding_box<t_vec2, std::vector>(anaPolys2d, 2);
	auto anaOutBB = tl2::bounding_box<t_vec2, std::vector>(anaPolysOut2d, 2);

	auto monoCircleBB = tl2::sphere_bounding_box<t_vec2, std::vector>(monoCircles2d, 2);
	auto monoCircleIntOutBB = tl2::sphere_bounding_box<t_vec2, std::vector>(monoCirclesIntOut2d, 2);
	auto sampleCircleBB = tl2::sphere_bounding_box<t_vec2, std::vector>(sampleCircles2d, 2);
	auto anaCircleBB = tl2::sphere_bounding_box<t_vec2, std::vector>(anaCircles2d, 2);


	// check for collisions with the walls
	for(const auto& wall : walls)
	{
		// wall polygons
		std::vector<t_vec> wallPoly;
		get_comp_polys(wall, wallPoly);

		auto wallPoly2d = convert_poly_2d(wallPoly);
		if(wallPoly2d && wallPoly2d->size())
		{
			std::vector<std::vector<t_vec2>> wallPolys2d;
			wallPolys2d.push_back(*wallPoly2d);

			auto wallBB = tl2::bounding_box<t_vec2, std::vector>(wallPolys2d, 2);

			// check for collisions
			// TODO: exclude checks for objects that are already colliding
			//       in the instrument definition file

			if(check_collision_poly_poly(monoPolysIntOut2d, wallPolys2d, monoIntOutBB, wallBB))
				return true;
			if(check_collision_poly_poly(samplePolys2d, wallPolys2d, sampleBB, wallBB))
				return true;
			if(check_collision_poly_poly(anaPolys2d, wallPolys2d, anaBB, wallBB))
				return true;

			if(check_collision_circle_poly(monoCirclesIntOut2d, wallPolys2d, monoCircleIntOutBB, wallBB))
				return true;
			if(check_collision_circle_poly(sampleCircles2d, wallPolys2d, sampleCircleBB, wallBB))
				return true;
			if(check_collision_circle_poly(anaCircles2d, wallPolys2d, anaCircleBB, wallBB))
				return true;
		}


		// wall circles
		std::tuple<t_vec, t_real> wallCircle;
		get_comp_circles(wall, wallCircle);

		auto wallCircle2d = convert_circle_2d(wallCircle);
		if(wallCircle2d && std::get<0>(*wallCircle2d).size())
		{
			std::vector<std::tuple<t_vec2, t_real>> wallCircles2d;
			wallCircles2d.push_back(*wallCircle2d);

			auto wallCirclesBB = tl2::sphere_bounding_box<t_vec2, std::vector>(wallCircles2d, 2);

			if(check_collision_circle_circle(monoCirclesIntOut2d, wallCircles2d))
				return true;
			if(check_collision_circle_circle(sampleCircles2d, wallCircles2d))
				return true;
			if(check_collision_circle_circle(anaCircles2d, wallCircles2d))
				return true;

			if(check_collision_circle_poly(wallCircles2d, monoPolys2d, wallCirclesBB, monoBB))
				return true;
			if(check_collision_circle_poly(wallCircles2d, samplePolys2d, wallCirclesBB, sampleBB))
				return true;
			if(check_collision_circle_poly(wallCircles2d, anaPolys2d, wallCirclesBB, anaBB))
				return true;
		}
	}


	// check for instrument self-collisions
	// circle-circle
	if(check_collision_circle_circle(monoCircles2d, sampleCircles2d))
		return true;
	if(check_collision_circle_circle(sampleCircles2d, anaCircles2d))
		return true;
	if(check_collision_circle_circle(monoCircles2d, anaCircles2d))
		return true;

	// circle-polygon
	if(check_collision_circle_poly(monoCircles2d, anaPolys2d, monoCircleBB, anaBB))
		return true;
	if(check_collision_circle_poly(monoCircles2d, samplePolys2d, monoCircleBB, sampleBB))
		return true;
	if(check_collision_circle_poly(sampleCircles2d, monoPolysIn2d, sampleCircleBB, monoInBB))
		return true;
	if(check_collision_circle_poly(sampleCircles2d, anaPolys2d, sampleCircleBB, anaBB))
		return true;
	if(check_collision_circle_poly(anaCircles2d, monoPolys2d, anaCircleBB, monoBB))
		return true;
	if(check_collision_circle_poly(anaCircles2d, samplePolysIn2d, anaCircleBB, sampleInBB))
		return true;

	// polygon-polygon
	if(check_collision_poly_poly(anaPolys2d, monoPolys2d, anaBB, monoBB))
		return true;
	if(check_collision_poly_poly(samplePolys2d, anaPolysOut2d, sampleBB, anaOutBB))
		return true;

	return false;
}


/**
 * an object is requested to be dragged from the gui
 */
void InstrumentSpace::DragObject(bool drag_start, const std::string& obj,
	t_real x_start, t_real y_start, t_real x, t_real y)
{
	// cases concerning instrument axes
	m_instr.DragObject(drag_start, obj, x_start, y_start, x, y);


	// cases involving walls
	bool wall_dragged = false;

	for(auto& wall : GetWalls())
	{
		if(wall->GetId() != obj)
			continue;

		t_vec pos_startcur = tl2::create<t_vec>({ x_start, y_start });
		t_vec pos_cur = tl2::create<t_vec>({ x, y });

		t_vec pos_obj3 = wall->GetCentre();
		t_vec pos_obj = pos_obj3;
		pos_obj.resize(2);

		if(drag_start)
			m_drag_pos_axis_start = pos_obj;

		t_vec pos_drag = pos_cur - pos_startcur + m_drag_pos_axis_start;
		pos_obj3[0] = pos_drag[0];
		pos_obj3[1] = pos_drag[1];

		wall->SetCentre(pos_obj3);
		wall_dragged = true;
	}


	if(wall_dragged)
	{
		EmitUpdate();
		GetInstrument().EmitUpdate();	// needed to trigger collision detection
	}
}


/**
 * load an instrument space definition from a property tree
 */
std::pair<bool, std::string> InstrumentSpace::load(
	/*const*/ pt::ptree& prop, InstrumentSpace& instrspace, const std::string* filename)
{
	std::string unknown = "<unknown>";
	if(!filename) filename = &unknown;

	// get variables from config file
	std::unordered_map<std::string, std::string> propvars{};

	if(auto vars = prop.get_child_optional(FILE_BASENAME "variables"); vars)
	{
		// iterate variables
		for(const auto &var : *vars)
		{
			const auto& key = var.first;
			std::string val = var.second.get<std::string>("<xmlattr>.value", "");
			//std::cout << key << " = " << val << std::endl;

			propvars.insert(std::make_pair(key, val));
		}

		replace_ptree_values(prop, propvars);
	}

	// load instrument definition
	if(auto instr = prop.get_child_optional(FILE_BASENAME "instrument_space"); instr)
	{
		if(!instrspace.Load(*instr))
			return std::make_pair(false, "Instrument configuration \"" +
				*filename + "\" could not be loaded.");
	}
	else
	{
		return std::make_pair(false, "No instrument definition found in \"" +
			*filename + "\".");
	}

	std::ostringstream timestamp;
	if(auto optTime = prop.get_optional<t_real>(FILE_BASENAME "timestamp"); optTime)
		timestamp << tl2::epoch_to_str(*optTime);;

	return std::make_pair(true, timestamp.str());
}


/**
 * load an instrument space definition from an xml file
 */
std::pair<bool, std::string> InstrumentSpace::load(
	const std::string& filename, InstrumentSpace& instrspace)
{
	if(filename == "" || !fs::exists(fs::path(filename)))
		return std::make_pair(false, "Instrument file \"" + filename + "\" does not exist.");

	// open xml
	std::ifstream ifstr{filename};
	if(!ifstr)
		return std::make_pair(false, "Could not read instrument file \"" + filename + "\".");

	// read xml
	pt::ptree prop;
	pt::read_xml(ifstr, prop);
	// check format and version
	if(auto opt = prop.get_optional<std::string>(FILE_BASENAME "ident");
		!opt || *opt != PROG_IDENT)
	{
		return std::make_pair(false, "Instrument file \"" + filename +
			"\" has invalid identifier.");
	}

	return load(prop, instrspace, &filename);
}


/**
 * get the properties of a geometry object in the instrument space
 */
std::vector<ObjectProperty> InstrumentSpace::GetProperties(const std::string& obj) const
{
	// find the wall with the given id
	if(auto iter = std::find_if(m_walls.begin(), m_walls.end(),
		[&obj](const std::shared_ptr<Geometry>& wall) -> bool
		{
			return wall->GetId() == obj;
		}); iter != m_walls.end())
	{
		return (*iter)->GetProperties();
	}

	// otherwise look for the properties in the instrument
	return m_instr.GetProperties(obj);
}


/**
 * set the properties of a geometry object in the instrument space
 */
std::tuple<bool, std::shared_ptr<Geometry>> InstrumentSpace::SetProperties(
	const std::string& obj, const std::vector<ObjectProperty>& props)
{
	// find the wall with the given id
	if(auto iter = std::find_if(m_walls.begin(), m_walls.end(),
		[&obj](const std::shared_ptr<Geometry>& wall) -> bool
		{
			return wall->GetId() == obj;
		}); iter != m_walls.end())
	{
		(*iter)->SetProperties(props);
		return std::make_tuple(true, *iter);
	}

	// otherwise pass the data on to the instrument
	return m_instr.SetProperties(obj, props);
}
