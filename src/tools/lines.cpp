/**
 * line intersection test program
 * @author Tobias Weber <tweber@ill.fr>
 * @date 11-Nov-2020
 * @note Forked on 19-apr-2021 from my privately developed "geo" project (https://github.com/t-weber/geo).
 * @license see 'LICENSE' file
 */

#include "lines.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QActionGroup>
#include <QtWidgets/QProgressDialog>
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

#include <boost/asio.hpp>
namespace asio = boost::asio;

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
namespace ptree = boost::property_tree;

#include "tlibs2/libs/helper.h"
#include "src/libs/hull.h"


// ----------------------------------------------------------------------------

Vertex::Vertex(const QPointF& pos, double rad) : m_rad{rad}
{
	setPos(pos);
	setFlags(flags() | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
}


Vertex::~Vertex()
{
}


QRectF Vertex::boundingRect() const
{
	return QRectF{-m_rad/2., -m_rad/2., m_rad, m_rad};
}


void Vertex::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	std::array<QColor, 2> colours =
	{
		QColor::fromRgbF(0.,0.,1.),
		QColor::fromRgbF(0.,0.,0.),
	};

	QRadialGradient grad{};
	grad.setCenter(0., 0.);
	grad.setRadius(m_rad);

	for(std::size_t col=0; col<colours.size(); ++col)
		grad.setColorAt(col/double(colours.size()-1), colours[col]);

	painter->setBrush(grad);
	painter->setPen(*colours.rbegin());

	painter->drawEllipse(-m_rad/2., -m_rad/2., m_rad, m_rad);
}

// ----------------------------------------------------------------------------




// ----------------------------------------------------------------------------

LinesScene::LinesScene(QWidget *parent) : QGraphicsScene(parent), m_parent{parent}
{
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


void LinesScene::SetIntersectionCalculationMethod(IntersectionCalculationMethod m)
{
	m_intersectioncalculationmethod = m;
	UpdateIntersections();
}


void LinesScene::UpdateAll()
{
	UpdateLines();
	UpdateIntersections();
	UpdateTrapezoids();
	UpdateVoro();
}


void LinesScene::UpdateLines()
{
	// remove previous lines
	for(QGraphicsItem* item : m_elems_lines)
	{
		removeItem(item);
		delete item;
	}
	m_elems_lines.clear();


	// get new lines
	m_lines.clear();
	if(m_elems_vertices.size() < 2)
		return;
	m_lines.reserve(m_elems_vertices.size()/2);

	for(std::size_t i=0; i<m_elems_vertices.size()-1; i+=2)
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


	for(const auto& line : m_lines)
	{
		const t_vec& vertex1 = line.first;
		const t_vec& vertex2 = line.second;

		QLineF qline{QPointF{vertex1[0], vertex1[1]}, QPointF{vertex2[0], vertex2[1]}};
		QGraphicsItem *item = addLine(qline, penEdge);
		m_elems_lines.push_back(item);
	}
}


void LinesScene::UpdateIntersections()
{
	// remove previous intersection points
	for(QGraphicsItem* item : m_elems_inters)
	{
		removeItem(item);
		delete item;
	}
	m_elems_inters.clear();


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

	for(const auto& intersection : intersections)
	{
		const t_vec& inters = std::get<2>(intersection);

		const t_real width = 14.;
		QRectF rect{inters[0]-width/2, inters[1]-width/2, width, width};
		QGraphicsItem *item = addEllipse(rect, pen, brush);
		m_elems_inters.push_back(item);
	}
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


void LinesScene::UpdateTrapezoids()
{
	// remove previous trapezoids
	for(QGraphicsItem* item : m_elems_trap)
	{
		removeItem(item);
		delete item;
	}
	m_elems_trap.clear();

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
}


void LinesScene::UpdateVoroImage(const QTransform& trafoSceneToVP)
{
	QTransform trafoVPToScene = trafoSceneToVP.inverted();

	if(!m_elem_voro)
		return;

	unsigned int num_threads = std::thread::hardware_concurrency();
	if(num_threads > 8)
		num_threads = 8;
	asio::thread_pool tp{num_threads};

	std::vector<std::shared_ptr<std::packaged_task<void()>>> packages;
	std::mutex mtx;

	const int width = m_elem_voro->width();
	const int height = m_elem_voro->height();
	std::unordered_map<std::size_t, QColor> linecolours;

	QProgressDialog progdlg(m_parent);
	progdlg.setWindowModality(Qt::WindowModal);
	progdlg.setMinimum(0);
	progdlg.setMaximum(height);
	QString msg = QString("Calculating Voronoi regions in %1 threads...").arg(num_threads);
	progdlg.setLabel(new QLabel(msg));

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

		t_real dist = dist_pt_line(pt, line.first, line.second, true);
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
	// remove previous voronoi diagram
	for(QGraphicsItem* item : m_elems_voro)
	{
		removeItem(item);
		delete item;
	}
	m_elems_voro.clear();

	// don't calculate if disabled or if there are intersections
	if(!m_calcvoro && !m_calcvorovertex)
		return;
	if(m_stoponinters && m_elems_inters.size())
		return;

	// get vertices and bisectors
	auto [vertices, linear_edges, all_parabolic_edges, graph]
		= geo::calc_voro<t_vec, std::pair<t_vec, t_vec>, t_graph>(m_lines);
	m_vorograph = std::move(graph);

	if(m_calcvoro)
	{
		// linear voronoi edges
		QPen penLinEdge;
		penLinEdge.setStyle(Qt::SolidLine);
		penLinEdge.setWidthF(1.);
		penLinEdge.setColor(QColor::fromRgbF(0.,0.,0.));

		for(const auto& linear_edge : linear_edges)
		{
			QLineF line{
				QPointF{std::get<0>(linear_edge)[0], std::get<0>(linear_edge)[1]},
				QPointF{std::get<1>(linear_edge)[0], std::get<1>(linear_edge)[1]} };
			QGraphicsItem *item = addLine(line, penLinEdge);
			m_elems_voro.push_back(item);
		}

		// parabolic voronoi edges
		QPen penParaEdge = penLinEdge;

		for(const auto& parabolic_edges : all_parabolic_edges)
		{
			QPolygonF poly;
			poly.reserve(parabolic_edges.size());
			for(const auto& parabolic_edge : parabolic_edges)
				poly << QPointF{parabolic_edge[0], parabolic_edge[1]};

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

		for(const auto& vertex : vertices)
		{
			const t_real width = 8.;
			QRectF rect{vertex[0]-width/2, vertex[1]-width/2, width, width};
			QGraphicsItem *item = addEllipse(rect, penVertex, brushVertex);
			m_elems_voro.push_back(item);
		}
	}
}

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------

LinesView::LinesView(LinesScene *scene, QWidget *parent) : QGraphicsView(scene, parent),
	m_scene{scene}
{
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	setInteractive(true);
	setMouseTracking(true);
}


LinesView::~LinesView()
{
}


void LinesView::resizeEvent(QResizeEvent *evt)
{
	int widthView = evt->size().width();
	int heightView = evt->size().height();

	QPointF pt1{mapToScene(QPoint{0,0})};
	QPointF pt2{mapToScene(QPoint{widthView, heightView})};

	// include bounds given by vertices
	const double padding = 16;
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
}



void LinesView::mousePressEvent(QMouseEvent *evt)
{
	QPoint posVP = evt->pos();
	QPointF posScene = mapToScene(posVP);

	QList<QGraphicsItem*> items = this->items(posVP);
	QGraphicsItem* item = nullptr;
	bool item_is_vertex = false;
	auto &verts = m_scene->GetVertexElems();

	for(int itemidx=0; itemidx<items.size(); ++itemidx)
	{
		item = items[itemidx];
		auto iter = std::find(verts.begin(), verts.end(), static_cast<Vertex*>(item));
		item_is_vertex = (iter != verts.end());
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
			auto iter = std::find(verts.begin(), verts.end(), static_cast<Vertex*>(item));

			std::size_t idx = iter - verts.begin();
			if(iter != verts.end())
				iter = verts.erase(iter);
			delete item;

			// move remaining vertex of line to the end
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
		QResizeEvent evt{size(), size()};
		resizeEvent(&evt);
		m_scene->UpdateAll();
	}

	QPoint posVP = evt->pos();
	QPointF posScene = mapToScene(posVP);
	emit SignalMouseCoordinates(posScene.x(), posScene.y(), posVP.x(), posVP.y());
}


void LinesView::wheelEvent(QWheelEvent *evt)
{
	//t_real s = std::pow(2., evt->angleDelta().y() / 1000.);
	//scale(s, s);
	QGraphicsView::wheelEvent(evt);
}


void LinesView::drawBackground(QPainter* painter, const QRectF& rect)
{
	// hack, because the background brush is drawn with respect to scene (0,0), not vp (0,0)
	// TODO: handle scene-viewport trafos other than translations
	if(m_scene->GetVoroImage())
		painter->drawImage(mapToScene(QPoint(0,0)), *m_scene->GetVoroImage());
	else
		QGraphicsView::drawBackground(painter, rect);
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------

LinesWnd::LinesWnd(QWidget* pParent) : QMainWindow{pParent},
	m_scene{new LinesScene{this}},
	m_view{new LinesView{m_scene.get(), this}},
	m_statusLabel{std::make_shared<QLabel>(this)}
{
	// ------------------------------------------------------------------------
	// restore settings
	if(m_sett.contains("wnd_geo"))
	{
		QByteArray arr{m_sett.value("wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		resize(1024, 768);
	}
	if(m_sett.contains("wnd_state"))
	{
		QByteArray arr{m_sett.value("wnd_state").toByteArray()};
		this->restoreState(arr);
	}
	// ------------------------------------------------------------------------


	m_view->setRenderHints(QPainter::Antialiasing);

	setWindowTitle("Line Segments");
	setCentralWidget(m_view.get());

	QStatusBar *statusBar = new QStatusBar{this};
	statusBar->addPermanentWidget(m_statusLabel.get(), 1);
	setStatusBar(statusBar);


	// menu actions
	QAction *actionNew = new QAction{"New", this};
	connect(actionNew, &QAction::triggered, [this]()
	{ m_scene->ClearVertices(); });

	QAction *actionLoad = new QAction{"Open...", this};
	connect(actionLoad, &QAction::triggered, [this]()
	{
		if(QString file = QFileDialog::getOpenFileName(this, "Open Data", "",
			"XML Files (*.xml);;All Files (* *.*)"); file!="")
		{
			std::ifstream ifstr(file.toStdString());
			if(!ifstr)
			{
				QMessageBox::critical(this, "Error", "File could not be opened for loading.");
				return;
			}

			m_scene->ClearVertices();

			ptree::ptree prop{};
			ptree::read_xml(ifstr, prop);

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

			if(vertidx > 0)
				m_scene->UpdateAll();
			else
				QMessageBox::warning(this, "Warning", "File contains no data.");
		}
	});

	QAction *actionSaveAs = new QAction{"Save as...", this};
	connect(actionSaveAs, &QAction::triggered, [this]()
	{
		if(QString file = QFileDialog::getSaveFileName(this, "Save Data", "",
			"XML Files (*.xml);;All Files (* *.*)"); file!="")
		{
			std::ofstream ofstr(file.toStdString());
			if(!ofstr)
			{
				QMessageBox::critical(this, "Error", "File could not be opened for saving.");
				return;
			}

			ptree::ptree prop{};

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
		}
	});

	QAction *actionExportSvg = new QAction{"Export SVG...", this};
	connect(actionExportSvg, &QAction::triggered, [this]()
	{
		if(QString file = QFileDialog::getSaveFileName(this, "Export SVG", "",
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
		if(QString file = QFileDialog::getSaveFileName(this, "Export DOT", "",
			"DOT Files (*.dot);;All Files (* *.*)"); file!="")
		{
			const auto& graph = m_scene->GetVoroGraph();

			std::ofstream ofstr(file.toStdString());
			print_graph(graph, ofstr);
			ofstr << std::endl;
		}
	});

	QAction *actionQuit = new QAction{"Quit", this};
	actionQuit->setMenuRole(QAction::QuitRole);
	connect(actionQuit, &QAction::triggered, [this]() { this->close(); });


	QAction *actionZoomIn = new QAction{"Zoom in", this};
	connect(actionZoomIn, &QAction::triggered, [this]()
	{
		if(m_view)
			m_view->scale(2., 2.);
	});

	QAction *actionZoomOut = new QAction{"Zoom out", this};
	connect(actionZoomOut, &QAction::triggered, [this]()
	{
		if(m_view)
			m_view->scale(0.5, 0.5);
	});


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


	QAction *actionIntersDirect = new QAction{"Direct", this};
	actionIntersDirect->setCheckable(true);
	actionIntersDirect->setChecked(false);
	connect(actionIntersDirect, &QAction::toggled, [this]()
	{ m_scene->SetIntersectionCalculationMethod(IntersectionCalculationMethod::DIRECT); });

	QAction *actionIntersSweep = new QAction{"Sweep", this};
	actionIntersSweep->setCheckable(true);
	actionIntersSweep->setChecked(true);
	connect(actionIntersSweep, &QAction::toggled, [this]()
	{ m_scene->SetIntersectionCalculationMethod(IntersectionCalculationMethod::SWEEP); });


	QAction *actionStopOnInters = new QAction{"Stop on Intersections", this};
	actionStopOnInters->setCheckable(true);
	actionStopOnInters->setChecked(m_scene->GetStopOnInters());
	connect(actionStopOnInters, &QAction::toggled, [this](bool b)
	{ m_scene->SetStopOnInters(b); });


	QActionGroup *groupInters = new QActionGroup{this};
	groupInters->addAction(actionIntersDirect);
	groupInters->addAction(actionIntersSweep);

	QAction *actionAboutQt = new QAction(QIcon::fromTheme("help-about"), "About Qt Libraries...", this);
	QAction *actionAbout = new QAction(QIcon::fromTheme("help-about"), "About Program...", this);

	actionAboutQt->setMenuRole(QAction::AboutQtRole);
	actionAbout->setMenuRole(QAction::AboutRole);

	connect(actionAboutQt, &QAction::triggered, this, []() { qApp->aboutQt(); });

	connect(actionAbout, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgAbout)
			this->m_dlgAbout = std::make_shared<AboutDlg>(this, &m_sett);

		m_dlgAbout->show();
		m_dlgAbout->raise();
		m_dlgAbout->activateWindow();
	});


	// shortcuts
	actionNew->setShortcut(QKeySequence::New);
	actionLoad->setShortcut(QKeySequence::Open);
	//actionSave->setShortcut(QKeySequence::Save);
	actionSaveAs->setShortcut(QKeySequence::SaveAs);
	actionQuit->setShortcut(QKeySequence::Quit);
	actionZoomIn->setShortcut(QKeySequence::ZoomIn);
	actionZoomOut->setShortcut(QKeySequence::ZoomOut);


	// menu
	QMenu *menuFile = new QMenu{"File", this};
	QMenu *menuView = new QMenu{"View", this};
	QMenu *menuCalc = new QMenu{"Calculate", this};
	QMenu *menuOptions = new QMenu{"Options", this};
	QMenu *menuBack = new QMenu{"Intersection Backend", this};
	QMenu *menuHelp = new QMenu("Help", this);

	menuFile->addAction(actionNew);
	menuFile->addSeparator();
	menuFile->addAction(actionLoad);
	menuFile->addAction(actionSaveAs);
	menuFile->addSeparator();
	menuFile->addAction(actionExportSvg);
	menuFile->addAction(actionExportGraph);
	menuFile->addSeparator();
	menuFile->addAction(actionQuit);

	menuView->addAction(actionZoomIn);
	menuView->addAction(actionZoomOut);

	menuCalc->addAction(actionVoronoiRegions);
	menuCalc->addAction(actionVoronoiVertices);
	menuCalc->addSeparator();
	menuCalc->addAction(actionTrap);
	menuCalc->addSeparator();
	menuCalc->addAction(actionVoroBitmap);

	menuBack->addAction(actionIntersDirect);
	menuBack->addAction(actionIntersSweep);

	menuOptions->addAction(actionStopOnInters);
	menuOptions->addSeparator();
	menuOptions->addMenu(menuBack);

	menuHelp->addAction(actionAboutQt);
	menuHelp->addSeparator();
	menuHelp->addAction(actionAbout);


	// menu bar
	QMenuBar *menuBar = new QMenuBar{this};
	menuBar->setNativeMenuBar(false);
	menuBar->addMenu(menuFile);
	menuBar->addMenu(menuView);
	menuBar->addMenu(menuCalc);
	menuBar->addMenu(menuOptions);
	//menuBar->addMenu(menuBack);
	menuBar->addMenu(menuHelp);
	setMenuBar(menuBar);


	// connections
	connect(m_view.get(), &LinesView::SignalMouseCoordinates,
	[this](double x, double y, double vpx, double vpy)
	{
		SetStatusMessage(QString("Scene: x=%1, y=%2, Viewport: x=%3, y=%4.")
			.arg(x, 5).arg(y, 5).arg(vpx, 5).arg(vpy, 5));
	});


	SetStatusMessage("Ready.");
}


void LinesWnd::SetStatusMessage(const QString& msg)
{
	m_statusLabel->setText(msg);
}


void LinesWnd::closeEvent(QCloseEvent *e)
{
	// ------------------------------------------------------------------------
	// save settings
	QByteArray geo{this->saveGeometry()}, state{this->saveState()};
	m_sett.setValue("wnd_geo", geo);
	m_sett.setValue("wnd_state", state);
	// ------------------------------------------------------------------------

	QMainWindow::closeEvent(e);
}


LinesWnd::~LinesWnd()
{
}

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
int main(int argc, char** argv)
{
	try
	{
		auto app = std::make_unique<QApplication>(argc, argv);
		app->setOrganizationName("tw");
		app->setApplicationName("lines");
		tl2::set_locales();

		auto vis = std::make_unique<LinesWnd>();
		vis->show();
		vis->raise();
		vis->activateWindow();

		return app->exec();
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
	}

	return -1;
}
// ----------------------------------------------------------------------------
