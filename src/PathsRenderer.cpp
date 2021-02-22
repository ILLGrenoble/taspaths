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

#include "PathsRenderer.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QPainter>
#include <QtGui/QGuiApplication>
#include <QtCore/QtGlobal>

#include <iostream>
#include <boost/scope_exit.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/algorithm/string/replace.hpp>
namespace algo = boost::algorithm;

#include "tlibs2/libs/file.h"


// ----------------------------------------------------------------------------
// GL plot implementation

PathsRenderer::PathsRenderer(QWidget *pParent) : QOpenGLWidget(pParent)
{
	if constexpr(m_usetimer)
	{
		connect(&m_timer, &QTimer::timeout,
			this, static_cast<void (PathsRenderer::*)()>(&PathsRenderer::tick));
		m_timer.start(std::chrono::milliseconds(1000 / 60));
	}

	UpdateCam();

	//setUpdateBehavior(QOpenGLWidget::PartialUpdate);
	setMouseTracking(true);
}


PathsRenderer::~PathsRenderer()
{
	setMouseTracking(false);

	if constexpr(m_usetimer)
		m_timer.stop();

	makeCurrent();
	BOOST_SCOPE_EXIT(this_) { this_->doneCurrent(); } BOOST_SCOPE_EXIT_END

	// delete gl objects within current gl context
	m_pShaders.reset();

	qgl_funcs* pGl = get_gl_functions(this);
	for(auto &obj : m_objs)
	{
		obj.m_pvertexbuf.reset();
		obj.m_pnormalsbuf.reset();
		obj.m_pcolorbuf.reset();
		if(pGl) pGl->glDeleteVertexArrays(1, &obj.m_vertexarr);
	}

	m_objs.clear();
	LOGGLERR(pGl)
}


QPointF PathsRenderer::GlToScreenCoords(const t_vec_gl& vec4, bool *pVisible)
{
	auto [ vecPersp, vec ] =
		tl2::hom_to_screen_coords<t_mat_gl, t_vec_gl>
			(vec4, m_matCam, m_matPerspective, m_matViewport, true);

	// position not visible -> return a point outside the viewport
	if(vecPersp[2] > 1.)
	{
		if(pVisible) *pVisible = false;
		return QPointF(-1*m_iScreenDims[0], -1*m_iScreenDims[1]);
	}

	if(pVisible) *pVisible = true;
	return QPointF(vec[0], vec[1]);
}


t_mat_gl PathsRenderer::GetArrowMatrix(const t_vec_gl& vecTo, t_real_gl postscale, const t_vec_gl& vecPostTrans,
	const t_vec_gl& vecFrom, t_real_gl prescale, const t_vec_gl& vecPreTrans)
{
	t_mat_gl mat = tl2::unit<t_mat_gl>(4);

	mat *= tl2::hom_translation<t_mat_gl>(vecPreTrans[0], vecPreTrans[1], vecPreTrans[2]);
	mat *= tl2::hom_scaling<t_mat_gl>(prescale, prescale, prescale);

	mat *= tl2::rotation<t_mat_gl, t_vec_gl>(vecFrom, vecTo);

	mat *= tl2::hom_scaling<t_mat_gl>(postscale, postscale, postscale);
	mat *= tl2::hom_translation<t_mat_gl>(vecPostTrans[0], vecPostTrans[1], vecPostTrans[2]);

	return mat;
}


std::size_t PathsRenderer::AddTriangleObject(const std::vector<t_vec3_gl>& triag_verts,
	const std::vector<t_vec3_gl>& triag_norms,
	t_real_gl r, t_real_gl g, t_real_gl b, t_real_gl a)
{
	auto [boundingSpherePos, boundingSphereRad] = tl2::bounding_sphere<t_vec3_gl>(triag_verts);
	auto col = tl2::create<t_vec_gl>({r,g,b,a});

	QMutexLocker _locker{&m_mutexObj};

	PathsObj obj;
	create_triangle_object(this, obj, triag_verts, triag_verts, triag_norms, col, 
		false, m_attrVertex, m_attrVertexNorm, m_attrVertexCol);

	obj.m_mat = tl2::hom_translation<t_mat_gl>(0., 0., 0.);
	obj.m_boundingSpherePos = std::move(boundingSpherePos);
	obj.m_boundingSphereRad = boundingSphereRad;
	obj.m_labelPos = tl2::create<t_vec3_gl>({0., 0., 0.75});
	m_objs.emplace_back(std::move(obj));

	return m_objs.size()-1;		// object handle
}


std::size_t PathsRenderer::AddCoordinateCross(t_real_gl min, t_real_gl max)
{
	auto col = tl2::create<t_vec_gl>({0,0,0,1});
	auto verts = std::vector<t_vec3_gl>
	{{
		tl2::create<t_vec3_gl>({min,0,0}), tl2::create<t_vec3_gl>({max,0,0}),
		tl2::create<t_vec3_gl>({0,min,0}), tl2::create<t_vec3_gl>({0,max,0}),
		tl2::create<t_vec3_gl>({0,0,min}), tl2::create<t_vec3_gl>({0,0,max}),
	}};

	QMutexLocker _locker{&m_mutexObj};

	PathsObj obj;
	create_line_object(this, obj, verts, col, m_attrVertex, m_attrVertexCol);

	m_objs.emplace_back(std::move(obj));

	return m_objs.size()-1;		// object handle
}



void PathsRenderer::UpdateCam()
{
	m_matCam = m_matCamBase;
	m_matCam(2,3) /= m_zoom;
	m_matCam *= m_matCamRot;
	std::tie(m_matCam_inv, std::ignore) = tl2::inv<t_mat_gl>(m_matCam);

	m_bPickerNeedsUpdate = true;
	update();
}


void PathsRenderer::SetLight(std::size_t idx, const t_vec3_gl& pos)
{
	if(m_lights.size() < idx+1)
		m_lights.resize(idx+1);

	m_lights[idx] = pos;
	m_bLightsNeedUpdate = true;
}


void PathsRenderer::UpdateLights()
{
	constexpr int MAX_LIGHTS = 4;	// max. number allowed in shader

	int num_lights = std::min(MAX_LIGHTS, static_cast<int>(m_lights.size()));
	t_real_gl pos[num_lights * 3];

	for(int i=0; i<num_lights; ++i)
	{
		pos[i*3 + 0] = m_lights[i][0];
		pos[i*3 + 1] = m_lights[i][1];
		pos[i*3 + 2] = m_lights[i][2];
	}

	m_pShaders->setUniformValueArray(m_uniLightPos, pos, num_lights, 3);
	m_pShaders->setUniformValue(m_uniNumActiveLights, num_lights);

	m_bLightsNeedUpdate = false;
}


void PathsRenderer::EnablePicker(bool b)
{
	m_bPickerEnabled = b;
}


void PathsRenderer::UpdatePicker()
{
	if(!m_bInitialised || !m_bPlatformSupported || !m_bPickerEnabled) return;

	// picker ray
	auto [org, dir] = tl2::hom_line_from_screen_coords<t_mat_gl, t_vec_gl>(
		m_posMouse.x(), m_posMouse.y(), 0., 1., m_matCam_inv,
		m_matPerspective_inv, m_matViewport_inv, &m_matViewport, true);
	t_vec3_gl org3 = tl2::create<t_vec3_gl>({org[0], org[1], org[2]});
	t_vec3_gl dir3 = tl2::create<t_vec3_gl>({dir[0], dir[1], dir[2]});


	// intersection with unit sphere around origin
	bool hasSphereInters = false;
	t_vec_gl vecClosestSphereInters = tl2::create<t_vec_gl>({0,0,0,0});

	auto intersUnitSphere =
	tl2::intersect_line_sphere<t_vec3_gl, std::vector>(org3, dir3,
		tl2::create<t_vec3_gl>({0,0,0}), t_real_gl(m_pickerSphereRadius));
	for(const auto& result : intersUnitSphere)
	{
		t_vec_gl vecInters4 = tl2::create<t_vec_gl>({result[0], result[1], result[2], 1});

		if(!hasSphereInters)
		{	// first intersection
			vecClosestSphereInters = vecInters4;
			hasSphereInters = true;
		}
		else
		{	// test if next intersection is closer...
			t_vec_gl oldPosTrafo = m_matCam * vecClosestSphereInters;
			t_vec_gl newPosTrafo = m_matCam * vecInters4;

			// ... it is closer.
			if(tl2::norm(newPosTrafo) < tl2::norm(oldPosTrafo))
				vecClosestSphereInters = vecInters4;
		}
	}


	// intersection with geometry
	bool hasInters = false;
	t_vec_gl vecClosestInters = tl2::create<t_vec_gl>({0,0,0,0});
	std::size_t objInters = 0xffffffff;


	QMutexLocker _locker{&m_mutexObj};

	for(std::size_t curObj=0; curObj<m_objs.size(); ++curObj)
	{
		const auto& obj = m_objs[curObj];

		if(obj.m_type != GlRenderObjType::TRIANGLES || !obj.m_visible || !obj.m_valid)
			continue;


		t_mat_gl matTrafo = obj.m_mat;

		// scaling factor, TODO: maximum factor for non-uniform scaling
		auto scale = std::cbrt(std::abs(tl2::det(matTrafo)));

		// intersection with bounding sphere?
		auto boundingInters =
			tl2::intersect_line_sphere<t_vec3_gl, std::vector>(org3, dir3,
				matTrafo * obj.m_boundingSpherePos, scale*obj.m_boundingSphereRad);
		if(boundingInters.size() == 0)
			continue;


		// test actual polygons for intersection
		for(std::size_t startidx=0; startidx+2<obj.m_triangles.size(); startidx+=3)
		{
			std::vector<t_vec3_gl> poly{ {
				obj.m_triangles[startidx+0],
				obj.m_triangles[startidx+1],
				obj.m_triangles[startidx+2]
			} };

			auto [vecInters, bInters, lamInters] =
				tl2::intersect_line_poly<t_vec3_gl, t_mat_gl>(org3, dir3, poly, matTrafo);

			if(bInters)
			{
				t_vec_gl vecInters4 = tl2::create<t_vec_gl>({vecInters[0], vecInters[1], vecInters[2], 1});

				if(!hasInters)
				{	// first intersection
					vecClosestInters = vecInters4;
					objInters = curObj;
					hasInters = true;
				}
				else
				{	// test if next intersection is closer...
					t_vec_gl oldPosTrafo = m_matCam * vecClosestInters;
					t_vec_gl newPosTrafo = m_matCam * vecInters4;

					if(tl2::norm(newPosTrafo) < tl2::norm(oldPosTrafo))
					{	// ...it is closer
						vecClosestInters = vecInters4;
						objInters = curObj;
					}
				}
			}
		}
	}

	m_bPickerNeedsUpdate = false;
	t_vec3_gl vecClosestInters3 = tl2::create<t_vec3_gl>({vecClosestInters[0], vecClosestInters[1], vecClosestInters[2]});
	t_vec3_gl vecClosestSphereInters3 = tl2::create<t_vec3_gl>({vecClosestSphereInters[0], vecClosestSphereInters[1], vecClosestSphereInters[2]});

	emit PickerIntersection(hasInters ? &vecClosestInters3 : nullptr, objInters, hasSphereInters ? &vecClosestSphereInters3 : nullptr);
}



void PathsRenderer::tick()
{
	tick(std::chrono::milliseconds(1000 / 60));
}


void PathsRenderer::tick(const std::chrono::milliseconds& ms)
{
	// TODO
	UpdateCam();
}


/**
 * pure gl drawing
 */
void PathsRenderer::DoPaintGL(qgl_funcs *pGl)
{
	if(!pGl)
		return;

	// clear
	pGl->glClearColor(1., 1., 1., 1.);
	pGl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	pGl->glEnable(GL_DEPTH_TEST);


	// bind shaders
	m_pShaders->bind();
	BOOST_SCOPE_EXIT(m_pShaders) { m_pShaders->release(); } BOOST_SCOPE_EXIT_END
	LOGGLERR(pGl);

	if(m_bLightsNeedUpdate) UpdateLights();

	// set cam matrix
	m_pShaders->setUniformValue(m_uniMatrixCam, m_matCam);
	m_pShaders->setUniformValue(m_uniMatrixCamInv, m_matCam_inv);


	auto colOverride = tl2::create<t_vec_gl>({1,1,1,1});

	// render triangle geometry
	for(const auto& obj : m_objs)
	{
		// set override color to white
		m_pShaders->setUniformValue(m_uniConstCol, colOverride);

		if(!obj.m_visible || !obj.m_valid) continue;

		m_pShaders->setUniformValue(m_uniMatrixObj, obj.m_mat);

		// main vertex array object
		pGl->glBindVertexArray(obj.m_vertexarr);


		pGl->glEnableVertexAttribArray(m_attrVertex);
		if(obj.m_type == GlRenderObjType::TRIANGLES)
			pGl->glEnableVertexAttribArray(m_attrVertexNorm);
		pGl->glEnableVertexAttribArray(m_attrVertexCol);
		BOOST_SCOPE_EXIT(pGl, &m_attrVertex, &m_attrVertexNorm, &m_attrVertexCol)
		{
			pGl->glDisableVertexAttribArray(m_attrVertexCol);
			pGl->glDisableVertexAttribArray(m_attrVertexNorm);
			pGl->glDisableVertexAttribArray(m_attrVertex);
		}
		BOOST_SCOPE_EXIT_END
		LOGGLERR(pGl);


		if(obj.m_type == GlRenderObjType::TRIANGLES)
			pGl->glDrawArrays(GL_TRIANGLES, 0, obj.m_triangles.size());
		else if(obj.m_type == GlRenderObjType::LINES)
			pGl->glDrawArrays(GL_LINES, 0, obj.m_vertices.size());
		else
			std::cerr << "Unknown plot object type." << std::endl;

		LOGGLERR(pGl);
	}

	pGl->glDisable(GL_DEPTH_TEST);
}


/**
 * directly draw on a qpainter
 */
void PathsRenderer::DoPaintNonGL(QPainter &painter)
{
	QFont fontOrig = painter.font();
	QPen penOrig = painter.pen();

	QPen penLabel(Qt::black);
	painter.setPen(penLabel);


	// coordinate labels
	painter.drawText(GlToScreenCoords(tl2::create<t_vec_gl>({0.,0.,0.,1.})), "0");
	for(t_real_gl f=-std::floor(m_CoordMax); f<=std::floor(m_CoordMax); f+=0.5)
	{
		if(tl2::equals<t_real_gl>(f, 0))
			continue;

		std::ostringstream ostrF;
		ostrF << f;
		painter.drawText(GlToScreenCoords(tl2::create<t_vec_gl>({f,0.,0.,1.})), ostrF.str().c_str());
		painter.drawText(GlToScreenCoords(tl2::create<t_vec_gl>({0.,f,0.,1.})), ostrF.str().c_str());
		painter.drawText(GlToScreenCoords(tl2::create<t_vec_gl>({0.,0.,f,1.})), ostrF.str().c_str());
	}

	painter.drawText(GlToScreenCoords(tl2::create<t_vec_gl>({m_CoordMax*t_real_gl(1.2), 0., 0., 1.})), "x");
	painter.drawText(GlToScreenCoords(tl2::create<t_vec_gl>({0., m_CoordMax*t_real_gl(1.2), 0., 1.})), "y");
	painter.drawText(GlToScreenCoords(tl2::create<t_vec_gl>({0., 0., m_CoordMax*t_real_gl(1.2), 1.})), "z");


	// render object labels
	for(const auto& obj : m_objs)
	{
		if(!obj.m_visible || !obj.m_valid) continue;

		if(obj.m_label != "")
		{
			t_vec3_gl posLabel3d = obj.m_mat * obj.m_labelPos;
			auto posLabel2d = GlToScreenCoords(tl2::create<t_vec_gl>({posLabel3d[0], posLabel3d[1], posLabel3d[2], 1.}));

			QFont fontLabel = fontOrig;
			QPen penLabel = penOrig;

			fontLabel.setStyleStrategy(QFont::StyleStrategy(/*QFont::OpenGLCompatible |*/ QFont::PreferAntialias | QFont::PreferQuality));
			fontLabel.setWeight(QFont::Medium);
			//penLabel.setColor(QColor(int((1.-obj.m_color[0])*255.), int((1.-obj.m_color[1])*255.), int((1.-obj.m_color[2])*255.), int(obj.m_color[3]*255.)));
			penLabel.setColor(QColor(0,0,0,255));
			painter.setFont(fontLabel);
			painter.setPen(penLabel);
			painter.drawText(posLabel2d, obj.m_label.c_str());

			fontLabel.setWeight(QFont::Normal);
			penLabel.setColor(QColor(int(obj.m_color[0]*255.), int(obj.m_color[1]*255.), int(obj.m_color[2]*255.), int(obj.m_color[3]*255.)));
			painter.setFont(fontLabel);
			painter.setPen(penLabel);
			painter.drawText(posLabel2d, obj.m_label.c_str());
		}
	}

	// restore original styles
	painter.setFont(fontOrig);
	painter.setPen(penOrig);
}


void PathsRenderer::initializeGL()
{
	m_bInitialised = false;

	// --------------------------------------------------------------------
	// shaders
	// --------------------------------------------------------------------
	auto [frag_ok, strFragShader] = tl2::load_file<std::string>("res/frag.shader");
	auto [vertex_ok, strVertexShader] = tl2::load_file<std::string>("res/vertex.shader");

	if(!frag_ok || !vertex_ok)
	{
		std::cerr << "Fragment or vertex shader could not be loaded." << std::endl;
		return;
	}
	// --------------------------------------------------------------------


	// set glsl version and constants
	const std::string strGlsl = std::to_string(_GLSL_MAJ_VER*100 + _GLSL_MIN_VER*10);
	std::string strPi = std::to_string(tl2::pi<t_real_gl>);			// locale-dependent !
	algo::replace_all(strPi, std::string(","), std::string("."));	// ensure decimal point

	for(std::string* strSrc : { &strFragShader, &strVertexShader })
	{
		algo::replace_all(*strSrc, std::string("${GLSL_VERSION}"), strGlsl);
		algo::replace_all(*strSrc, std::string("${PI}"), strPi);
	}


	// GL functions
	auto *pGl = get_gl_functions(this);
	if(!pGl) return;

	m_strGlVer = (char*)pGl->glGetString(GL_VERSION);
	m_strGlShaderVer = (char*)pGl->glGetString(GL_SHADING_LANGUAGE_VERSION);
	m_strGlVendor = (char*)pGl->glGetString(GL_VENDOR);
	m_strGlRenderer = (char*)pGl->glGetString(GL_RENDERER);
	LOGGLERR(pGl);


	// shaders
	{
		static QMutex shadermutex;
		shadermutex.lock();
		BOOST_SCOPE_EXIT(&shadermutex) { shadermutex.unlock(); } BOOST_SCOPE_EXIT_END

		// shader compiler/linker error handler
		auto shader_err = [this](const char* err) -> void
		{
			std::cerr << err << std::endl;

			std::string strLog = m_pShaders->log().toStdString();
			if(strLog.size())
				std::cerr << "Shader log: " << strLog << std::endl;

			std::exit(-1);
		};

		// compile & link shaders
		m_pShaders = std::make_shared<QOpenGLShaderProgram>(this);

		if(!m_pShaders->addShaderFromSourceCode(QOpenGLShader::Fragment, strFragShader.c_str()))
			shader_err("Cannot compile fragment shader.");
		if(!m_pShaders->addShaderFromSourceCode(QOpenGLShader::Vertex, strVertexShader.c_str()))
			shader_err("Cannot compile vertex shader.");

		if(!m_pShaders->link())
			shader_err("Cannot link shaders.");

		m_uniMatrixCam = m_pShaders->uniformLocation("cam");
		m_uniMatrixCamInv = m_pShaders->uniformLocation("cam_inv");
		m_uniMatrixProj = m_pShaders->uniformLocation("proj");
		m_uniMatrixObj = m_pShaders->uniformLocation("obj");
		m_uniConstCol = m_pShaders->uniformLocation("constcol");
		m_uniLightPos = m_pShaders->uniformLocation("lightpos");
		m_uniNumActiveLights = m_pShaders->uniformLocation("activelights");
		m_attrVertex = m_pShaders->attributeLocation("vertex");
		m_attrVertexNorm = m_pShaders->attributeLocation("normal");
		m_attrVertexCol = m_pShaders->attributeLocation("vertexcol");
	}
	LOGGLERR(pGl);


	// 3d objects
	AddCoordinateCross(-m_CoordMax, m_CoordMax);


	// options
	pGl->glCullFace(GL_BACK);
	pGl->glEnable(GL_CULL_FACE);

	pGl->glEnable(GL_BLEND);
	pGl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	pGl->glEnable(GL_MULTISAMPLE);
	pGl->glEnable(GL_LINE_SMOOTH);
	pGl->glEnable(GL_POLYGON_SMOOTH);
	pGl->glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	pGl->glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

	m_bInitialised = true;


	if(IsInitialised())
		emit AfterGLInitialisation();
	else
		emit GLInitialisationFailed();
}


void PathsRenderer::resizeGL(int w, int h)
{
	m_iScreenDims[0] = w;
	m_iScreenDims[1] = h;
	m_bWantsResize = true;

	if(!m_bPlatformSupported || !m_bInitialised) return;

	if(auto *pContext = ((QOpenGLWidget*)this)->context(); !pContext)
		return;
	auto *pGl = get_gl_functions(this);
	if(!pGl)
		return;

	m_matViewport = tl2::hom_viewport<t_mat_gl>(w, h, 0., 1.);
	std::tie(m_matViewport_inv, std::ignore) = tl2::inv<t_mat_gl>(m_matViewport);

	m_matPerspective = tl2::hom_perspective<t_mat_gl>(0.01, 100., tl2::pi<t_real_gl>*0.5, t_real_gl(h)/t_real_gl(w));
	std::tie(m_matPerspective_inv, std::ignore) = tl2::inv<t_mat_gl>(m_matPerspective);

	pGl->glViewport(0, 0, w, h);
	pGl->glDepthRange(0, 1);

	// bind shaders
	m_pShaders->bind();
	BOOST_SCOPE_EXIT(m_pShaders) { m_pShaders->release(); } BOOST_SCOPE_EXIT_END
	LOGGLERR(pGl);

	// set matrices
	m_pShaders->setUniformValue(m_uniMatrixCam, m_matCam);
	m_pShaders->setUniformValue(m_uniMatrixCamInv, m_matCam_inv);
	m_pShaders->setUniformValue(m_uniMatrixProj, m_matPerspective);
	LOGGLERR(pGl);

	m_bWantsResize = false;
}


void PathsRenderer::paintGL()
{
	if(!m_bPlatformSupported || !m_bInitialised) return;
	QMutexLocker _locker{&m_mutexObj};

	if(auto *pContext = context(); !pContext) return;
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// gl painting
	{
		BOOST_SCOPE_EXIT(&painter) { painter.endNativePainting(); } BOOST_SCOPE_EXIT_END

		if(m_bPickerNeedsUpdate) UpdatePicker();

		auto *pGl = get_gl_functions(this);
		painter.beginNativePainting();
		DoPaintGL(pGl);
	}

	// qt painting
	DoPaintNonGL(painter);
}


void PathsRenderer::mouseMoveEvent(QMouseEvent *pEvt)
{
	m_posMouse = pEvt->localPos();

	if(m_bInRotation)
	{
		auto diff = (m_posMouse - m_posMouseRotationStart);
		t_real_gl phi = diff.x() + m_phi_saved;
		t_real_gl theta = diff.y() + m_theta_saved;

		m_matCamRot = tl2::rotation<t_mat_gl, t_vec_gl>(m_vecCamX, theta/180.*tl2::pi<t_real_gl>, 0);
		m_matCamRot *= tl2::rotation<t_mat_gl, t_vec_gl>(m_vecCamY, phi/180.*tl2::pi<t_real_gl>, 0);

		UpdateCam();
	}
	else
	{
		// also automatically done in UpdateCam
		m_bPickerNeedsUpdate = true;
		update();
	}

	m_mouseMovedBetweenDownAndUp = 1;
	pEvt->accept();
}


void PathsRenderer::mousePressEvent(QMouseEvent *pEvt)
{
	m_mouseMovedBetweenDownAndUp = 0;

	if(pEvt->buttons() & Qt::LeftButton) m_mouseDown[0] = 1;
	if(pEvt->buttons() & Qt::MiddleButton) m_mouseDown[1] = 1;
	if(pEvt->buttons() & Qt::RightButton) m_mouseDown[2] = 1;

	if(m_mouseDown[1])
	{
		// reset zoom
		m_zoom = 1;
		UpdateCam();
	}
	if(m_mouseDown[2])
	{
		// begin rotation
		if(!m_bInRotation)
		{
			m_posMouseRotationStart = m_posMouse;
			m_bInRotation = true;
		}
	}

	pEvt->accept();
	emit MouseDown(m_mouseDown[0], m_mouseDown[1], m_mouseDown[2]);
}


void PathsRenderer::mouseReleaseEvent(QMouseEvent *pEvt)
{
	bool mouseDownOld[] = { m_mouseDown[0], m_mouseDown[1], m_mouseDown[2] };

	if((pEvt->buttons() & Qt::LeftButton) == 0) m_mouseDown[0] = 0;
	if((pEvt->buttons() & Qt::MiddleButton) == 0) m_mouseDown[1] = 0;
	if((pEvt->buttons() & Qt::RightButton) == 0) m_mouseDown[2] = 0;

	if(!m_mouseDown[2])
	{
		// end rotation
		if(m_bInRotation)
		{
			auto diff = (m_posMouse - m_posMouseRotationStart);
			m_phi_saved += diff.x();
			m_theta_saved += diff.y();

			m_bInRotation = false;
		}
	}

	pEvt->accept();
	emit MouseUp(!m_mouseDown[0], !m_mouseDown[1], !m_mouseDown[2]);

	// only emit click if moving the mouse (i.e. rotationg the scene) was not the primary intent
	if(!m_mouseMovedBetweenDownAndUp)
	{
		bool mouseClicked[] = { !m_mouseDown[0] && mouseDownOld[0],
			!m_mouseDown[1] && mouseDownOld[1],
			!m_mouseDown[2] && mouseDownOld[2] };
		if(mouseClicked[0] || mouseClicked[1] || mouseClicked[2])
			emit MouseClick(mouseClicked[0], mouseClicked[1], mouseClicked[2]);
	}
}


void PathsRenderer::wheelEvent(QWheelEvent *pEvt)
{
	const t_real_gl degrees = pEvt->angleDelta().y() / 8.;

	// zoom
	m_zoom *= std::pow(2., degrees/64.);
	UpdateCam();

	pEvt->accept();
}


void PathsRenderer::paintEvent(QPaintEvent* pEvt)
{
	QOpenGLWidget::paintEvent(pEvt);
}

// ----------------------------------------------------------------------------
