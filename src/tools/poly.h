/**
 * polygon splitting and kernel calculation test program
 * @author Tobias Weber <tweber@ill.fr>
 * @date 11-nov-2020
 * @note Forked on 4-aug-2021 from my privately developed "geo" project (https://github.com/t-weber/geo).
 * @license see 'LICENSE' file
 */

#ifndef __POLY_GUI_H__
#define __POLY_GUI_H__


#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsItem>
#include <QtCore/QSettings>

#include <memory>
#include <vector>

#include "about.h"
#include "vertex.h"

#include "src/libs/lines.h"
#include "src/libs/voronoi_lines.h"


class PolyView : public QGraphicsView
{Q_OBJECT
public:
	using t_vec = ::t_vec2;
	using t_mat = ::t_mat22;

	static const constexpr t_real g_eps = 1e-5;


public:
	PolyView(QGraphicsScene *scene=nullptr, QWidget *parent=nullptr);
	virtual ~PolyView();

	PolyView(PolyView&) = delete;
	const PolyView& operator=(const PolyView&) const = delete;

	void AddVertex(const QPointF& pos);
	void ClearVertices();

	const std::vector<Vertex*>& GetVertexElems() const { return m_elems_vertices; }
	std::vector<Vertex*>& GetVertexElems() { return m_elems_vertices; }

	void UpdateAll();
	void UpdateEdges();
	void UpdateSplitPolygon();
	void UpdateKer();

	void SetSortVertices(bool b);
	bool GetSortVertices() const { return m_sortvertices; }

	void SetCalcSplitPolygon(bool b);
	bool GetCalcSplitPolygon() const { return m_splitpolygon; }

	void SetCalcKernel(bool b);
	bool GetCalcKernel() const { return m_calckernel; }

protected:
	virtual void mousePressEvent(QMouseEvent *evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *evt) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;

	virtual void resizeEvent(QResizeEvent *evt) override;

private:
	QGraphicsScene *m_scene = nullptr;

	std::vector<Vertex*> m_elems_vertices{};
	std::vector<QGraphicsItem*> m_elems_edges{}, m_elems_ker{}, m_elems_split{};

	bool m_dragging = false;

	std::vector<t_vec> m_vertices{};

	bool m_sortvertices = true;
	bool m_splitpolygon = true;
	bool m_calckernel = true;

signals:
	void SignalMouseCoordinates(double x, double y);
	void SignalError(const QString& err);
};



class PolyWnd : public QMainWindow
{
public:
	using QMainWindow::QMainWindow;

	PolyWnd(QWidget* pParent = nullptr);
	~PolyWnd();

	void SetStatusMessage(const QString& msg);

private:
	virtual void closeEvent(QCloseEvent *) override;

private:
	QSettings m_sett{"geo_tools", "polygon"};
	std::shared_ptr<AboutDlg> m_dlgAbout;

	std::shared_ptr<QGraphicsScene> m_scene;
	std::shared_ptr<PolyView> m_view;
	std::shared_ptr<QLabel> m_statusLabel;
};


#endif