/**
 * line intersection test program
 * @author Tobias Weber <tweber@ill.fr>
 * @date November-2020 - October-2021
 * @note Forked on 19-apr-2021 from my privately developed "geo" project (https://github.com/t-weber/geo).
 * @license GPLv3, see 'LICENSE' file
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

#include "src/core/mingw_hacks.h"
#include "lines.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	#include <QtGui/QActionGroup>
#else
	#include <QtWidgets/QActionGroup>
#endif
#include <QtCore/QDir>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QGridLayout>
#include <QtSvg/QSvgGenerator>

#include <locale>
#include <memory>
#include <array>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <future>
#include <iostream>

#include "tlibs2/libs/algos.h"
#include "tlibs2/libs/str.h"

#include "src/libs/voronoi_lines.h"

#include <boost/asio.hpp>
namespace asio = boost::asio;

#include <boost/scope_exit.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
namespace ptree = boost::property_tree;


#define GEOTOOLS_SHOW_MESSAGE


LinesScene::LinesScene(QWidget *parent) : QGraphicsScene(parent), m_parent{parent}
{
	ClearRegions();
	ClearGroups();
	ClearVertices();
}


LinesScene::~LinesScene()
{
	if(m_elem_voro)
		delete m_elem_voro;
}


void LinesScene::CreateVoroImage(int width, int height)
{
	// only delete old image and create a new one if the sizes have changed
	if(m_elem_voro && m_elem_voro->width()!=width && m_elem_voro->height()!=height)
	{
		delete m_elem_voro;
		m_elem_voro = nullptr;
	}

	if(!m_elem_voro)
	{
		m_elem_voro = new QImage(width, height, QImage::Format_RGB32);
		m_elem_voro->fill(QColor::fromRgbF(0.95, 0.95, 0.95, 1.));
	}
}


void LinesScene::AddVertex(const QPointF& pos)
{
	Vertex *vertex = new Vertex{pos};
	m_elems_vertices.push_back(vertex);
	addItem(vertex);
}


void LinesScene::AddRegion(std::vector<t_vec>&& region)
{
	m_regions.emplace_back(std::forward<std::vector<t_vec>>(region));
}


void LinesScene::AddGroup(std::pair<std::size_t, std::size_t>&& group)
{
	m_linegroups.emplace_back(std::make_pair(std::get<0>(group)/2, (std::get<1>(group)+1)/2));
	m_vertexgroups.emplace_back(std::forward<std::pair<std::size_t, std::size_t>>(group));
}


void LinesScene::ClearVertices()
{
	for(Vertex* vertex : m_elems_vertices)
	{
		removeItem(vertex);
		delete vertex;
	}
	m_elems_vertices.clear();

	setBackgroundBrush(QBrush{QColor::fromRgbF(0.95, 0.95, 0.95, 1.)});
	if(m_elem_voro)
		m_elem_voro->fill(backgroundBrush().color());

	UpdateAll();
}


void LinesScene::ClearRegions()
{
	m_regions.clear();
}


void LinesScene::ClearGroups()
{
	m_linegroups.clear();
	m_vertexgroups.clear();
}


void LinesScene::MakeRegionsFromGroups()
{
	ClearRegions();

	for(const auto& group : m_vertexgroups)
	{
		std::size_t beg = std::get<0>(group);
		std::size_t end = std::get<1>(group);

		std::vector<t_vec> region;
		region.reserve((end - beg) / 2);

		for(std::size_t vertidx=beg; vertidx<end; vertidx+=2)
		{
			if(vertidx < m_elems_vertices.size())
			{
				//const auto& line = m_lines[vertidx];
				//const t_vec& vec = std::get<0>(line);

				const Vertex* vert = m_elems_vertices[vertidx];
				t_vec vec = tl2::create<t_vec>({ vert->x(), vert->y() });
				//tl2_ops::operator<<(std::cout, vec) << std::endl;

				region.emplace_back(std::move(vec));
			}
		}

		if(region.size())
			AddRegion(std::move(region));
	}
}


void LinesScene::SetIntersectionCalculationMethod(IntersectionCalculationMethod m)
{
	m_intersectioncalculationmethod = m;
	UpdateIntersections();
}


void LinesScene::SetVoronoiCalculationMethod(VoronoiCalculationMethod m)
{
	m_voronoicalculationmethod = m;
	UpdateVoro();
}


void LinesScene::UpdateAll()
{
	// don't send the finished signal for every calculation
	// if we anyway send it in the end
	blockSignals(true);

#ifdef GEOTOOLS_SHOW_MESSAGE
	const std::size_t numLines_old = m_numLines;
#endif

	UpdateLines();
	UpdateIntersections();
	UpdateTrapezoids();
	UpdateVoro();

	blockSignals(false);

#ifdef GEOTOOLS_SHOW_MESSAGE
	// set or reset the initial text
	if((m_numLines == 0 && numLines_old != 0) ||
		(numLines_old == 0 && m_numLines != 0))
		update();
#endif

	emit CalculationFinished();
}


void LinesScene::UpdateLines()
{
	BOOST_SCOPE_EXIT(this_) { emit this_->CalculationFinished(); } BOOST_SCOPE_EXIT_END

	// remove previous lines
	for(QGraphicsItem* item : m_elems_lines)
	{
		removeItem(item);
		delete item;
	}

	// get new lines
	m_elems_lines.clear();
	m_lines.clear();
	m_numLines = 0;

	const std::size_t num_vertices = m_elems_vertices.size();
	if(num_vertices >= 2)
	{
		m_lines.reserve(num_vertices/2);

		for(std::size_t i=0; i<num_vertices-1; i+=2)
		{
			const Vertex* _vert1 = m_elems_vertices[i];
			const Vertex* _vert2 = m_elems_vertices[i+1];

			t_vec vert1 = tl2::create<t_vec>({_vert1->x(), _vert1->y()});
			t_vec vert2 = tl2::create<t_vec>({_vert2->x(), _vert2->y()});

			m_lines.emplace_back(std::make_pair(vert1, vert2));
		}


		QPen penEdge;
		penEdge.setStyle(Qt::SolidLine);
		penEdge.setWidthF(2.);
		penEdge.setColor(QColor::fromRgbF(0., 0., 1.));


		m_elems_lines.reserve(m_lines.size());

		for(const auto& line : m_lines)
		{
			const t_vec& vertex1 = line.first;
			const t_vec& vertex2 = line.second;

			QLineF qline{QPointF{vertex1[0], vertex1[1]}, QPointF{vertex2[0], vertex2[1]}};
			QGraphicsItem *item = addLine(qline, penEdge);
			m_elems_lines.push_back(item);
		}

		m_numLines = m_lines.size();
	}
}


void LinesScene::UpdateIntersections()
{
	BOOST_SCOPE_EXIT(this_) { emit this_->CalculationFinished(); } BOOST_SCOPE_EXIT_END

	// remove previous intersection points
	for(QGraphicsItem* item : m_elems_inters)
	{
		removeItem(item);
		delete item;
	}

	m_elems_inters.clear();
	m_numIntersections = 0;


	// don't calculate if disabled
	if(!m_calcinters)
		return;


	std::vector<std::tuple<std::size_t, std::size_t, t_vec>> intersections;

	switch(m_intersectioncalculationmethod)
	{
		case IntersectionCalculationMethod::DIRECT:
			intersections =
				geo::intersect_ineff<t_vec, std::pair<t_vec, t_vec>>(m_lines, g_eps);
			break;
		case IntersectionCalculationMethod::SWEEP:
			intersections =
				geo::intersect_sweep<t_vec, std::pair<t_vec, t_vec>>(m_lines, g_eps);
			break;
		default:
			QMessageBox::critical(m_parent, "Error", "Unknown intersection calculation method.");
			break;
	};


	QPen pen;
	pen.setStyle(Qt::SolidLine);
	pen.setWidthF(1.);
	pen.setColor(QColor::fromRgbF(0., 0.25, 0.));

	QBrush brush;
	brush.setStyle(Qt::SolidPattern);
	brush.setColor(QColor::fromRgbF(0., 0.75, 0.));


	m_elems_inters.reserve(intersections.size());

	for(const auto& intersection : intersections)
	{
		const t_vec& inters = std::get<2>(intersection);

		const t_real width = 14.;
		QRectF rect{inters[0]-width/2, inters[1]-width/2, width, width};
		QGraphicsItem *item = addEllipse(rect, pen, brush);
		m_elems_inters.push_back(item);
	}


	m_numIntersections = intersections.size();
}


void LinesScene::SetCalculateIntersections(bool b)
{
	m_calcinters = b;
	UpdateIntersections();
}


void LinesScene::SetCalculateTrapezoids(bool b)
{
	m_calctrapezoids = b;
	UpdateTrapezoids();
}


void LinesScene::SetCalculateVoro(bool b)
{
	m_calcvoro = b;
	UpdateVoro();
}


void LinesScene::SetCalculateVoroVertex(bool b)
{
	m_calcvorovertex = b;
	UpdateVoro();
}


void LinesScene::SetStopOnInters(bool b)
{
	m_stoponinters = b;
	UpdateTrapezoids();
	UpdateVoro();
}


void LinesScene::SetRemoveVerticesInRegions(bool b)
{
	m_removeverticesinregions = b;
	UpdateVoro();
}


void LinesScene::SetGroupLines(bool b)
{
	m_grouplines = b;
	UpdateVoro();
}


void LinesScene::UpdateTrapezoids()
{
	BOOST_SCOPE_EXIT(this_) { emit this_->CalculationFinished(); } BOOST_SCOPE_EXIT_END

	// remove previous trapezoids
	for(QGraphicsItem* item : m_elems_trap)
	{
		removeItem(item);
		delete item;
	}
	m_elems_trap.clear();
	m_numTrapezoids = 0;


	// don't calculate if disabled or if there are intersections
	if(!m_calctrapezoids)
		return;
	if(m_stoponinters && m_elems_inters.size())
		return;

	// calculate trapezoids
	bool randomise = true;
	bool shear = true;
	t_real padding = 25.;
	auto node = geo::create_trapezoid_tree<t_vec>(m_lines, randomise, shear, padding, g_eps);
	auto trapezoids = geo::get_trapezoids<t_vec>(node);

	QPen penTrap;
	penTrap.setWidthF(2.);

	for(const auto& trap : trapezoids)
	{
		for(std::size_t idx1=0; idx1<trap.size(); ++idx1)
		{
			std::size_t idx2 = idx1+1;
			if(idx2 >= trap.size())
				idx2 = 0;
			if(idx1 == idx2)
				continue;

			QLineF line
			{
				QPointF{trap[idx1][0], trap[idx1][1]},
				QPointF{trap[idx2][0], trap[idx2][1]}
			};

			QGraphicsItem *item = addLine(line, penTrap);
			m_elems_trap.push_back(item);
		}
	}


	m_numTrapezoids = trapezoids.size();
}


void LinesScene::UpdateVoroImage(const QTransform& trafoSceneToVP)
{
	//BOOST_SCOPE_EXIT(this_) { emit this_->CalculationFinished(); } BOOST_SCOPE_EXIT_END

	QTransform trafoVPToScene = trafoSceneToVP.inverted();

	if(!m_elem_voro)
		return;

	asio::thread_pool tp{g_maxnum_threads};
	std::vector<std::shared_ptr<std::packaged_task<void()>>> packages{};
	std::mutex mtx{};

	const int width = m_elem_voro->width();
	const int height = m_elem_voro->height();
	std::unordered_map<std::size_t, QColor> linecolours;

	QProgressDialog progdlg(m_parent);
	progdlg.setWindowModality(Qt::WindowModal);
	progdlg.setMinimum(0);
	progdlg.setMaximum(height);
	QString msg = QString("Calculating Voronoi regions in %1 threads...").arg(g_maxnum_threads);
	progdlg.setLabel(new QLabel(msg));

	packages.reserve(height);

	for(int y=0; y<height; ++y)
	{
		auto package = std::make_shared<std::packaged_task<void()>>(
			[this, y, width, &linecolours, &mtx, &trafoVPToScene]()
			{
				for(int x=0; x<width; ++x)
				{
					t_real scenex, sceney;
					trafoVPToScene.map(x, y, &scenex, &sceney);

					t_vec pt = tl2::create<t_vec>({scenex, sceney});
					std::size_t lineidx = GetClosestLineIdx(pt);

					// get colour for voronoi region
					QColor col{0xff, 0xff, 0xff, 0xff};

					std::lock_guard<std::mutex> _lck(mtx);
					auto iter = linecolours.find(lineidx);
					if(iter != linecolours.end())
					{
						col = iter->second;
					}
					else
					{
						col.setRgb(
							tl2::get_rand<int>(0,0xff),
							tl2::get_rand<int>(0,0xff),
							tl2::get_rand<int>(0,0xff));

						linecolours.insert(std::make_pair(lineidx, col));
					}

					m_elem_voro->setPixelColor(x, y, col);
				}
			});

		packages.push_back(package);
		asio::post(tp, [package]() { if(package) (*package)(); });
	}

	for(int y=0; y<height; ++y)
	{
		if(progdlg.wasCanceled())
		{
			tp.stop();
			break;
		}

		progdlg.setValue(y);
		if(packages[y])
			packages[y]->get_future().get();
	}

	tp.join();
	progdlg.setValue(m_elem_voro->height());

	setBackgroundBrush(*m_elem_voro);
}


std::size_t LinesScene::GetClosestLineIdx(const t_vec& pt) const
{
	t_real mindist = std::numeric_limits<t_real>::max();
	std::size_t minidx = 0;

	for(std::size_t idx=0; idx<m_lines.size(); ++idx)
	{
		const auto& line = m_lines[idx];

		t_real dist = dist_pt_line(pt, line.first, line.second, false);
		if(dist < mindist)
		{
			mindist = dist;
			minidx = idx;
		}
	}

	return minidx;
}


void LinesScene::UpdateVoro()
{
	BOOST_SCOPE_EXIT(this_) { emit this_->CalculationFinished(); } BOOST_SCOPE_EXIT_END

	using t_line = std::pair<t_vec, t_vec>;

	// remove previous voronoi diagram
	for(QGraphicsItem* item : m_elems_voro)
	{
		removeItem(item);
		delete item;
	}

	m_elems_voro.clear();
	m_numVoronoiVertices = 0;
	m_numVoronoiLinearEdges = 0;
	m_numVoronoiLParabolicEdges = 0;


	// don't calculate if disabled or if there are intersections
	if(!m_calcvoro && !m_calcvorovertex)
		return;
	if(m_stoponinters && m_elems_inters.size())
		return;


	t_real edge_eps = 1e-2;
	geo::VoronoiLinesRegions<t_vec, t_line> regions{};
	geo::VoronoiLinesResults<t_vec, t_line, t_graph> results{};
	regions.SetGroupLines(m_grouplines);
	regions.SetRemoveVoronoiVertices(m_removeverticesinregions);
	regions.SetLineGroups(&m_linegroups);

	switch(m_voronoicalculationmethod)
	{
		case VoronoiCalculationMethod::BOOSTPOLY:
			results = geo::calc_voro<t_vec, t_line, t_graph>(
				m_lines, g_eps, edge_eps, &regions);
			break;
#ifdef USE_CGAL
		case VoronoiCalculationMethod::CGAL:
			results = geo::calc_voro_cgal<t_vec, t_line, t_graph>(
				m_lines, g_eps, edge_eps, &regions);
			break;
#endif
#ifdef USE_OVD
		case VoronoiCalculationMethod::OVD:
			results = geo::calc_voro_ovd<t_vec, t_line, t_graph>(
				m_lines, g_eps, edge_eps, &regions);
			break;
#endif
		default:
			QMessageBox::critical(m_parent, "Error",
				"Unknown voronoi diagram calculation method.");
			break;
	};


	m_elems_voro.reserve(results.GetLinearEdges().size() +
		results.GetParabolicEdges().size() +
		results.GetVoronoiVertices().size());

	// voronoi edges
	if(m_calcvoro)
	{
		// linear voronoi edges
		QPen penLinEdge;
		penLinEdge.setStyle(Qt::SolidLine);
		penLinEdge.setWidthF(1.);
		penLinEdge.setColor(QColor::fromRgbF(0.,0.,0.));

		for(const auto& linear_edge : results.GetLinearEdges())
		{
			const auto& linear_bisector = std::get<1>(linear_edge);

			QLineF line{
				QPointF{std::get<0>(linear_bisector)[0], std::get<0>(linear_bisector)[1]},
				QPointF{std::get<1>(linear_bisector)[0], std::get<1>(linear_bisector)[1]} };

			QGraphicsItem *item = addLine(line, penLinEdge);
			m_elems_voro.push_back(item);
		}

		// parabolic voronoi edges
		QPen penParaEdge = penLinEdge;

		for(const auto& parabolic_edges : results.GetParabolicEdges())
		{
			const auto& parabolic_points = std::get<1>(parabolic_edges);

			QPolygonF poly;
			poly.reserve(parabolic_points.size());
			for(const auto& parabolic_point : parabolic_points)
				poly << QPointF{parabolic_point[0], parabolic_point[1]};

			QPainterPath path;
			path.addPolygon(poly);

			QGraphicsItem *item = addPath(path, penParaEdge);
			m_elems_voro.push_back(item);
		}
	}

	// voronoi vertices
	if(m_calcvorovertex)
	{
		QPen penVertex;
		penVertex.setStyle(Qt::SolidLine);
		penVertex.setWidthF(1.);
		penVertex.setColor(QColor::fromRgbF(0.25, 0., 0.));

		QBrush brushVertex;
		brushVertex.setStyle(Qt::SolidPattern);
		brushVertex.setColor(QColor::fromRgbF(0.75, 0., 0.));

		for(const t_vec& vertex : results.GetVoronoiVertices())
		{
			//tl2_ops::operator<<(std::cout, vertex) << std::endl;
			const t_real width = 8.;
			QRectF rect{vertex[0]-width/2, vertex[1]-width/2, width, width};
			QGraphicsItem *item = addEllipse(rect, penVertex, brushVertex);
			m_elems_voro.push_back(item);
		}
	}


	m_numVoronoiVertices = results.GetVoronoiVertices().size();
	m_numVoronoiLinearEdges = results.GetLinearEdges().size();
	m_numVoronoiLParabolicEdges = results.GetParabolicEdges().size();
}

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------

LinesView::LinesView(LinesScene *scene, QWidget *parent)
	: QGraphicsView(scene, parent), m_scene{scene}
{
	setCacheMode(QGraphicsView::CacheBackground);

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	setInteractive(true);
	setMouseTracking(true);

	//setSceneRect(mapToScene(viewport()->rect()).boundingRect());
}


LinesView::~LinesView()
{
}


void LinesView::UpdateAll()
{
	// triggers updates
	QResizeEvent evt{size(), size()};
	resizeEvent(&evt);
}


void LinesView::resizeEvent(QResizeEvent *evt)
{
	int widthView = evt->size().width();
	int heightView = evt->size().height();

	QPointF pt1{mapToScene(QPoint{0,0})};
	QPointF pt2{mapToScene(QPoint{widthView, heightView})};

	// include bounds given by vertices
	const t_real padding = 16;
	for(const Vertex* vertex : m_scene->GetVertexElems())
	{
		QPointF vertexpos = vertex->scenePos();

		if(vertexpos.x() < pt1.x())
			pt1.setX(vertexpos.x() - padding);
		if(vertexpos.x() > pt2.x())
			pt2.setX(vertexpos.x() + padding);
		if(vertexpos.y() < pt1.y())
			pt1.setY(vertexpos.y() - padding);
		if(vertexpos.y() > pt2.y())
			pt2.setY(vertexpos.y() + padding);
	}

	setSceneRect(QRectF{pt1, pt2});
	m_scene->CreateVoroImage(widthView, heightView);

	//QGraphicsView::resizeEvent(evt);
}



void LinesView::mousePressEvent(QMouseEvent *evt)
{
	QPoint posVP = evt->pos();
	QPointF posScene = mapToScene(posVP);

	QList<QGraphicsItem*> items = this->items(posVP);
	QGraphicsItem* item = nullptr;
	bool item_is_vertex = false;
	auto &verts = m_scene->GetVertexElems();
	decltype(verts.begin()) vertiter;

	for(int itemidx=0; itemidx<items.size(); ++itemidx)
	{
		item = items[itemidx];
		vertiter = std::find(verts.begin(), verts.end(), static_cast<Vertex*>(item));
		item_is_vertex = (vertiter != verts.end());
		if(item_is_vertex)
			break;
	}

	// only select vertices
	if(!item_is_vertex)
		item = nullptr;


	if(evt->button() == Qt::LeftButton)
	{
		// if no vertex is at this position, create a new one
		if(!item)
		{
			m_scene->AddVertex(posScene);
			m_dragging = true;

			m_scene->UpdateAll();
		}

		else
		{
			// vertex is being dragged
			if(item_is_vertex)
			{
				m_dragging = true;
			}
		}
	}
	else if(evt->button() == Qt::RightButton)
	{
		// if a vertex is at this position, remove it
		if(item && item_is_vertex && vertiter != verts.end())
		{
			m_scene->removeItem(item);
			vertiter = verts.erase(vertiter);
			delete item;

			// move remaining vertex of line to the end
			std::size_t idx = vertiter - verts.begin();
			std::size_t otheridx = (idx % 2 == 0 ? idx : idx-1);

			if(otheridx < verts.size())
			{
				Vertex* vert = verts[otheridx];
				verts.erase(verts.begin()+otheridx);
				verts.push_back(vert);
			}

			m_scene->UpdateAll();
		}
	}

	QGraphicsView::mousePressEvent(evt);
}


void LinesView::mouseReleaseEvent(QMouseEvent *evt)
{
	if(evt->button() == Qt::LeftButton)
		m_dragging = false;

	m_scene->UpdateAll();

	QGraphicsView::mouseReleaseEvent(evt);
}


void LinesView::mouseMoveEvent(QMouseEvent *evt)
{
	QGraphicsView::mouseMoveEvent(evt);

	if(m_dragging)
	{
		UpdateAll();
		m_scene->UpdateAll();
	}

	QPoint posVP = evt->pos();
	QPointF posScene = mapToScene(posVP);


	// get the regions the cursor is in
	std::vector<std::size_t> cursor_regions;
	for(std::size_t regionidx=0; regionidx<m_scene->GetRegions().size(); ++regionidx)
	{
		const auto& region = m_scene->GetRegions()[regionidx];

		auto vec = tl2::create<typename LinesScene::t_vec>({ posScene.x(), posScene.y() });
		bool in_region = geo::pt_inside_poly<typename LinesScene::t_vec>(
			region, vec, nullptr, g_eps);

		if(in_region)
			cursor_regions.push_back(regionidx);
	}


	// get the vertices the cursor is on (if any)
	QList<QGraphicsItem*> items = this->items(posVP);
	const auto &verts = m_scene->GetVertexElems();
	std::vector<std::size_t> vert_indices;

	for(int itemidx=0; itemidx<items.size(); ++itemidx)
	{
		QGraphicsItem* item = items[itemidx];
		auto iter = std::find(verts.begin(), verts.end(), static_cast<Vertex*>(item));
		if(iter != verts.end())
			vert_indices.push_back(iter - verts.begin());
	}


	emit SignalMouseCoordinates(posScene.x(), posScene.y(), posVP.x(), posVP.y(), cursor_regions, vert_indices);
}


void LinesView::wheelEvent(QWheelEvent *evt)
{
	//t_real s = std::pow(2., evt->angleDelta().y() / 1000.);
	//scale(s, s);
	QGraphicsView::wheelEvent(evt);
}


void LinesView::drawBackground(QPainter* painter, const QRectF& rect)
{
	QGraphicsView::drawBackground(painter, rect);

	// hack, because the background brush is drawn with respect to scene (0,0), not vp (0,0)
	// TODO: handle scene-viewport trafos other than translations
	if(m_scene->GetVoroImage())
		painter->drawImage(mapToScene(QPoint(0,0)), *m_scene->GetVoroImage());
}



void LinesView::drawForeground(QPainter* painter, const QRectF& rect)
{
	QGraphicsView::drawForeground(painter, rect);

#ifdef GEOTOOLS_SHOW_MESSAGE
	if(!m_scene->GetNumLines())
	{
		QFont font = painter->font();
		font.setBold(true);

		QString msg{"Click to place line segments."};
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		int msg_width = QFontMetrics{font}.horizontalAdvance(msg);
#else
		int msg_width = QFontMetrics{font}.width(msg);
#endif

		QRect rectVP = viewport()->rect();

		painter->setFont(font);
		painter->drawText(
			rectVP.width()/2 - msg_width/2,
			rectVP.height()/2, msg);
	}
#endif
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------

LinesWnd::LinesWnd(QWidget* pParent) : QDialog{pParent},
	m_scene{new LinesScene{this}},
	m_view{new LinesView{m_scene.get(), this}},
	m_statusLabel{std::make_shared<QLabel>(this)}
{
#ifdef TASPATHS_TOOLS_STANDALONE
	// set-up common gui variables
	GeoSettingsDlg::SetGuiTheme(&g_theme);
	GeoSettingsDlg::SetGuiFont(&g_font);
	GeoSettingsDlg::SetGuiUseNativeMenubar(&g_use_native_menubar);
	GeoSettingsDlg::SetGuiUseNativeDialogs(&g_use_native_dialogs);

	// restore settings
	GeoSettingsDlg::ReadSettings(&m_sett);
#endif

	m_view->setRenderHints(QPainter::Antialiasing);

	setWindowTitle("Line Segments");

	QGridLayout *layout = new QGridLayout(this);
	layout->setSpacing(6);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->addWidget(m_view.get(), 0, 0, 1, 1);

	m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

	QStatusBar *statusBar = new QStatusBar{this};
	statusBar->addPermanentWidget(m_statusLabel.get(), 1);
	statusBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	layout->addWidget(statusBar, 1, 0, 1, 1);


	// file menu
	QAction *actionNew = new QAction{QIcon::fromTheme("document-new"), "New", this};
	connect(actionNew, &QAction::triggered, this, &LinesWnd::NewFile);

	QAction *actionLoad = new QAction{QIcon::fromTheme("document-open"), "Open...", this};
	connect(actionLoad, &QAction::triggered, this,
		static_cast<void (LinesWnd::*)()>(&LinesWnd::OpenFile));

	QAction *actionSave = new QAction{QIcon::fromTheme("document-save"), "Save", this};
	connect(actionSave, &QAction::triggered, this,
		static_cast<void (LinesWnd::*)()>(&LinesWnd::SaveFile));

	QAction *actionSaveAs = new QAction{QIcon::fromTheme("document-save-as"), "Save as...", this};
	connect(actionSaveAs, &QAction::triggered, this, &LinesWnd::SaveFileAs);

	QAction *actionExportSvg = new QAction{QIcon::fromTheme("image-x-generic"), "Export SVG...", this};
	connect(actionExportSvg, &QAction::triggered, [this]()
	{
		QString dirLast = m_sett.value("cur_image_dir", QDir::homePath()).toString();

		if(QString file = QFileDialog::getSaveFileName(this,
			"Export SVG", dirLast+"/untitled.svg",
			"SVG Files (*.svg);;All Files (* *.*)"); file!="")
		{
			QSvgGenerator svggen;
			svggen.setSize(QSize{width(), height()});
			svggen.setFileName(file);

			QPainter paint(&svggen);
			m_scene->render(&paint);
		}
	});

	QAction *actionExportGraph = new QAction{"Export Voronoi Graph...", this};
	connect(actionExportGraph, &QAction::triggered, [this]()
	{
		QString dirLast = m_sett.value("cur_dir", QDir::homePath()).toString();

		if(QString file = QFileDialog::getSaveFileName(this,
			"Export DOT", dirLast+"/untitled.dot",
			"DOT Files (*.dot);;All Files (* *.*)"); file!="")
		{
			const auto& graph = m_scene->GetVoroGraph();

			std::ofstream ofstr(file.toStdString());
			print_graph(graph, ofstr);
			ofstr << std::endl;
		}
	});

#ifdef TASPATHS_TOOLS_STANDALONE
	QAction *actionSettings = new QAction(QIcon::fromTheme("preferences-system"), "Settings...", this);
	actionSettings->setMenuRole(QAction::PreferencesRole);
	connect(actionSettings, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgSettings)
		{
			this->m_dlgSettings = std::make_shared<GeoSettingsDlg>(
				this, &m_sett);
		}

		m_dlgSettings->show();
		m_dlgSettings->raise();
		m_dlgSettings->activateWindow();
	});
#endif

#ifdef TASPATHS_TOOLS_STANDALONE
	QAction *actionQuit = new QAction{QIcon::fromTheme("application-exit"), "Quit", this};
	actionQuit->setMenuRole(QAction::QuitRole);
#else
	QAction *actionQuit = new QAction{QIcon::fromTheme("window-close"), "Close", this};
#endif
	connect(actionQuit, &QAction::triggered, [this]() { this->close(); });


	// view menu
	QAction *actionZoomIn = new QAction{QIcon::fromTheme("zoom-in"), "Zoom in", this};
	connect(actionZoomIn, &QAction::triggered, [this]()
	{
		if(!m_view)
			return;
		m_view->scale(2., 2.);
	});

	QAction *actionZoomOut = new QAction{QIcon::fromTheme("zoom-out"), "Zoom out", this};
	connect(actionZoomOut, &QAction::triggered, [this]()
	{
		if(!m_view)
			return;
		m_view->scale(0.5, 0.5);
	});

	QAction *actionIncreaseVertexSize = new QAction{"Increase Vertex Size", this};
	connect(actionIncreaseVertexSize, &QAction::triggered, [this]()
	{
		if(!m_scene)
			return;

		for(Vertex* vert : m_scene->GetVertexElems())
		{
			t_real rad = vert->GetRadius();
			rad *= t_real(2.);
			vert->SetRadius(rad);
		}

		m_scene->update(QRectF());
	});

	QAction *actionDecreaseVertexSize = new QAction{"Decrease Vertex Size", this};
	connect(actionDecreaseVertexSize, &QAction::triggered, [this]()
	{
		if(!m_scene)
			return;

		for(Vertex* vert : m_scene->GetVertexElems())
		{
			t_real rad = vert->GetRadius();
			rad *= t_real(0.5);
			vert->SetRadius(rad);
		}

		m_scene->update(QRectF());
	});

	QAction *actionInfos = new QAction{"Infos...", this};
	connect(actionInfos, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgInfo)
		{
			this->m_dlgInfo = std::make_shared<GeoInfoDlg>(this, &m_sett);
			connect(m_scene.get(), &LinesScene::CalculationFinished,
				this, &LinesWnd::UpdateInfos);

			this->UpdateInfos();
		}

		m_dlgInfo->show();
		m_dlgInfo->raise();
		m_dlgInfo->activateWindow();
	});


	// calculate menu
	QAction *actionIntersections = new QAction{"Intersections", this};
	actionIntersections->setCheckable(true);
	actionIntersections->setChecked(m_scene->GetCalculateIntersections());
	connect(actionIntersections, &QAction::toggled, [this](bool b)
	{ m_scene->SetCalculateIntersections(b); });

	QAction *actionVoronoiRegions = new QAction{"Voronoi Bisectors", this};
	actionVoronoiRegions->setCheckable(true);
	actionVoronoiRegions->setChecked(m_scene->GetCalculateVoro());
	connect(actionVoronoiRegions, &QAction::toggled, [this](bool b)
	{ m_scene->SetCalculateVoro(b); });

	QAction *actionVoronoiVertices = new QAction{"Voronoi Vertices", this};
	actionVoronoiVertices->setCheckable(true);
	actionVoronoiVertices->setChecked(m_scene->GetCalculateVoroVertex());
	connect(actionVoronoiVertices, &QAction::toggled, [this](bool b)
	{ m_scene->SetCalculateVoroVertex(b); });

	QAction *actionVoroBitmap = new QAction{"Voronoi Regions", this};
	connect(actionVoroBitmap, &QAction::triggered, [this]()
	{ m_scene->UpdateVoroImage(m_view->viewportTransform()); });

	QAction *actionTrap = new QAction{"Trapezoid Map", this};
	actionTrap->setCheckable(true);
	actionTrap->setChecked(m_scene->GetCalculateTrapezoids());
	connect(actionTrap, &QAction::toggled, [this](bool b)
	{ m_scene->SetCalculateTrapezoids(b); });


	// options menu
	QAction *actionIntersDirect = new QAction{"Direct", this};
	actionIntersDirect->setCheckable(true);
	actionIntersDirect->setChecked(m_scene->GetIntersectionCalculationMethod() == IntersectionCalculationMethod::DIRECT);
	connect(actionIntersDirect, &QAction::toggled, [this]()
	{ m_scene->SetIntersectionCalculationMethod(IntersectionCalculationMethod::DIRECT); });

	QAction *actionIntersSweep = new QAction{"Sweep", this};
	actionIntersSweep->setCheckable(true);
	actionIntersSweep->setChecked(m_scene->GetIntersectionCalculationMethod() == IntersectionCalculationMethod::SWEEP);
	connect(actionIntersSweep, &QAction::toggled, [this]()
	{ m_scene->SetIntersectionCalculationMethod(IntersectionCalculationMethod::SWEEP); });

	QActionGroup *groupInters = new QActionGroup{this};
	groupInters->addAction(actionIntersDirect);
	groupInters->addAction(actionIntersSweep);


	QAction *actionVoroBoost = new QAction{"Boost.Polygon", this};
	actionVoroBoost->setCheckable(true);
	actionVoroBoost->setChecked(m_scene->GetVoronoiCalculationMethod() == VoronoiCalculationMethod::BOOSTPOLY);
	connect(actionVoroBoost, &QAction::toggled, [this]()
	{ m_scene->SetVoronoiCalculationMethod(VoronoiCalculationMethod::BOOSTPOLY); });

	QAction *actionVoroCGAL = new QAction{"CGAL/Segment Delaunay Graph", this};
	actionVoroCGAL->setCheckable(true);
	actionVoroCGAL->setChecked(m_scene->GetVoronoiCalculationMethod() == VoronoiCalculationMethod::CGAL);
	connect(actionVoroCGAL, &QAction::toggled, [this]()
	{ m_scene->SetVoronoiCalculationMethod(VoronoiCalculationMethod::CGAL); });
#if !defined(USE_CGAL)
	actionVoroCGAL->setEnabled(false);
#endif

	QAction *actionVoroOVD = new QAction{"OpenVoronoi", this};
	actionVoroOVD->setCheckable(true);
	actionVoroOVD->setChecked(false);
	connect(actionVoroOVD, &QAction::toggled, [this]()
	{ m_scene->SetVoronoiCalculationMethod(VoronoiCalculationMethod::OVD); });
#if !defined(USE_OVD)
	actionVoroOVD->setEnabled(false);
#endif

	QActionGroup *groupVoro = new QActionGroup{this};
	groupVoro->addAction(actionVoroBoost);
	groupVoro->addAction(actionVoroCGAL);
	groupVoro->addAction(actionVoroOVD);


	QAction *actionStopOnInters = new QAction{"Stop on Intersections", this};
	actionStopOnInters->setCheckable(true);
	actionStopOnInters->setChecked(m_scene->GetStopOnInters());
	connect(actionStopOnInters, &QAction::toggled, [this](bool b)
	{ m_scene->SetStopOnInters(b); });

	QAction *actionGroupLines = new QAction{"Group Lines", this};
	actionGroupLines->setCheckable(true);
	actionGroupLines->setChecked(m_scene->GetGroupLines());
	connect(actionGroupLines, &QAction::toggled, [this](bool b)
	{ m_scene->SetGroupLines(b); });

	QAction *actionRemoveVerticesInRegions = new QAction{"Remove Vertices in Regions", this};
	actionRemoveVerticesInRegions->setCheckable(true);
	actionRemoveVerticesInRegions->setChecked(m_scene->GetRemoveVerticesInRegions());
	connect(actionRemoveVerticesInRegions, &QAction::toggled, [this](bool b)
	{ m_scene->SetRemoveVerticesInRegions(b); });

	QAction *actionAboutQt = new QAction(QIcon::fromTheme("help-about"), "About Qt Libraries...", this);
	QAction *actionAbout = new QAction(QIcon::fromTheme("help-about"), "About this Program...", this);

	actionAboutQt->setMenuRole(QAction::AboutQtRole);
	actionAbout->setMenuRole(QAction::AboutRole);

	connect(actionAboutQt, &QAction::triggered, this, []() { qApp->aboutQt(); });

	connect(actionAbout, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgAbout)
			this->m_dlgAbout = std::make_shared<GeoAboutDlg>(this, &m_sett);

		m_dlgAbout->show();
		m_dlgAbout->raise();
		m_dlgAbout->activateWindow();
	});


	// shortcuts
	actionNew->setShortcut(QKeySequence::New);
	actionLoad->setShortcut(QKeySequence::Open);
	actionSave->setShortcut(QKeySequence::Save);
	actionSaveAs->setShortcut(QKeySequence::SaveAs);
#ifdef TASPATHS_TOOLS_STANDALONE
	actionSettings->setShortcut(QKeySequence::Preferences);
	actionQuit->setShortcut(QKeySequence::Quit);
#else
	actionQuit->setShortcut(QKeySequence::Close);
#endif
	actionZoomIn->setShortcut(QKeySequence::ZoomIn);
	actionZoomOut->setShortcut(QKeySequence::ZoomOut);


	// menu
	QMenu *menuFile = new QMenu{"File", this};
	QMenu *menuView = new QMenu{"View", this};
	QMenu *menuCalc = new QMenu{"Calculate", this};
	QMenu *menuOptions = new QMenu{"Options", this};
	QMenu *menuInters = new QMenu{"Intersection Backends", this};
	QMenu *menuVoro = new QMenu{"Voronoi Diagram Backends", this};
	QMenu *menuHelp = new QMenu("Help", this);


	// recent files
	m_menuOpenRecent = new QMenu("Open Recent", menuFile);
	m_menuOpenRecent->setIcon(QIcon::fromTheme("document-open-recent"));

	m_recent.SetRecentFilesMenu(m_menuOpenRecent);
	m_recent.SetMaxRecentFiles(g_maxnum_recents);
	m_recent.SetOpenFunc(&m_open_func);


	// menu items
	menuFile->addAction(actionNew);
	menuFile->addSeparator();
	menuFile->addAction(actionLoad);
	menuFile->addMenu(m_menuOpenRecent);
	menuFile->addSeparator();
	menuFile->addAction(actionSave);
	menuFile->addAction(actionSaveAs);
	menuFile->addSeparator();
	menuFile->addAction(actionExportSvg);
	menuFile->addAction(actionExportGraph);
	menuFile->addSeparator();
#ifdef TASPATHS_TOOLS_STANDALONE
	menuFile->addAction(actionSettings);
	menuFile->addSeparator();
#endif
	menuFile->addAction(actionQuit);

	menuView->addAction(actionZoomIn);
	menuView->addAction(actionZoomOut);
	menuView->addSeparator();
	menuView->addAction(actionIncreaseVertexSize);
	menuView->addAction(actionDecreaseVertexSize);
	menuView->addSeparator();
	menuView->addAction(actionInfos);

	menuCalc->addAction(actionIntersections);
	menuCalc->addSeparator();
	menuCalc->addAction(actionVoronoiRegions);
	menuCalc->addAction(actionVoronoiVertices);
	menuCalc->addSeparator();
	menuCalc->addAction(actionTrap);
	menuCalc->addSeparator();
	menuCalc->addAction(actionVoroBitmap);

	menuInters->addAction(actionIntersDirect);
	menuInters->addAction(actionIntersSweep);
	menuVoro->addAction(actionVoroBoost);
	menuVoro->addAction(actionVoroCGAL);
	menuVoro->addAction(actionVoroOVD);

	menuOptions->addAction(actionStopOnInters);
	menuOptions->addSeparator();
	menuOptions->addAction(actionGroupLines);
	menuOptions->addAction(actionRemoveVerticesInRegions);
	menuOptions->addSeparator();
	menuOptions->addMenu(menuInters);
	menuOptions->addMenu(menuVoro);

	menuHelp->addAction(actionAboutQt);
	menuHelp->addSeparator();
	menuHelp->addAction(actionAbout);


	// menu bar
	QMenuBar *menuBar = new QMenuBar{this};
	//menuBar->setNativeMenuBar(false);
	menuBar->addMenu(menuFile);
	menuBar->addMenu(menuView);
	menuBar->addMenu(menuCalc);
	menuBar->addMenu(menuOptions);
	menuBar->addMenu(menuHelp);
	layout->setMenuBar(menuBar);


	// ------------------------------------------------------------------------
	// restore settings
	if(m_sett.contains("lines_wnd_geo"))
	{
		QByteArray arr{m_sett.value("lines_wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		resize(800, 600);
	}

	// recent files
	if(m_sett.contains("lines_recent_files"))
		m_recent.SetRecentFiles(m_sett.value("lines_recent_files").toStringList());
	// ------------------------------------------------------------------------


	// connections
	connect(m_view.get(), &LinesView::SignalMouseCoordinates,
	[this](t_real x, t_real y, t_real vpx, t_real vpy, 
		const std::vector<std::size_t>& cursor_regions,
		const std::vector<std::size_t>& vert_indices)
	{
		QString cursormsg = QString("Scene: x=%1, y=%2, Viewport: x=%3, y=%4.")
			.arg(x, 5).arg(y, 5).arg(vpx, 5).arg(vpy, 5);

		QString regionmsg;
		if(cursor_regions.size())
		{
			regionmsg = QString(" Cursor is in region ");
			for(std::size_t regionidx=0; regionidx<cursor_regions.size(); ++regionidx)
			{
				regionmsg += QString::number(cursor_regions[regionidx]);
				if(regionidx < cursor_regions.size()-1)
					regionmsg += ", ";
			}
			regionmsg += ".";
		}

		QString vertmsg;
		if(vert_indices.size())
		{
			vertmsg = QString(" Cursor is on vertices ");
			for(std::size_t vertidx=0; vertidx<vert_indices.size(); ++vertidx)
			{
				vertmsg += QString::number(vert_indices[vertidx]);
				if(vertidx < vert_indices.size()-1)
					vertmsg += ", ";
			}
			vertmsg += ".";
		}

		SetStatusMessage(cursormsg + regionmsg + vertmsg);
	});


	SetStatusMessage("Ready.");
}


/**
 * File -> New
 */
void LinesWnd::NewFile()
{
	SetCurrentFile("");

	m_scene->ClearRegions();
	m_scene->ClearGroups();
	m_scene->ClearVertices();
}


/**
 * open file
 */
bool LinesWnd::OpenFile(const QString& file)
{
	std::ifstream ifstr(file.toStdString());
	if(!ifstr)
	{
		QMessageBox::critical(this, "Error", "File could not be opened for loading.");
		return false;
	}

	m_scene->ClearRegions();
	m_scene->ClearGroups();
	m_scene->ClearVertices();

	ptree::ptree prop{};
	ptree::read_xml(ifstr, prop);

	// read vertices
	std::size_t vertidx = 0;
	while(true)
	{
		std::ostringstream ostrVert;
		ostrVert << "lines2d.vertices." << vertidx;

		auto vertprop = prop.get_child_optional(ostrVert.str());
		if(!vertprop)
			break;

		auto vertx = vertprop->get_optional<t_real>("<xmlattr>.x");
		auto verty = vertprop->get_optional<t_real>("<xmlattr>.y");

		if(!vertx || !verty)
			break;

		m_scene->AddVertex(QPointF{*vertx, *verty});
		++vertidx;
	}

	// read groups
	std::size_t groupidx = 0;
	while(true)
	{
		std::ostringstream ostrGroup;
		ostrGroup << "lines2d.groups." << groupidx;

		auto groupprop = prop.get_child_optional(ostrGroup.str());
		if(!groupprop)
			break;

		auto beg = groupprop->get_optional<t_real>("begin");
		auto end = groupprop->get_optional<t_real>("end");

		if(beg && end)
		{
			auto group = std::make_pair(*beg, *end);
			m_scene->AddGroup(std::move(group));
		}

		++groupidx;
	}

	m_scene->MakeRegionsFromGroups();

	// read regions (if groups is not defined)
	std::size_t regionidx = 0;
	while(true)
	{
		std::ostringstream ostrRegion;
		ostrRegion << "lines2d.regions." << regionidx;

		auto regionprop = prop.get_child_optional(ostrRegion.str());
		if(!regionprop)
			break;

		if(regionidx==0 && m_scene->GetRegions().size())
		{
			std::cerr << "Warning: A vertex group is already defined, ignoring regions." << std::endl;
			break;
		}

		std::size_t regionvertidx = 0;
		std::vector<typename LinesScene::t_vec> region;
		while(true)
		{
			std::ostringstream ostrVert;
			ostrVert << regionvertidx;

			auto vertprop = regionprop->get_child_optional(ostrVert.str());
			if(!vertprop)
				break;

			auto vertx = vertprop->get_optional<t_real>("<xmlattr>.x");
			auto verty = vertprop->get_optional<t_real>("<xmlattr>.y");

			if(!vertx || !verty)
				break;

			region.emplace_back(tl2::create<LinesScene::t_vec>({ *vertx, *verty }));
			++regionvertidx;
		}

		//std::reverse(region.begin(), region.end());
		m_scene->AddRegion(std::move(region));
		++regionidx;
	}

	if(vertidx > 0)
	{
		m_view->UpdateAll();
		m_scene->UpdateAll();

		SetCurrentFile(file);
		m_recent.AddRecentFile(file);

		m_sett.setValue("cur_dir", QFileInfo(file).path());
	}
	else
	{
		QMessageBox::warning(this, "Warning", "File contains no data.");
		return false;
	}

	return true;
}


/**
 * File -> Open
 */
void LinesWnd::OpenFile()
{
	QString dirLast = m_sett.value("cur_dir", QDir::homePath()).toString();

	if(QString file = QFileDialog::getOpenFileName(this,
		"Open Data", dirLast,
		"XML Files (*.xml);;All Files (* *.*)"); file!="")
	{
		OpenFile(file);
	}
}


/**
 * save file
 */
bool LinesWnd::SaveFile(const QString& file)
{
	std::ofstream ofstr(file.toStdString());
	if(!ofstr)
	{
		QMessageBox::critical(this, "Error", "File could not be opened for saving.");
		return false;
	}

	ptree::ptree prop{};
	prop.put("lines2d.ident", "takin_taspaths_lines");
	prop.put("lines2d.doi", "https://doi.org/10.5281/zenodo.4625649");
	prop.put("lines2d.timestamp", tl2::var_to_str(tl2::epoch<t_real>()));

	std::size_t vertidx = 0;
	for(const Vertex* vertex : m_scene->GetVertexElems())
	{
		QPointF vertexpos = vertex->scenePos();

		std::ostringstream ostrX, ostrY;
		ostrX << "lines2d.vertices." << vertidx << ".<xmlattr>.x";
		ostrY << "lines2d.vertices." << vertidx << ".<xmlattr>.y";

		prop.put<t_real>(ostrX.str(), vertexpos.x());
		prop.put<t_real>(ostrY.str(), vertexpos.y());

		++vertidx;
	}

	ptree::write_xml(ofstr, prop, ptree::xml_writer_make_settings('\t', 1, std::string{"utf-8"}));

	SetCurrentFile(file);
	m_recent.AddRecentFile(file);
	m_sett.setValue("cur_dir", QFileInfo(file).path());

	return true;
}


/**
 * File -> Save
 */
void LinesWnd::SaveFile()
{
	if(m_recent.GetCurFile() == "")
		SaveFileAs();
	else
		SaveFile(m_recent.GetCurFile());
}


/**
 * File -> Save As
 */
void LinesWnd::SaveFileAs()
{
	QString dirLast = m_sett.value("cur_dir", QDir::homePath()).toString();

	if(QString file = QFileDialog::getSaveFileName(this,
		"Save Data", dirLast+"/untitled.xml",
		"XML Files (*.xml);;All Files (* *.*)"); file!="")
	{
		SaveFile(file);
	}
}


/**
 * remember current file and set window title
 */
void LinesWnd::SetCurrentFile(const QString &file)
{
	m_recent.SetCurFile(file);
	this->setWindowFilePath(m_recent.GetCurFile());

	/*static const QString title(PROG_TITLE);
	if(m_recent.GetCurFile() == "")
		this->setWindowTitle(title);
	else
		this->setWindowTitle(title + " \u2014 " + m_recent.GetCurFile());*/
}



/**
 * update the text on the status line
 */
void LinesWnd::SetStatusMessage(const QString& msg)
{
	if(!m_statusLabel)
		return;

	m_statusLabel->setText(msg);
}


/**
 * update the text in the info dialog
 */
void LinesWnd::UpdateInfos()
{
	if(!m_dlgInfo || !m_scene)
		return;

	std::ostringstream ostr;
	ostr << "Number of line segments: " << m_scene->GetNumLines() << ".\n";
	ostr << "Number of Voronoi vertices: " << m_scene->GetNumVoronoiVertices() << ".\n";
	ostr << "Number of linear Voronoi bisectors: " << m_scene->GetNumVoronoiLinearEdges() << ".\n";
	ostr << "Number of quadratic Voronoi bisectors: " << m_scene->GetNumVoronoiParabolicEdges() << ".\n";
	ostr << "Number of intersections: " << m_scene->GetNumIntersections() << ".\n";
	ostr << "Number of trapezoids: " << m_scene->GetNumTrapezoids() << ".\n";

	m_dlgInfo->SetInfo(ostr.str().c_str());
}


void LinesWnd::closeEvent(QCloseEvent *e)
{
	// save settings
	QByteArray geo{this->saveGeometry()};
	m_sett.setValue("lines_wnd_geo", geo);

	// save recent files
	m_recent.TrimEntries();
	m_sett.setValue("lines_recent_files", m_recent.GetRecentFiles());

	QDialog::closeEvent(e);
}


LinesWnd::~LinesWnd()
{
}

// ----------------------------------------------------------------------------
