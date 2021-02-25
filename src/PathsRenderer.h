/**
 * paths rendering widget
 * @author Tobias Weber <tweber@ill.fr>
 * @date feb-2021
 * @license GPLv3, see 'LICENSE' file
 *
 * References:
 *   - http://doc.qt.io/qt-5/qopenglwidget.html#details
 *   - http://code.qt.io/cgit/qt/qtbase.git/tree/examples/opengl/threadedqopenglwidget
 */

#ifndef __PATHS_WIDGET_H__
#define __PATHS_WIDGET_H__

#include <QtCore/QMutex>
#include <QtCore/QTimer>

#include <QtWidgets/QOpenGLWidget>

#include <QtGui/QMouseEvent>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLBuffer>

#include <QtGui/QMatrix4x4>
#include <QtGui/QVector4D>
#include <QtGui/QVector3D>

#include <memory>
#include <chrono>
#include <atomic>
#include <unordered_map>

#include "tlibs2/libs/math20.h"
#include "tlibs2/libs/glplot.h"

#include "Instrument.h"



struct PathsObj : public GlRenderObj
{
	t_mat_gl m_mat = tl2::unit<t_mat_gl>();

	bool m_visible = true;		// object shown?
	bool m_highlighted = false;	// object highlighted?
	bool m_cull = true;			// object faces culled?

	t_vec3_gl m_labelPos = tl2::create<t_vec3_gl>({0., 0., 0.});
	std::string m_label;
	std::string m_datastr;

	t_vec3_gl m_boundingSpherePos = tl2::create<t_vec3_gl>({ 0., 0., 0. });
	t_real_gl m_boundingSphereRad = 0.;
};



// forward declarations
class PathsRenderer;



// ----------------------------------------------------------------------------
/**
 * rendering widget
 */
class PathsRenderer : public QOpenGLWidget
{ Q_OBJECT

public:
	PathsRenderer(QWidget *pParent = nullptr);
	virtual ~PathsRenderer();

	void Clear();
	void LoadInstrument(const Instrument& instr);


protected:
	virtual void paintEvent(QPaintEvent*) override;
	virtual void initializeGL() override;
	virtual void paintGL() override;
	virtual void resizeGL(int w, int h) override;

	virtual void mouseMoveEvent(QMouseEvent *pEvt) override;
	virtual void mousePressEvent(QMouseEvent *Evt) override;
	virtual void mouseReleaseEvent(QMouseEvent *Evt) override;
	virtual void wheelEvent(QWheelEvent *pEvt) override;


private:
	QMutex m_mutexObj{QMutex::Recursive};

	bool m_mouseMovedBetweenDownAndUp = 0;
	bool m_mouseDown[3] = {0,0,0};


protected slots:
	void tick();


public slots:
	void EnablePicker(bool b);


signals:
	void AfterGLInitialisation();

	void MouseDown(bool left, bool mid, bool right);
	void MouseUp(bool left, bool mid, bool right);
	void MouseClick(bool left, bool mid, bool right);

	void BasePlaneCoordsChanged(t_real_gl x, t_real_gl y);
	void PickerIntersection(const t_vec3_gl* pos, std::string obj_name, const t_vec3_gl* posSphere);


protected:
	// ------------------------------------------------------------------------
	// shader interface
	// ------------------------------------------------------------------------
	std::shared_ptr<QOpenGLShaderProgram> m_pShaders;

	// vertex attributes
	GLint m_attrVertex = -1;
	GLint m_attrVertexNorm = -1;
	GLint m_attrVertexCol = -1;
	GLint m_attrTexCoords = -1;

	// lighting
	GLint m_uniConstCol = -1;
	GLint m_uniLightPos = -1;
	GLint m_uniNumActiveLights = -1;

	// matrices
	GLint m_uniMatrixProj = -1;
	GLint m_uniMatrixCam = -1;
	GLint m_uniMatrixCamInv = -1;
	GLint m_uniMatrixObj = -1;

	// cursor
	GLint m_uniCursorActive = -1;
	GLint m_uniCursorCoords = -1;
	// ------------------------------------------------------------------------

	// version identifiers
	std::string m_strGlVer, m_strGlShaderVer, m_strGlVendor, m_strGlRenderer;

	// cursor uv coordinates
	GLfloat m_curCursorUV[2] = {0., 0.};

	// matrices
	t_mat_gl m_matPerspective = tl2::unit<t_mat_gl>();
	t_mat_gl m_matPerspective_inv = tl2::unit<t_mat_gl>();
	t_mat_gl m_matViewport = tl2::unit<t_mat_gl>();
	t_mat_gl m_matViewport_inv = tl2::unit<t_mat_gl>();
	t_mat_gl m_matCamBase = tl2::create<t_mat_gl>({1,0,0,0,  0,1,0,0,  0,0,1,-5,  0,0,0,1});
	t_mat_gl m_matCamRot = tl2::unit<t_mat_gl>();
	t_mat_gl m_matCam = tl2::unit<t_mat_gl>();
	t_mat_gl m_matCam_inv = tl2::unit<t_mat_gl>();

	t_vec_gl m_vecCamX = tl2::create<t_vec_gl>({1.,0.,0.,0.});
	t_vec_gl m_vecCamY = tl2::create<t_vec_gl>({0.,0.,1.,0.});

	t_real_gl m_phi_saved = 0, m_theta_saved = 0;
	t_real_gl m_zoom = 1.;
	t_real_gl m_CoordMax = 2.5;		// extent of coordinate axes

	std::atomic<bool> m_bPlatformSupported = true;
	std::atomic<bool> m_bInitialised = false;
	std::atomic<bool> m_bWantsResize = false;
	std::atomic<bool> m_bPickerEnabled = true;
	std::atomic<bool> m_bPickerNeedsUpdate = false;
	std::atomic<bool> m_bLightsNeedUpdate = false;
	std::atomic<int> m_iScreenDims[2] = { 800, 600 };
	t_real_gl m_pickerSphereRadius = 1;

	std::vector<t_vec3_gl> m_lights;
	std::unordered_map<std::string, PathsObj> m_objs;

	QPointF m_posMouse;
	QPointF m_posMouseRotationStart, m_posMouseRotationEnd;
	bool m_bInRotation = false;

	QTimer m_timer;


protected:
	void UpdateCam();
	void UpdatePicker();
	void UpdateLights();

	void DoPaintGL(qgl_funcs *pGL);
	void DoPaintNonGL(QPainter &painter);

	void tick(const std::chrono::milliseconds& ms);


public:
	std::tuple<std::string, std::string, std::string, std::string> GetGlDescr() const;
	bool IsInitialised() const { return m_bInitialised; }

	QPointF GlToScreenCoords(const t_vec_gl& vec, bool *pVisible=nullptr);

	void SetCamBase(const t_mat_gl& mat, const t_vec_gl& vecX, const t_vec_gl& vecY)
	{ m_matCamBase = mat; m_vecCamX = vecX; m_vecCamY = vecY; UpdateCam(); }
	void SetPickerSphereRadius(t_real_gl rad) { m_pickerSphereRadius = rad; }

	void DeleteObject(PathsObj& obj);
	void DeleteObject(const std::string& obj_name);

	void AddTriangleObject(const std::string& obj_name,
		const std::vector<t_vec3_gl>& triag_verts,
		const std::vector<t_vec3_gl>& triag_norms, const std::vector<t_vec3_gl>& triag_uvs,
		t_real_gl r=0, t_real_gl g=0, t_real_gl b=0, t_real_gl a=1);

	void AddBasePlane(const std::string& obj_name, t_real_gl len_x=10, t_real_gl len_y=10);
	void AddCoordinateCross(const std::string& obj_name);

	void SetCoordMax(t_real_gl d) { m_CoordMax = d; }

	void SetLight(std::size_t idx, const t_vec3_gl& pos);
};
// ----------------------------------------------------------------------------


#endif
