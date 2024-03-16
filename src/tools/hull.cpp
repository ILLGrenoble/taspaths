/**
 * convex hull test program
 * @author Tobias Weber <tweber@ill.fr>
 * @date 15-Aug-2020
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

#include "hull.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	#include <QtGui/QActionGroup>
#else
	#include <QtWidgets/QActionGroup>
#endif
#include <QtCore/QDir>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtSvg/QSvgGenerator>

#include <locale>
#include <memory>
#include <array>
#include <vector>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
namespace ptree = boost::property_tree;

#define USE_BOOST_GRAPH
#include "src/libs/hull.h"
#include "src/libs/voronoi.h"
#include "src/libs/graphs.h"

#include "tlibs2/libs/algos.h"
#include "tlibs2/libs/str.h"
#include "tlibs2/libs/qt/numerictablewidgetitem.h"


#define GEOTOOLS_SHOW_MESSAGE


HullScene::HullScene(QWidget* parent) : QGraphicsScene(parent), m_parent{parent}
{
}


HullScene::~HullScene()
{
}


void HullScene::AddVertex(const QPointF& pos)
{
	Vertex *vertex = new Vertex{pos};
	m_vertices.insert(vertex);
	addItem(vertex);
}



void HullScene::SetCalculateHull(bool b)
{
	m_calchull = b;
	UpdateHull();
}


void HullScene::SetCalculateVoronoiVertices(bool b)
{
	m_calcvoronoivertices = b;
	UpdateDelaunay();
}


void HullScene::SetCalculateVoronoiRegions(bool b)
{
	m_calcvoronoiregions = b;
	UpdateDelaunay();
}


void HullScene::SetCalculateDelaunay(bool b)
{
	m_calcdelaunay = b;
	UpdateDelaunay();
}


void HullScene::SetCalculateKruskal(bool b)
{
	m_calckruskal = b;
	UpdateDelaunay();
}


void HullScene::SetHullCalculationMethod(HullCalculationMethod m)
{
	m_hullcalculationmethod = m;
	UpdateHull();
}


void HullScene::SetDelaunayCalculationMethod(DelaunayCalculationMethod m)
{
	m_delaunaycalculationmethod = m;
	UpdateDelaunay();
}


void HullScene::SetSpanCalculationMethod(SpanCalculationMethod m)
{
	m_spancalculationmethod = m;
	UpdateDelaunay();
}


void HullScene::ClearVertices()
{
	for(Vertex* vertex : m_vertices)
	{
		removeItem(vertex);
		delete vertex;
	}
	m_vertices.clear();

	UpdateAll();
}


void HullScene::UpdateAll()
{
	UpdateDelaunay();
	UpdateHull();

#ifdef GEOTOOLS_SHOW_MESSAGE
	// set or reset the initial text
	const std::size_t num_verts = m_vertices.size();
	if(num_verts < 3)
		update();
#endif
}


void HullScene::UpdateHull()
{
	// remove previous hull
	for(QGraphicsItem* hullItem : m_hull)
	{
		removeItem(hullItem);
		delete hullItem;
	}
	m_hull.clear();

	if(!m_calchull || m_vertices.size() < 3)
		return;

	std::vector<t_vec> vertices;
	vertices.reserve(m_vertices.size());
	std::transform(m_vertices.begin(), m_vertices.end(), std::back_inserter(vertices),
		[](const Vertex* vert) -> t_vec { return tl2::create<t_vec>({vert->x(), vert->y()}); } );


	std::vector<std::vector<t_vec>> hull;
	hull.reserve(1);

	switch(m_hullcalculationmethod)
	{
		case HullCalculationMethod::QHULL:
			std::tie(std::ignore, hull, std::ignore)
				= geo::calc_delaunay<t_vec>(2, vertices, true, false);
			break;
		case HullCalculationMethod::CONTOUR:
			hull.emplace_back(geo::calc_hull_contour<t_vec>(vertices, g_eps));
			break;
		case HullCalculationMethod::ITERATIVE:
			hull.emplace_back(geo::calc_hull_iterative_bintree<t_vec>(vertices, g_eps));
			break;
		case HullCalculationMethod::RECURSIVE:
			hull.emplace_back(geo::calc_hull_recursive<t_vec>(vertices, g_eps));
			break;
		default:
			QMessageBox::critical(m_parent, "Error", "Unknown hull calculation method.");
			break;
	}

	// convex hull
	QPen penHull;
	penHull.setWidthF(3.);

	for(const auto& thetriag : hull)
	{
		for(std::size_t idx1=0; idx1<thetriag.size(); ++idx1)
		{
			std::size_t idx2 = idx1+1;
			if(idx2 >= thetriag.size())
				idx2 = 0;
			if(idx1 == idx2)
				continue;

			QLineF line{QPointF{thetriag[idx1][0], thetriag[idx1][1]}, QPointF{thetriag[idx2][0], thetriag[idx2][1]}};
			QGraphicsItem *item = addLine(line, penHull);
			m_hull.insert(item);
		}
	}
}


void HullScene::UpdateDelaunay()
{
	// remove previous triangulation
	for(QGraphicsItem* item : m_delaunay)
	{
		removeItem(item);
		delete item;
	}
	m_delaunay.clear();

	// remove previous voronoi vertices
	for(QGraphicsItem* item : m_voronoi)
	{
		removeItem(item);
		delete item;
	}
	m_voronoi.clear();


	if((!m_calcdelaunay && !m_calckruskal
		&& !m_calcvoronoivertices && !m_calcvoronoiregions)
		|| m_vertices.size() < 4)
		return;


	// get vertices
	std::vector<t_vec> vertices;
	vertices.reserve(m_vertices.size());
	std::transform(m_vertices.begin(), m_vertices.end(), std::back_inserter(vertices),
		[](const Vertex* vert) -> t_vec
		{
			return tl2::create<t_vec>({vert->x(), vert->y()});
		});


	std::vector<t_vec> voronoi{};
	std::vector<std::vector<t_vec>> triags{};
	std::vector<std::set<std::size_t>> neighbours{};

	switch(m_delaunaycalculationmethod)
	{
		case DelaunayCalculationMethod::QHULL:
			std::tie(voronoi, triags, neighbours) =
				geo::calc_delaunay<t_vec>(2, vertices, false);
			break;
		case DelaunayCalculationMethod::ITERATIVE:
			std::tie(voronoi, triags, neighbours) =
				geo::calc_delaunay_iterative<t_vec>(vertices, g_eps);
			break;
		case DelaunayCalculationMethod::PARABOLIC:
			std::tie(voronoi, triags, neighbours) =
				geo::calc_delaunay_parabolic<t_vec, t_vec_dyn>(vertices);
			break;
		default:
			QMessageBox::critical(m_parent, "Error", "Unknown Delaunay calculation method.");
			break;
	}


	const t_real itemRad = 7.;

	if(m_calcvoronoivertices)
	{
		QPen penVoronoi;
		penVoronoi.setStyle(Qt::SolidLine);
		penVoronoi.setWidthF(1.);

		QPen penCircle;
		penCircle.setStyle(Qt::DotLine);
		penCircle.setWidthF(1.);
		penCircle.setColor(QColor::fromRgbF(1.,0.,0.));

		QBrush brushVoronoi;
		brushVoronoi.setStyle(Qt::SolidPattern);
		brushVoronoi.setColor(QColor::fromRgbF(1.,0.,0.));

		// voronoi vertices
		for(std::size_t idx=0; idx<voronoi.size(); ++idx)
		{
			const t_vec& voronoivert = voronoi[idx];

			QPointF voronoipt{voronoivert[0], voronoivert[1]};
			QGraphicsItem *voronoiItem = addEllipse(
				voronoipt.x()-itemRad/2., voronoipt.y()-itemRad/2., itemRad, itemRad, penVoronoi, brushVoronoi);
			m_voronoi.insert(voronoiItem);

			// circles
			if(idx < triags.size())
			{
				const auto& triag = triags[idx];
				if(triag.size() >= 3)
				{
					t_real rad = tl2::norm(voronoivert-triag[0]);

					QGraphicsItem *voronoiCircle = addEllipse(
						voronoipt.x()-rad, voronoipt.y()-rad, rad*2., rad*2., penCircle);
					m_voronoi.insert(voronoiCircle);
				}
			}
		}
	}


	if(m_calcvoronoiregions && neighbours.size()==voronoi.size())
	{
		QPen penVoronoi;
		penVoronoi.setStyle(Qt::SolidLine);
		penVoronoi.setWidthF(1.);
		penVoronoi.setColor(QColor::fromRgbF(1.,0.,0.));

		QPen penVoronoiUnbound;
		penVoronoiUnbound.setStyle(Qt::DashLine);
		penVoronoiUnbound.setWidthF(1.);
		penVoronoiUnbound.setColor(QColor::fromRgbF(1.,0.,0.));

		for(std::size_t idx=0; idx<voronoi.size(); ++idx)
		{
			// voronoi vertex and its corresponding delaunay triangle
			const t_vec& voronoivert = voronoi[idx];
			const auto& thetriag = triags[idx];

			std::vector<const t_vec*> neighbourverts;
			neighbourverts.reserve(neighbours[idx].size());

			for(std::size_t neighbourIdx : neighbours[idx])
			{
				const t_vec& neighbourvert = voronoi[neighbourIdx];
				neighbourverts.push_back(&neighbourvert);

				QLineF line{QPointF{voronoivert[0], voronoivert[1]}, QPointF{neighbourvert[0], neighbourvert[1]}};
				QGraphicsItem *item = addLine(line, penVoronoi);
				m_voronoi.insert(item);
			}

			// not all triangle edges have neighbours -> there are unbound regions
			if(neighbourverts.size() < 3)
			{
				// slopes of existing voronoi edges
				std::vector<t_real> slopes;
				slopes.reserve(neighbourverts.size());

				for(const t_vec* vec : neighbourverts)
					slopes.push_back(geo::line_angle(voronoivert, *vec));

				// iterate delaunay triangle vertices
				for(std::size_t idx1=0; idx1<thetriag.size(); ++idx1)
				{
					std::size_t idx2 = idx1+1;
					if(idx2 >= thetriag.size())
						idx2 = 0;

					t_vec vecMid = thetriag[idx1] + (thetriag[idx2] - thetriag[idx1]) * t_real{0.5};
					t_real angle = geo::line_angle(voronoivert, vecMid);

					// if the slope angle doesn't exist yet, it leads to an unbound external region
					if(auto iterSlope = std::find_if(slopes.begin(), slopes.end(), [angle](t_real angle2) -> bool
					{ return tl2::angle_equals<t_real>(angle, angle2, g_eps, tl2::pi<t_real>); });
					iterSlope == slopes.end())
					{
						t_vec vecUnbound = (vecMid-voronoivert);
						t_real lengthUnbound = 1000. / tl2::norm(vecUnbound);
						t_vec vecOuter = voronoivert;

						// voronoi vertex on other side of edge?
						if(geo::side_of_line<t_vec>(thetriag[idx1], thetriag[idx2], voronoivert) < 0.)
							vecOuter -= lengthUnbound*vecUnbound;
						else
							vecOuter += lengthUnbound*vecUnbound;

						QLineF line{QPointF{voronoivert[0], voronoivert[1]}, QPointF{vecOuter[0], vecOuter[1]}};
						QGraphicsItem *item = addLine(line, penVoronoiUnbound);
						m_voronoi.insert(item);
					}
				}
			}
		}
	}


	if(m_calcdelaunay)
	{
		QPen penDelaunay;
		penDelaunay.setStyle(Qt::SolidLine);
		penDelaunay.setWidthF(1.);
		penDelaunay.setColor(QColor::fromRgbF(0.,0.,0.));

		// delaunay triangles
		for(const auto& thetriag : triags)
		{
			for(std::size_t idx1=0; idx1<thetriag.size(); ++idx1)
			{
				std::size_t idx2 = idx1+1;
				if(idx2 >= thetriag.size())
					idx2 = 0;

				QLineF line{QPointF{thetriag[idx1][0], thetriag[idx1][1]}, QPointF{thetriag[idx2][0], thetriag[idx2][1]}};
				QGraphicsItem *item = addLine(line, penDelaunay);
				m_delaunay.insert(item);
			}
		}
	}


	if(m_calckruskal)
	{
		QPen penKruskal;
		penKruskal.setStyle(Qt::SolidLine);
		penKruskal.setWidthF(2.);
		penKruskal.setColor(QColor::fromRgbF(0.,0.7,0.));

		auto edges = geo::get_edges(vertices, triags, g_eps);
		std::vector<std::pair<std::size_t, std::size_t>> span;

		switch(m_spancalculationmethod)
		{
			case SpanCalculationMethod::KRUSKAL:
				span = geo::calc_min_spantree<t_vec>(vertices, edges);
				break;
			case SpanCalculationMethod::BOOST:
				span = geo::calc_min_spantree_boost<t_vec>(vertices);
				break;
			default:
				QMessageBox::critical(m_parent, "Error", "Unknown span tree calculation method.");
				break;
		}

		for(const auto& spanedge : span)
		{
			const t_vec& vert1 = vertices[spanedge.first];
			const t_vec& vert2 = vertices[spanedge.second];

			QLineF line{QPointF{vert1[0], vert1[1]}, QPointF{vert2[0], vert2[1]}};
			QGraphicsItem *item = addLine(line, penKruskal);
			m_delaunay.insert(item);
		}
	}
}

// ----------------------------------------------------------------------------




// ----------------------------------------------------------------------------

HullView::HullView(HullScene *scene, QWidget *parent) : QGraphicsView(scene, parent),
	m_scene{scene}
{
	setCacheMode(QGraphicsView::CacheBackground);

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	setInteractive(true);
	setMouseTracking(true);

	setBackgroundBrush(QBrush{QColor::fromRgbF(0.95, 0.95, 0.95, 1.)});
	//setSceneRect(mapToScene(viewport()->rect()).boundingRect());
}


HullView::~HullView()
{
}


void HullView::UpdateAll()
{
	// triggers updates
	QResizeEvent evt{size(), size()};
	resizeEvent(&evt);
}


void HullView::resizeEvent(QResizeEvent *evt)
{
	QPointF pt1{mapToScene(QPoint{0,0})};
	QPointF pt2{mapToScene(QPoint{evt->size().width(), evt->size().height()})};

	const t_real padding = 16;

	// include bounds given by vertices
	for(const Vertex* vertex : m_scene->GetVertices())
	{
		QPointF vertexpos = vertex->scenePos();

		if(vertexpos.x() < pt1.x())
			pt1.setX(vertexpos.x() -  padding);
		if(vertexpos.x() > pt2.x())
			pt2.setX(vertexpos.x() +  padding);
		if(vertexpos.y() < pt1.y())
			pt1.setY(vertexpos.y() -  padding);
		if(vertexpos.y() > pt2.y())
			pt2.setY(vertexpos.y() +  padding);
	}

	setSceneRect(QRectF{pt1, pt2});

	//QGraphicsView::resizeEvent(evt);
}



void HullView::mousePressEvent(QMouseEvent *evt)
{
	QPoint posVP = evt->pos();
	QPointF posScene = mapToScene(posVP);

	QList<QGraphicsItem*> items = this->items(posVP);
	QGraphicsItem* item = nullptr;
	bool item_is_vertex = false;

	auto& verts = m_scene->GetVertices();

	for(int itemidx=0; itemidx<items.size(); ++itemidx)
	{
		item = items[itemidx];
		item_is_vertex = verts.find(static_cast<Vertex*>(item)) != verts.end();
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
		if(item && item_is_vertex)
		{
			m_scene->removeItem(item);
			verts.erase(static_cast<Vertex*>(item));
			delete item;
			m_scene->UpdateAll();
		}
	}

	QGraphicsView::mousePressEvent(evt);
}


void HullView::mouseReleaseEvent(QMouseEvent *evt)
{
	if(evt->button() == Qt::LeftButton)
		m_dragging = false;

	m_scene->UpdateAll();
	QGraphicsView::mouseReleaseEvent(evt);
}


void HullView::mouseMoveEvent(QMouseEvent *evt)
{
	QGraphicsView::mouseMoveEvent(evt);

	if(m_dragging)
	{
		UpdateAll();
		m_scene->UpdateAll();
	}

	QPoint posVP = evt->pos();
	QPointF posScene = mapToScene(posVP);
	emit SignalMouseCoordinates(posScene.x(), posScene.y());
}


void HullView::wheelEvent(QWheelEvent *evt)
{
	//t_real s = std::pow(2., evt->angleDelta().y() / 1000.);
	//scale(s, s);
	QGraphicsView::wheelEvent(evt);
}


void HullView::drawBackground(QPainter* painter, const QRectF& rect)
{
	QGraphicsView::drawBackground(painter, rect);
}


void HullView::drawForeground(QPainter* painter, const QRectF& rect)
{
	QGraphicsView::drawForeground(painter, rect);

#ifdef GEOTOOLS_SHOW_MESSAGE
	if(!m_scene->GetVertices().size())
	{
		QFont font = painter->font();
		font.setBold(true);

		QString msg{"Click to place vertices."};
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

HullWnd::HullWnd(QWidget* pParent) : QDialog{pParent},
	m_scene{new HullScene{this}},
	m_view{new HullView{m_scene.get(), this}},
	m_statusLabel{std::make_shared<QLabel>(this)}
{
	// ------------------------------------------------------------------------
	// restore settings
#ifdef TASPATHS_TOOLS_STANDALONE
	// set-up common gui variables
	GeoSettingsDlg::SetGuiTheme(&g_theme);
	GeoSettingsDlg::SetGuiFont(&g_font);
	GeoSettingsDlg::SetGuiUseNativeMenubar(&g_use_native_menubar);
	GeoSettingsDlg::SetGuiUseNativeDialogs(&g_use_native_dialogs);

	GeoSettingsDlg::ReadSettings(&m_sett);
#endif

	m_scene->SetCalculateHull(
		m_sett.value("hull_calc_hull", m_scene->GetCalculateHull()).toBool());
	m_scene->SetCalculateVoronoiVertices(
		m_sett.value("hull_calc_voronoivertices", m_scene->GetCalculateVoronoiVertices()).toBool());
	m_scene->SetCalculateVoronoiRegions(
		m_sett.value("hull_calc_voronoiregions", m_scene->GetCalculateVoronoiRegions()).toBool());
	m_scene->SetCalculateDelaunay(
		m_sett.value("hull_calc_delaunay", m_scene->GetCalculateDelaunay()).toBool());
	m_scene->SetCalculateKruskal(
		m_sett.value("hull_calc_kruskal", m_scene->GetCalculateKruskal()).toBool());
	// ------------------------------------------------------------------------


	m_view->setRenderHints(QPainter::Antialiasing);

	setWindowTitle("Convex Hull");

	QGridLayout *layout = new QGridLayout(this);
	layout->setSpacing(6);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->addWidget(m_view.get(), 0, 0, 1, 1);

	m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

	QStatusBar *statusBar = new QStatusBar{this};
	statusBar->addPermanentWidget(m_statusLabel.get(), 1);
	statusBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	layout->addWidget(statusBar, 1, 0, 1, 1);


	// menu actions
	QAction *actionNew = new QAction{QIcon::fromTheme("document-new"), "New", this};
	connect(actionNew, &QAction::triggered, this, &HullWnd::NewFile);

	QAction *actionLoad = new QAction{QIcon::fromTheme("document-open"), "Open...", this};
	connect(actionLoad, &QAction::triggered, this,
		static_cast<void (HullWnd::*)()>(&HullWnd::OpenFile));

	QAction *actionSave = new QAction{QIcon::fromTheme("document-save"), "Save", this};
	connect(actionSave, &QAction::triggered, this,
		static_cast<void (HullWnd::*)()>(&HullWnd::SaveFile));

	QAction *actionSaveAs = new QAction{QIcon::fromTheme("document-save-as"), "Save as...", this};
	connect(actionSaveAs, &QAction::triggered, this, &HullWnd::SaveFileAs);

	QAction *actionExportSvg = new QAction{QIcon::fromTheme("image-x-generic"), "Export SVG...", this};
	connect(actionExportSvg, &QAction::triggered, [this]()
	{
		QString dirLast = m_sett.value("cur_image_dir", QDir::homePath()).toString();

		if(QString file = QFileDialog::getSaveFileName(
			this, "Export SVG", dirLast+"/untitled.svg",
			"SVG Files (*.svg);;All Files (* *.*)"); file!="")
		{
			QSvgGenerator svggen;
			svggen.setSize(QSize{width(), height()});
			svggen.setFileName(file);

			QPainter paint(&svggen);
			m_scene->render(&paint);
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
	connect(actionQuit, &QAction::triggered, [this]()
		{ this->close(); });


	QAction *actionZoomIn = new QAction{QIcon::fromTheme("zoom-in"), "Zoom in", this};
	connect(actionZoomIn, &QAction::triggered, [this]()
	{
		if(m_view)
			m_view->scale(2., 2.);
	});

	QAction *actionZoomOut = new QAction{QIcon::fromTheme("zoom-out"), "Zoom out", this};
	connect(actionZoomOut, &QAction::triggered, [this]()
	{
		if(m_view)
			m_view->scale(0.5, 0.5);
	});

	QAction *actionIncreaseVertexSize = new QAction{"Increase Vertex Size", this};
	connect(actionIncreaseVertexSize, &QAction::triggered, [this]()
	{
		if(!m_scene)
			return;

		for(Vertex* vert : m_scene->GetVertices())
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

		for(Vertex* vert : m_scene->GetVertices())
		{
			t_real rad = vert->GetRadius();
			rad *= t_real(0.5);
			vert->SetRadius(rad);
		}

		m_scene->update(QRectF());
	});


	QAction *actionHullDlg = new QAction{"General Convex Hull...", this};
	connect(actionHullDlg, &QAction::triggered, [this]()
	{
		if(!m_hulldlg)
			m_hulldlg = std::make_shared<HullDlg>(this);

		m_hulldlg->show();
		m_hulldlg->raise();
		m_hulldlg->activateWindow();
	});


	QAction *actionHull = new QAction{"Convex Hull", this};
	actionHull->setCheckable(true);
	actionHull->setChecked(m_scene->GetCalculateHull());
	connect(actionHull, &QAction::toggled, [this](bool b)
		{ m_scene->SetCalculateHull(b); });

	QAction *actionVoronoi = new QAction{"Voronoi Vertices", this};
	actionVoronoi->setCheckable(true);
	actionVoronoi->setChecked(m_scene->GetCalculateVoronoiVertices());
	connect(actionVoronoi, &QAction::toggled, [this](bool b)
		{ m_scene->SetCalculateVoronoiVertices(b); });

	QAction *actionVoronoiRegions = new QAction{"Voronoi Regions", this};
	actionVoronoiRegions->setCheckable(true);
	actionVoronoiRegions->setChecked(m_scene->GetCalculateVoronoiRegions());
	connect(actionVoronoiRegions, &QAction::toggled, [this](bool b)
		{ m_scene->SetCalculateVoronoiRegions(b); });

	QAction *actionDelaunay = new QAction{"Delaunay Triangulation", this};
	actionDelaunay->setCheckable(true);
	actionDelaunay->setChecked(m_scene->GetCalculateDelaunay());
	connect(actionDelaunay, &QAction::toggled, [this](bool b)
		{ m_scene->SetCalculateDelaunay(b); });

	QAction *actionSpanTree = new QAction{"Minimum Spanning Tree", this};
	actionSpanTree->setCheckable(true);
	actionSpanTree->setChecked(m_scene->GetCalculateKruskal());
	connect(actionSpanTree, &QAction::toggled, [this](bool b)
		{ m_scene->SetCalculateKruskal(b); });


	QAction *actionHullQHull = new QAction{"QHull", this};
	actionHullQHull->setCheckable(true);
	actionHullQHull->setChecked(true);
	connect(actionHullQHull, &QAction::toggled, [this]()
		{ m_scene->SetHullCalculationMethod(HullCalculationMethod::QHULL); });

	QAction *actionHullContour = new QAction{"Contour", this};
	actionHullContour->setCheckable(true);
	connect(actionHullContour, &QAction::toggled, [this]()
		{ m_scene->SetHullCalculationMethod(HullCalculationMethod::CONTOUR); });

	QAction *actionHullInc = new QAction{"Incremental", this};
	actionHullInc->setCheckable(true);
	connect(actionHullInc, &QAction::toggled, [this]()
		{ m_scene->SetHullCalculationMethod(HullCalculationMethod::ITERATIVE); });

	QAction *actionHullDivide = new QAction{"Divide && Conquer", this};
	actionHullDivide->setCheckable(true);
	connect(actionHullDivide, &QAction::toggled, [this]()
		{ m_scene->SetHullCalculationMethod(HullCalculationMethod::RECURSIVE); });


	QAction *actionDelaunayQHull = new QAction{"QHull", this};
	actionDelaunayQHull->setCheckable(true);
	actionDelaunayQHull->setChecked(true);
	connect(actionDelaunayQHull, &QAction::toggled, [this]()
		{ m_scene->SetDelaunayCalculationMethod(DelaunayCalculationMethod::QHULL); });

	QAction *actionDelaunayInc = new QAction{"Incremental", this};
	actionDelaunayInc->setCheckable(true);
	connect(actionDelaunayInc, &QAction::toggled, [this]()
		{ m_scene->SetDelaunayCalculationMethod(DelaunayCalculationMethod::ITERATIVE); });

	QAction *actionDelaunayPara = new QAction{"Parabolic Trafo", this};
	actionDelaunayPara->setCheckable(true);
	connect(actionDelaunayPara, &QAction::toggled, [this]()
		{ m_scene->SetDelaunayCalculationMethod(DelaunayCalculationMethod::PARABOLIC); });


	QAction *actionSpanKruskal = new QAction{"Kruskal", this};
	actionSpanKruskal->setCheckable(true);
	actionSpanKruskal->setChecked(true);
	connect(actionSpanKruskal, &QAction::toggled, [this]()
		{ m_scene->SetSpanCalculationMethod(SpanCalculationMethod::KRUSKAL); });

	QAction *actionSpanBoost = new QAction{"Kruskal via Boost.Graph", this};
	actionSpanBoost->setCheckable(true);
	connect(actionSpanBoost, &QAction::toggled, [this]()
		{ m_scene->SetSpanCalculationMethod(SpanCalculationMethod::BOOST); });

	QActionGroup *groupHullBack = new QActionGroup{this};
	groupHullBack->addAction(actionHullQHull);
	groupHullBack->addAction(actionHullContour);
	groupHullBack->addAction(actionHullInc);
	groupHullBack->addAction(actionHullDivide);

	QActionGroup *groupDelaunayBack = new QActionGroup{this};
	groupDelaunayBack->addAction(actionDelaunayQHull);
	groupDelaunayBack->addAction(actionDelaunayInc);
	groupDelaunayBack->addAction(actionDelaunayPara);

	QActionGroup *groupSpanBack = new QActionGroup{this};
	groupSpanBack->addAction(actionSpanKruskal);
	groupSpanBack->addAction(actionSpanBoost);

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


	// menu
	QMenu *menuFile = new QMenu{"File", this};
	QMenu *menuView = new QMenu{"View", this};
	QMenu *menuCalc = new QMenu{"Calculate", this};
	QMenu *menuBack = new QMenu{"Backends", this};
	QMenu *menuTools = new QMenu{"Tools", this};
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

	menuTools->addAction(actionHullDlg);

	menuCalc->addAction(actionHull);
	menuCalc->addSeparator();
	menuCalc->addAction(actionVoronoi);
	menuCalc->addAction(actionVoronoiRegions);
	menuCalc->addSeparator();
	menuCalc->addAction(actionDelaunay);
	menuCalc->addAction(actionSpanTree);


	QMenu *menuBackHull = new QMenu{"Convex Hull", this};
	menuBackHull->addAction(actionHullQHull);
	menuBackHull->addAction(actionHullContour);
	menuBackHull->addAction(actionHullInc);
	menuBackHull->addAction(actionHullDivide);

	QMenu *menuDelaunay = new QMenu{"Delaunay Triangulation", this};
	menuDelaunay->addAction(actionDelaunayQHull);
	menuDelaunay->addAction(actionDelaunayInc);
	menuDelaunay->addAction(actionDelaunayPara);

	QMenu *menuSpan = new QMenu{"Minimum Spanning Tree", this};
	menuSpan->addAction(actionSpanKruskal);
	menuSpan->addAction(actionSpanBoost);

	menuBack->addMenu(menuBackHull);
	menuBack->addMenu(menuDelaunay);
	menuBack->addMenu(menuSpan);

	menuHelp->addAction(actionAboutQt);
	menuHelp->addSeparator();
	menuHelp->addAction(actionAbout);


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


	// menu bar
	QMenuBar *menuBar = new QMenuBar{this};
	//menuBar->setNativeMenuBar(false);
	menuBar->addMenu(menuFile);
	menuBar->addMenu(menuView);
	menuBar->addMenu(menuCalc);
	menuBar->addMenu(menuBack);
	menuBar->addMenu(menuTools);
	menuBar->addMenu(menuHelp);
	layout->setMenuBar(menuBar);


	// ------------------------------------------------------------------------
	// restore settings
	if(m_sett.contains("hull_wnd_geo"))
	{
		QByteArray arr{m_sett.value("hull_wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		resize(800, 600);
	}

	// recent files
	if(m_sett.contains("hull_recent_files"))
		m_recent.SetRecentFiles(m_sett.value("hull_recent_files").toStringList());
	// ------------------------------------------------------------------------


	// connections
	connect(m_view.get(), &HullView::SignalMouseCoordinates, 
		[this](t_real x, t_real y)
		{
			SetStatusMessage(QString("x=%1, y=%2.").arg(x, 5).arg(y, 5));
		});


	SetStatusMessage("Ready.");
}


void HullWnd::SetStatusMessage(const QString& msg)
{
	m_statusLabel->setText(msg);
}


void HullWnd::closeEvent(QCloseEvent *e)
{
	// save settings
	QByteArray geo{this->saveGeometry()};
	m_sett.setValue("hull_wnd_geo", geo);
	m_sett.setValue("hull_calc_hull", m_scene->GetCalculateHull());
	m_sett.setValue("hull_calc_voronoivertices", m_scene->GetCalculateVoronoiVertices());
	m_sett.setValue("hull_calc_voronoiregions", m_scene->GetCalculateVoronoiRegions());
	m_sett.setValue("hull_calc_delaunay", m_scene->GetCalculateDelaunay());
	m_sett.setValue("hull_calc_kruskal", m_scene->GetCalculateKruskal());

	// save recent files
	m_recent.TrimEntries();
	m_sett.setValue("hull_recent_files", m_recent.GetRecentFiles());

	QDialog::closeEvent(e);
}


HullWnd::~HullWnd()
{
}


/**
 * File -> New
 */
void HullWnd::NewFile()
{
	SetCurrentFile("");

	m_scene->ClearVertices();
}


/**
 * open file
 */
bool HullWnd::OpenFile(const QString& file)
{
	std::ifstream ifstr(file.toStdString());
	if(!ifstr)
	{
		QMessageBox::critical(this, "Error", "File could not be opened for loading.");
		return false;
	}

	m_scene->ClearVertices();

	ptree::ptree prop{};
	ptree::read_xml(ifstr, prop);

	std::size_t vertidx = 0;
	while(true)
	{
		std::ostringstream ostrVert;
		ostrVert << "voro2d.vertices." << vertidx;

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

	if(vertidx > 0)
	{
		m_sett.setValue("cur_dir", QFileInfo(file).path());
		m_scene->UpdateAll();

		SetCurrentFile(file);
		m_recent.AddRecentFile(file);
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
void HullWnd::OpenFile()
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
bool HullWnd::SaveFile(const QString& file)
{
	std::ofstream ofstr(file.toStdString());
	if(!ofstr)
	{
		QMessageBox::critical(this, "Error", "File could not be opened for saving.");
		return false;
	}

	ptree::ptree prop{};
	prop.put("voro2d.ident", "takin_taspaths_hull");
	prop.put("voro2d.doi", "https://doi.org/10.5281/zenodo.4625649");
	prop.put("voro2d.timestamp", tl2::var_to_str(tl2::epoch<t_real>()));

	std::size_t vertidx = 0;
	for(const Vertex* vertex : m_scene->GetVertices())
	{
		QPointF vertexpos = vertex->scenePos();

		std::ostringstream ostrX, ostrY;
		ostrX << "voro2d.vertices." << vertidx << ".<xmlattr>.x";
		ostrY << "voro2d.vertices." << vertidx << ".<xmlattr>.y";

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
void HullWnd::SaveFile()
{
	if(m_recent.GetCurFile() == "")
		SaveFileAs();
	else
		SaveFile(m_recent.GetCurFile());
}


/**
 * File -> Save As
 */
void HullWnd::SaveFileAs()
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
void HullWnd::SetCurrentFile(const QString &file)
{
	m_recent.SetCurFile(file);
	this->setWindowFilePath(m_recent.GetCurFile());

	/*static const QString title(PROG_TITLE);
	if(m_recent.GetCurFile() == "")
		this->setWindowTitle(title);
	else
		this->setWindowTitle(title + " \u2014 " + m_recent.GetCurFile());*/
}


// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
HullDlg::HullDlg(QWidget* pParent) : QDialog{pParent}
{
	// ------------------------------------------------------------------------
	// restore settings
#ifdef TASPATHS_TOOLS_STANDALONE
	GeoSettingsDlg::ReadSettings(&m_sett);
#endif

	if(m_sett.contains("hull_dlg_geo"))
	{
		QByteArray arr{m_sett.value("hull_dlg_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		resize(450, 400);
	}
	// ------------------------------------------------------------------------

	setWindowTitle("Convex Hull Calculation");


	// table
	m_tab = std::make_shared<QTableWidget>(this);
	m_tab->setShowGrid(true);
	m_tab->setSortingEnabled(true);
	m_tab->setMouseTracking(true);
	m_tab->setSelectionBehavior(QTableWidget::SelectRows);
	m_tab->setSelectionMode(QTableWidget::ContiguousSelection);
	m_tab->setContextMenuPolicy(Qt::CustomContextMenu);


	// text edit
	m_editResults = std::make_shared<QPlainTextEdit>(this);
	m_editResults->setReadOnly(true);


	// buttons
	QToolButton *tabBtnAdd = new QToolButton(this);
	QToolButton *tabBtnDel = new QToolButton(this);
	QToolButton *tabBtnUp = new QToolButton(this);
	QToolButton *tabBtnDown = new QToolButton(this);

	tabBtnAdd->setSizePolicy(QSizePolicy{QSizePolicy::Fixed, QSizePolicy::Fixed});
	tabBtnDel->setSizePolicy(QSizePolicy{QSizePolicy::Fixed, QSizePolicy::Fixed});
	tabBtnUp->setSizePolicy(QSizePolicy{QSizePolicy::Fixed, QSizePolicy::Fixed});
	tabBtnDown->setSizePolicy(QSizePolicy{QSizePolicy::Fixed, QSizePolicy::Fixed});

	tabBtnAdd->setText("\xe2\x8a\x95");
	tabBtnDel->setText("\xe2\x8a\x96");
	tabBtnUp->setText("\342\206\221");
	tabBtnDown->setText("\342\206\223");

	tabBtnAdd->setToolTip("Add vertex.");
	tabBtnDel->setToolTip("Delete vertex.");
	tabBtnUp->setToolTip("Move vertex up.");
	tabBtnDown->setToolTip("Move vertex down.");

	QDialogButtonBox *buttons = new QDialogButtonBox(this);
	buttons->setStandardButtons(QDialogButtonBox::Ok);


	// dimension spin box
	QSpinBox *spin = new QSpinBox(this);
	spin->setMinimum(2);
	spin->setMaximum(99);
	spin->setValue(3);
	spin->setPrefix("dim = ");


	// delaunay / hull check box
	m_checkDelaunay = std::make_shared<QCheckBox>(this);
	m_checkDelaunay->setChecked(false);
	m_checkDelaunay->setText("Delaunay");


	// splitter
	QSplitter *splitter = new QSplitter(Qt::Vertical, this);
	splitter->addWidget(m_tab.get());
	splitter->addWidget(m_editResults.get());


	// grid
	int y = 0;
	auto pTabGrid = new QGridLayout(this);
	pTabGrid->setSpacing(4);
	pTabGrid->setContentsMargins(12, 12, 12, 12);
	pTabGrid->addWidget(splitter, y++,0,1,9);
	pTabGrid->addWidget(tabBtnAdd, y,0,1,1);
	pTabGrid->addWidget(tabBtnDel, y,1,1,1);
	pTabGrid->addWidget(tabBtnUp, y,2,1,1);
	pTabGrid->addWidget(tabBtnDown, y,3,1,1);
	pTabGrid->addItem(new QSpacerItem(4, 4,
		QSizePolicy::Expanding, QSizePolicy::Minimum), y,4,1,1);
	pTabGrid->addWidget(spin, y,5,1,1);
	pTabGrid->addWidget(m_checkDelaunay.get(), y,6,1,1);
	pTabGrid->addItem(new QSpacerItem(4, 4,
		QSizePolicy::Expanding, QSizePolicy::Minimum), y,7,1,1);
	pTabGrid->addWidget(buttons, y++,8,1,1);


	// table context menu
	m_contextMenuTab = std::make_shared<QMenu>(m_tab.get());
	m_contextMenuTab->addAction("Add Item Before", this,
		[this]() { this->AddTabItem(-2); });
	m_contextMenuTab->addAction("Add Item After", this,
		[this]() { this->AddTabItem(-3); });
	m_contextMenuTab->addAction("Delete Item", this,
		&HullDlg::DelTabItem);


	// connect the signals
	connect(tabBtnAdd, &QToolButton::clicked,
		this, [this]() { this->AddTabItem(-1); });
	connect(tabBtnDel, &QToolButton::clicked,
		this, &HullDlg::DelTabItem);
	connect(tabBtnUp, &QToolButton::clicked,
		this, &HullDlg::MoveTabItemUp);
	connect(tabBtnDown, &QToolButton::clicked,
		this, &HullDlg::MoveTabItemDown);
	connect(m_tab.get(), &QTableWidget::currentCellChanged,
		this, &HullDlg::TableCellChanged);
	connect(m_tab.get(), &QTableWidget::customContextMenuRequested,
		this, &HullDlg::ShowTableContextMenu);
	connect(spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		this, &HullDlg::SetDim);
	connect(m_checkDelaunay.get(), &QCheckBox::stateChanged,
		[this]() { CalculateHull(); });
	connect(buttons, &QDialogButtonBox::accepted, this, &HullDlg::accept);

	SetDim(3);
}


void HullDlg::accept()
{
	// ------------------------------------------------------------------------
	// save settings
	QByteArray geo{this->saveGeometry()};
	m_sett.setValue("hull_dlg_geo", geo);
	// ------------------------------------------------------------------------

	QDialog::accept();
}


void HullDlg::SetDim(int dim)
{
	m_tab->clear();
	m_tab->horizontalHeader()->setDefaultSectionSize(125);
	m_tab->verticalHeader()->setDefaultSectionSize(32);
	m_tab->verticalHeader()->setVisible(false);
	m_tab->setColumnCount(dim);
	m_tab->setRowCount(0);

	for(int i=0; i<dim; ++i)
	{
		m_tab->setColumnWidth(i, 125);

		QString coord = QString("x%1").arg(i);
		m_tab->setHorizontalHeaderItem(i, new QTableWidgetItem{coord});
	}

	CalculateHull();
}


void HullDlg::CalculateHull()
{
	using namespace tl2_ops;

	try
	{
		m_editResults->clear();

		bool calc_hull = !m_checkDelaunay->isChecked();
		int dim = m_tab->columnCount();
		int rows = m_tab->rowCount();
		int needed_dim = calc_hull ? dim+1 : dim+2;

		if(rows < needed_dim)
		{
			m_editResults->setPlainText("Not enough vectors.\n");
			return;
		}

		// get vertices
		std::vector<t_vec> vertices;
		vertices.reserve(rows);

		for(int row=0; row<rows; ++row)
		{
			t_vec vertex = tl2::create<t_vec>(dim);

			for(int col=0; col<dim; ++col)
			{
				auto widget = dynamic_cast<tl2::NumericTableWidgetItem<t_real>*>(
					m_tab->item(row, col));
				vertex[col] = widget->GetValue();
			}

			vertices.emplace_back(std::move(vertex));
		}


		int numNonZeroVerts = 0;
		for(const t_vec& vertex : vertices)
		{
			t_real n = tl2::norm<t_vec>(vertex);
			if(!tl2::equals<t_real>(n, 0, g_eps))
				++numNonZeroVerts;
		}

		if(numNonZeroVerts < needed_dim)
		{
			m_editResults->setPlainText("Not enough independent vectors.\n");
			return;
		}


		// calculate hull / delaunay triangulation
		auto [voro, triags, neighbourindices] =
			geo::calc_delaunay<t_vec>(dim, vertices, calc_hull);


		// output results
		std::ostringstream ostr;

		for(std::size_t vertidx=0; vertidx<vertices.size(); ++vertidx)
		{
			ostr << "Vertex " << (vertidx+1) << ": "
				<< vertices[vertidx] << "\n";
		}

		if(voro.size())
			ostr << "\n";

		// voronoi vertices
		for(std::size_t voroidx=0; voroidx<voro.size(); ++voroidx)
		{
			ostr << "Voronoi vertex " << (voroidx+1) << ": "
				<< voro[voroidx] << "\n";
		}

		if(triags.size())
			ostr << "\n";

		// hull or delaunay triangles
		for(std::size_t polyidx=0; polyidx<triags.size(); ++polyidx)
		{
			const auto& poly = triags[polyidx];
			if(poly.size() <= 2)
				ostr << "Edge " << (polyidx+1) << ":\n";
			else
				ostr << "Polygon " << (polyidx+1) << ":\n";
			ostr << "\tVertices:\n";
			for(const auto& vertex : poly)
				ostr << "\t\t" << vertex << "\n";

			if(neighbourindices.size())
			{
				const auto& neighbours = neighbourindices[polyidx];
				ostr << "\tNeighbour indices:\n";
				ostr << "\t\t";
				for(auto neighbour : neighbours)
					ostr << neighbour << ", ";
				ostr << "\n";
			}

			ostr << "\n";
		}

		m_editResults->setPlainText(ostr.str().c_str());
	}
	catch(const std::exception& ex)
	{
		m_editResults->setPlainText(ex.what());
	}
}


void HullDlg::AddTabItem(int row)
{
	// append to end of table
	if(row == -1)
		row = m_tab->rowCount();

	// use row from member variable
	else if(row == -2 && m_iCursorRow >= 0)
		row = m_iCursorRow;

	// use row from member variable +1
	else if(row == -3 && m_iCursorRow >= 0)
		row = m_iCursorRow + 1;

	m_tab->setSortingEnabled(false);
	m_tab->insertRow(row);

	for(int col=0; col<m_tab->columnCount(); ++col)
		m_tab->setItem(row, col, new tl2::NumericTableWidgetItem<t_real>(0, g_prec));

	m_tab->scrollToItem(m_tab->item(row, 0));
	m_tab->setCurrentCell(row, 0);
	m_tab->setSortingEnabled(true);
}


void HullDlg::DelTabItem()
{
	// if nothing is selected, clear all items
	if(m_tab->selectedItems().count() == 0)
	{
		m_tab->clearContents();
		m_tab->setRowCount(0);
	}


	for(int row : GetSelectedRows(true))
		m_tab->removeRow(row);
}


void HullDlg::MoveTabItemUp()
{
	m_tab->setSortingEnabled(false);

	auto selected = GetSelectedRows(false);
	for(int row : selected)
	{
		if(row == 0)
			continue;

		auto *item = m_tab->item(row, 0);
		if(!item || !item->isSelected())
			continue;

		m_tab->insertRow(row-1);
		for(int col=0; col<m_tab->columnCount(); ++col)
			m_tab->setItem(row-1, col, m_tab->item(row+1, col)->clone());
		m_tab->removeRow(row+1);
	}

	for(int row=0; row<m_tab->rowCount(); ++row)
	{
		if(auto *item = m_tab->item(row, 0);
			item && std::find(selected.begin(), selected.end(), row+1) != selected.end())
		{
			for(int col=0; col<m_tab->columnCount(); ++col)
				m_tab->item(row, col)->setSelected(true);
		}
	}
}


void HullDlg::MoveTabItemDown()
{
	m_tab->setSortingEnabled(false);

	auto selected = GetSelectedRows(true);
	for(int row : selected)
	{
		if(row == m_tab->rowCount()-1)
			continue;

		auto *item = m_tab->item(row, 0);
		if(!item || !item->isSelected())
			continue;

		m_tab->insertRow(row+2);
		for(int col=0; col<m_tab->columnCount(); ++col)
			m_tab->setItem(row+2, col, m_tab->item(row, col)->clone());
		m_tab->removeRow(row);
	}

	for(int row=0; row<m_tab->rowCount(); ++row)
	{
		if(auto *item = m_tab->item(row, 0);
			item && std::find(selected.begin(), selected.end(), row-1) != selected.end())
		{
			for(int col=0; col<m_tab->columnCount(); ++col)
				m_tab->item(row, col)->setSelected(true);
		}
	}
}


std::vector<int> HullDlg::GetSelectedRows(bool sort_reversed) const
{
	std::vector<int> vec;
	vec.reserve(m_tab->selectedItems().size());

	for(int row=0; row<m_tab->rowCount(); ++row)
	{
		if(auto *item = m_tab->item(row, 0); item && item->isSelected())
			vec.push_back(row);
	}

	if(sort_reversed)
	{
		std::stable_sort(vec.begin(), vec.end(), [](int row1, int row2)
		{ return row1 > row2; });
	}

	return vec;
}


void HullDlg::TableCellChanged(int, int, int, int)
{
	CalculateHull();
}


void HullDlg::ShowTableContextMenu(const QPoint& pt)
{
	const auto* item = m_tab->itemAt(pt);
	if(!item)
		return;

	m_iCursorRow = item->row();
	auto ptGlob = m_tab->mapToGlobal(pt);
	ptGlob.setY(ptGlob.y() + m_contextMenuTab->sizeHint().height()/2);
	m_contextMenuTab->popup(ptGlob);
}
// ----------------------------------------------------------------------------
