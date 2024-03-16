/**
 * polygon splitting and kernel calculation test program
 * @author Tobias Weber <tweber@ill.fr>
 * @date 11-nov-2020
 * @note Forked on 4-aug-2021 from my privately developed "geo" project (https://github.com/t-weber/geo).
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

#include "poly.h"

#include <QtCore/QDir>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
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

#include "tlibs2/libs/algos.h"
#include "tlibs2/libs/str.h"


#define GEOTOOLS_SHOW_MESSAGE


// ----------------------------------------------------------------------------

PolyView::PolyView(QGraphicsScene *scene, QWidget *parent) : QGraphicsView(scene, parent),
	m_scene{scene}
{
	setCacheMode(QGraphicsView::CacheBackground);

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	setInteractive(true);
	setMouseTracking(true);

	//setSceneRect(mapToScene(viewport()->rect()).boundingRect());
	setBackgroundBrush(QBrush{QColor::fromRgbF(0.95, 0.95, 0.95, 1.)});
}


PolyView::~PolyView()
{
}


void PolyView::drawBackground(QPainter* painter, const QRectF& rect)
{
	QGraphicsView::drawBackground(painter, rect);
}


void PolyView::drawForeground(QPainter* painter, const QRectF& rect)
{
	QGraphicsView::drawForeground(painter, rect);

#ifdef GEOTOOLS_SHOW_MESSAGE
	if(!m_vertices.size())
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


void PolyView::resizeEvent(QResizeEvent *evt)
{
	QPointF pt1{mapToScene(QPoint{0,0})};
	QPointF pt2{mapToScene(QPoint{evt->size().width(), evt->size().height()})};

	const double padding = 16;

	// include bounds given by vertices
	for(const Vertex* vertex : m_elems_vertices)
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
}


void PolyView::AddVertex(const QPointF& pos)
{
	Vertex *vertex = new Vertex{pos};
	m_elems_vertices.push_back(vertex);
	m_scene->addItem(vertex);
}


void PolyView::mousePressEvent(QMouseEvent *evt)
{
	QPoint posVP = evt->pos();
	QPointF posScene = mapToScene(posVP);

	QList<QGraphicsItem*> items = this->items(posVP);
	QGraphicsItem* item = nullptr;
	bool item_is_vertex = false;

	for(int itemidx=0; itemidx<items.size(); ++itemidx)
	{
		item = items[itemidx];
		auto iter = std::find(m_elems_vertices.begin(), m_elems_vertices.end(), static_cast<Vertex*>(item));
		item_is_vertex = (iter != m_elems_vertices.end());
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
			AddVertex(posScene);
			m_dragging = true;
			UpdateAll();
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
			auto iter = std::find(m_elems_vertices.begin(), m_elems_vertices.end(), static_cast<Vertex*>(item));
			if(iter != m_elems_vertices.end())
			{
				m_scene->removeItem(item);
				delete item;
				iter = m_elems_vertices.erase(iter);
				UpdateAll();
			}
		}
	}

	QGraphicsView::mousePressEvent(evt);
}


void PolyView::mouseReleaseEvent(QMouseEvent *evt)
{
	if(evt->button() == Qt::LeftButton)
		m_dragging = false;

	UpdateAll();
	QGraphicsView::mouseReleaseEvent(evt);
}


void PolyView::mouseMoveEvent(QMouseEvent *evt)
{
	QGraphicsView::mouseMoveEvent(evt);

	if(m_dragging)
	{
		QResizeEvent evt{size(), size()};
		resizeEvent(&evt);
		UpdateAll();
	}

	QPoint posVP = evt->pos();
	QPointF posScene = mapToScene(posVP);
	emit SignalMouseCoordinates(posScene.x(), posScene.y());
}


void PolyView::ClearVertices()
{
	for(Vertex* vertex : m_elems_vertices)
	{
		m_scene->removeItem(vertex);
		delete vertex;
	}
	m_elems_vertices.clear();

	UpdateAll();
}


/**
 * calculate everything
 */
void PolyView::UpdateAll()
{
	// get vertices
	m_vertices.clear();
	m_vertices.reserve(m_elems_vertices.size());
	std::transform(m_elems_vertices.begin(), m_elems_vertices.end(), std::back_inserter(m_vertices),
		[](const Vertex* vert) -> t_vec { return tl2::create<t_vec>({vert->x(), vert->y()}); } );

	if(m_sortvertices)
		std::tie(m_vertices, std::ignore) = geo::sort_vertices_by_angle<t_vec>(m_vertices);

	UpdateEdges();
	UpdateSplitPolygon();
	UpdateKer();

#ifdef GEOTOOLS_SHOW_MESSAGE
	// set or reset the initial text
	if(m_vertices.size() < 2)
		m_scene->update();
#endif
}


/**
 * draw the polygon edges
 */
void PolyView::UpdateEdges()
{
	// remove previous edges
	for(QGraphicsItem* item : m_elems_edges)
	{
		m_scene->removeItem(item);
		delete item;
	}
	m_elems_edges.clear();


	QPen penEdge;
	penEdge.setStyle(Qt::SolidLine);
	penEdge.setWidthF(2.);
	penEdge.setColor(QColor::fromRgbF(0., 0., 1.));

	const std::size_t num_vertices = m_vertices.size();
	m_elems_edges.reserve(num_vertices);

	for(std::size_t vertidx = 0; vertidx < num_vertices; ++vertidx)
	{
		std::size_t vertidx2 = (vertidx+1) % num_vertices;

		const t_vec& vertex1 = m_vertices[vertidx];
		const t_vec& vertex2 = m_vertices[vertidx2];

		QLineF line{QPointF{vertex1[0], vertex1[1]}, QPointF{vertex2[0], vertex2[1]}};
		QGraphicsItem *item = m_scene->addLine(line, penEdge);
		m_elems_edges.push_back(item);
	}
}


/**
 * split the polygon into convex regions
 */
void PolyView::UpdateSplitPolygon()
{
	try
	{
		// remove previous split poly
		for(QGraphicsItem* item : m_elems_split)
		{
			m_scene->removeItem(item);
			delete item;
		}
		m_elems_split.clear();

		if(!m_splitpolygon)
			return;

		auto splitpolys = geo::convex_split<t_vec>(m_vertices, g_eps);

		// already convex?
		if(splitpolys.size() == 0)
			return;

		QPen penKer;
		penKer.setStyle(Qt::SolidLine);
		penKer.setWidthF(2.);
		penKer.setColor(QColor::fromRgbF(0., 0., 1., 1.));

		QBrush brushKer;
		brushKer.setColor(QColor::fromRgbF(0., 0., 1., 0.1));
		brushKer.setStyle(Qt::SolidPattern);

		for(const auto& splitpoly : splitpolys)
		{
			QPolygonF poly;
			for(std::size_t vertidx = 0; vertidx < splitpoly.size(); ++vertidx)
			{
				const t_vec& vertex1 = splitpoly[vertidx];
				poly << QPointF(vertex1[0], vertex1[1]);
			}

			QGraphicsItem *item = m_scene->addPolygon(poly, penKer, brushKer);
			m_elems_split.push_back(item);
		}
	}
	catch(const std::exception& ex)
	{
		std::ostringstream ostrErr;
		ostrErr << "Error: " << ex.what();

		std::cerr << ostrErr.str() << std::endl;
		emit SignalError(ostrErr.str().c_str());
	}
}


/**
 * calculate the kernel of the polygon
 */
void PolyView::UpdateKer()
{
	// remove previous vis poly
	for(QGraphicsItem* item : m_elems_ker)
	{
		m_scene->removeItem(item);
		delete item;
	}
	m_elems_ker.clear();

	if(!m_calckernel)
		return;

	std::vector<t_vec> verts_reversed;
	verts_reversed.reserve(m_vertices.size());
	for(auto iter=m_vertices.rbegin(); iter!=m_vertices.rend(); ++iter)
		verts_reversed.push_back(*iter);

	auto kerpoly = geo::calc_ker<t_vec>(m_vertices, g_eps);
	auto kerpoly_reversed = geo::calc_ker<t_vec>(verts_reversed, g_eps);

	// in case the vertices were inserted in reversed order
	if(kerpoly_reversed.size() > kerpoly.size())
		kerpoly = kerpoly_reversed;


	QPen penKer;
	penKer.setStyle(Qt::SolidLine);
	penKer.setWidthF(2.);
	penKer.setColor(QColor::fromRgbF(1., 0., 0., 1.));

	QBrush brushKer;
	brushKer.setColor(QColor::fromRgbF(1., 0., 0., 0.1));
	brushKer.setStyle(Qt::SolidPattern);

	QPolygonF poly;
	for(std::size_t vertidx = 0; vertidx < kerpoly.size(); ++vertidx)
	{
		const t_vec& vertex1 = kerpoly[vertidx];
		poly << QPointF(vertex1[0], vertex1[1]);
	}

	QGraphicsItem *item = m_scene->addPolygon(poly, penKer, brushKer);
	m_elems_ker.push_back(item);
}


void PolyView::SetSortVertices(bool b)
{
	m_sortvertices = b;
	UpdateAll();
}


void PolyView::SetCalcSplitPolygon(bool b)
{
	m_splitpolygon = b;
	UpdateAll();
}


void PolyView::SetCalcKernel(bool b)
{
	m_calckernel = b;
	UpdateAll();
}

// ----------------------------------------------------------------------------




// ----------------------------------------------------------------------------

PolyWnd::PolyWnd(QWidget* pParent) : QDialog{pParent},
	m_scene{new QGraphicsScene{this}},
	m_view{new PolyView{m_scene.get(), this}},
	m_statusLabel{std::make_shared<QLabel>(this)}
{
	// restore settings
#ifdef TASPATHS_TOOLS_STANDALONE
	// set-up common gui variables
	GeoSettingsDlg::SetGuiTheme(&g_theme);
	GeoSettingsDlg::SetGuiFont(&g_font);
	GeoSettingsDlg::SetGuiUseNativeMenubar(&g_use_native_menubar);
	GeoSettingsDlg::SetGuiUseNativeDialogs(&g_use_native_dialogs);

	GeoSettingsDlg::ReadSettings(&m_sett);
#endif

	m_view->SetSortVertices(
		m_sett.value("poly_sort_vertices", m_view->GetSortVertices()).toBool());

	m_view->setRenderHints(QPainter::Antialiasing);

	setWindowTitle("Polygons");

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
	connect(actionNew, &QAction::triggered, this, &PolyWnd::NewFile);

	QAction *actionLoad = new QAction{QIcon::fromTheme("document-open"), "Open...", this};
	connect(actionLoad, &QAction::triggered, this,
		static_cast<void (PolyWnd::*)()>(&PolyWnd::OpenFile));

	QAction *actionSave = new QAction{QIcon::fromTheme("document-save"), "Save", this};
	connect(actionSave, &QAction::triggered, this,
		static_cast<void (PolyWnd::*)()>(&PolyWnd::SaveFile));

	QAction *actionSaveAs = new QAction{QIcon::fromTheme("document-save-as"), "Save as...", this};
	connect(actionSaveAs, &QAction::triggered, this,
		static_cast<void (PolyWnd::*)()>(&PolyWnd::SaveFileAs));

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

		for(Vertex* vert : m_view->GetVertexElems())
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

		for(Vertex* vert : m_view->GetVertexElems())
		{
			t_real rad = vert->GetRadius();
			rad *= t_real(0.5);
			vert->SetRadius(rad);
		}

		m_scene->update(QRectF());
	});


	QAction *actionSort = new QAction{"Sort Vertices", this};
	actionSort->setCheckable(true);
	actionSort->setChecked(m_view->GetSortVertices());
	connect(actionSort, &QAction::toggled, [this](bool b) { m_view->SetSortVertices(b); });

	QAction *actionSplit = new QAction{"Convex Regions", this};
	actionSplit->setCheckable(true);
	actionSplit->setChecked(m_view->GetCalcSplitPolygon());
	connect(actionSplit, &QAction::toggled, [this](bool b) { m_view->SetCalcSplitPolygon(b); });

	QAction *actionKer = new QAction{"Visibility Kernel", this};
	actionKer->setCheckable(true);
	actionKer->setChecked(m_view->GetCalcKernel());
	connect(actionKer, &QAction::toggled, [this](bool b) { m_view->SetCalcKernel(b); });


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

	menuCalc->addAction(actionSort);
	menuCalc->addSeparator();
	menuCalc->addAction(actionSplit);
	menuCalc->addAction(actionKer);

	menuHelp->addAction(actionAboutQt);
	menuHelp->addSeparator();
	menuHelp->addAction(actionAbout);


	// menu bar
	QMenuBar *menuBar = new QMenuBar{this};
	//menuBar->setNativeMenuBar(false);
	menuBar->addMenu(menuFile);
	menuBar->addMenu(menuView);
	menuBar->addMenu(menuCalc);
	menuBar->addMenu(menuHelp);
	layout->setMenuBar(menuBar);


	// ------------------------------------------------------------------------
	// restore settings
	if(m_sett.contains("poly_wnd_geo"))
	{
		QByteArray arr{m_sett.value("poly_wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		resize(800, 600);
	}

	// recent files
	if(m_sett.contains("poly_recent_files"))
		m_recent.SetRecentFiles(m_sett.value("poly_recent_files").toStringList());
	// ------------------------------------------------------------------------


	// connections
	connect(m_view.get(), &PolyView::SignalMouseCoordinates, [this](double x, double y) -> void
	{
		SetStatusMessage(QString("x=%1, y=%2.").arg(x, 5).arg(y, 5));
	});

	connect(m_view.get(), &PolyView::SignalError, this, &PolyWnd::SetStatusMessage);


	SetStatusMessage("Ready.");
}


/**
 * File -> New
 */
void PolyWnd::NewFile()
{
	SetCurrentFile("");

	m_view->ClearVertices();
}


/**
 * open file
 */
bool PolyWnd::OpenFile(const QString& file)
{
	std::ifstream ifstr(file.toStdString());
	if(!ifstr)
	{
		QMessageBox::critical(this, "Error", "File could not be opened for loading.");
		return false;
	}

	m_view->ClearVertices();

	ptree::ptree prop{};
	ptree::read_xml(ifstr, prop);

	std::size_t vertidx = 0;
	while(true)
	{
		std::ostringstream ostrVert;
		ostrVert << "vis2d.vertices." << vertidx;

		auto vertprop = prop.get_child_optional(ostrVert.str());
		if(!vertprop)
			break;

		auto vertx = vertprop->get_optional<t_real>("<xmlattr>.x");
		auto verty = vertprop->get_optional<t_real>("<xmlattr>.y");

		if(!vertx || !verty)
			break;

		m_view->AddVertex(QPointF{*vertx, *verty});

		++vertidx;
	}

	if(vertidx > 0)
	{
		m_view->UpdateAll();

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
void PolyWnd::OpenFile()
{
	QString dirLast = m_sett.value("cur_dir", QDir::homePath()).toString();

	if(QString file = QFileDialog::getOpenFileName(this,
		"Load Data", dirLast,
		"XML Files (*.xml);;All Files (* *.*)"); file!="")
	{
		OpenFile(file);
	}
}


/**
 * save file
 */
bool PolyWnd::SaveFile(const QString& file)
{
	std::ofstream ofstr(file.toStdString());
	if(!ofstr)
	{
		QMessageBox::critical(this, "Error", "File could not be opened for saving.");
		return false;
	}

	ptree::ptree prop{};
	prop.put("vis2d.ident", "takin_taspaths_poly");
	prop.put("vis2d.doi", "https://doi.org/10.5281/zenodo.4625649");
	prop.put("vis2d.timestamp", tl2::var_to_str(tl2::epoch<t_real>()));

	std::size_t vertidx = 0;
	for(const Vertex* vertex : m_view->GetVertexElems())
	{
		QPointF vertexpos = vertex->scenePos();

		std::ostringstream ostrX, ostrY;
		ostrX << "vis2d.vertices." << vertidx << ".<xmlattr>.x";
		ostrY << "vis2d.vertices." << vertidx << ".<xmlattr>.y";

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
void PolyWnd::SaveFile()
{
	if(m_recent.GetCurFile() == "")
		SaveFileAs();
	else
		SaveFile(m_recent.GetCurFile());
}


/**
 * File -> Save As
 */
void PolyWnd::SaveFileAs()
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
void PolyWnd::SetCurrentFile(const QString &file)
{
	m_recent.SetCurFile(file);
	this->setWindowFilePath(m_recent.GetCurFile());

	/*static const QString title(PROG_TITLE);
	if(m_recent.GetCurFile() == "")
		this->setWindowTitle(title);
	else
		this->setWindowTitle(title + " \u2014 " + m_recent.GetCurFile());*/
}


void PolyWnd::SetStatusMessage(const QString& msg)
{
	m_statusLabel->setText(msg);
}


void PolyWnd::closeEvent(QCloseEvent *e)
{
	// save settings
	QByteArray geo{this->saveGeometry()};
	m_sett.setValue("poly_wnd_geo", geo);
	m_sett.setValue("poly_sort_vertices", m_view->GetSortVertices());

	// save recent files
	m_recent.TrimEntries();
	m_sett.setValue("poly_recent_files", m_recent.GetRecentFiles());

	QDialog::closeEvent(e);
}


PolyWnd::~PolyWnd()
{
}

// ----------------------------------------------------------------------------
