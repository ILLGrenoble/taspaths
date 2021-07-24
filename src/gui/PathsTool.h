/**
 * TAS path tool
 * @author Tobias Weber <tweber@ill.fr>
 * @date feb-2021
 * @license GPLv3, see 'LICENSE' file
 */

#ifndef __PATHS_TOOL_H__
#define __PATHS_TOOL_H__

#include <QtCore/QSettings>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QLabel>

#include <string>
#include <memory>

#include "src/core/InstrumentSpace.h"
#include "src/core/PathsBuilder.h"

#include "tlibs2/libs/maths.h"

#include "PathsRenderer.h"
#include "ConfigSpace.h"
#include "GeoBrowser.h"
#include "About.h"
#include "Settings.h"

#include "dock/TASProperties.h"
#include "dock/XtalProperties.h"
#include "dock/CoordProperties.h"
#include "dock/PathProperties.h"
#include "dock/CamProperties.h"


// ----------------------------------------------------------------------------
class PathsTool : public QMainWindow
{ /*Q_OBJECT*/
private:
	QSettings m_sett{"takin", "taspaths"};

	// renderer
	std::shared_ptr<PathsRenderer> m_renderer
		{ std::make_shared<PathsRenderer>(this) };
	int m_multisamples{ 8 };

	// gl info strings
	std::string m_gl_ver, m_gl_shader_ver, m_gl_vendor, m_gl_renderer;

	QStatusBar *m_statusbar{ nullptr };
	QProgressBar *m_progress{ nullptr };
	QLabel *m_labelStatus{ nullptr };
	QLabel *m_labelCollisionStatus{ nullptr };

	QMenu *m_menuOpenRecent{ nullptr };
	QMenuBar *m_menubar{ nullptr };

	// dialogs and docks
	std::shared_ptr<AboutDlg> m_dlgAbout;
	std::shared_ptr<SettingsDlg> m_dlgSettings;
	std::shared_ptr<GeometriesBrowser> m_dlgGeoBrowser;
	std::shared_ptr<ConfigSpaceDlg> m_dlgConfigSpace;
	std::shared_ptr<TASPropertiesDockWidget> m_tasProperties;
	std::shared_ptr<XtalPropertiesDockWidget> m_xtalProperties;
	std::shared_ptr<XtalInfoDockWidget> m_xtalInfos;
	std::shared_ptr<CoordPropertiesDockWidget> m_coordProperties;
	std::shared_ptr<PathPropertiesDockWidget> m_pathProperties;
	std::shared_ptr<CamPropertiesDockWidget> m_camProperties;

	std::string m_initialInstrFile = "instrument.taspaths";

	// recent file list and currently active file
	QStringList m_recentFiles;
	QString m_curFile;

	// instrument configuration and paths builder
	InstrumentSpace m_instrspace;
	PathsBuilder m_pathsbuilder;

	// mouse picker
	t_real m_mouseX, m_mouseY;
	std::string m_curObj;

	// crystal matrices
	t_mat m_B = tl2::B_matrix<t_mat>(
		5., 5., 5.,
		tl2::pi<t_real>*0.5, tl2::pi<t_real>*0.5, tl2::pi<t_real>*0.5);
	t_mat m_UB = tl2::unit<t_mat>(3);

	// scattering plane
	t_vec m_plane_rlu[3] = {
		tl2::create<t_vec>({ 1, 0, 0 }),
		tl2::create<t_vec>({ 0, 1, 0 }),
		tl2::create<t_vec>({ 0, 0, 1 }),
	};

	// mono and ana d-spacings
	t_real m_dspacings[2] = { 3.355, 3.355 };

	// scattering senses
	t_real m_sensesCCW[3] = { 1., -1., 1. };


protected:
	void UpdateUB();


protected:
	// events
	virtual void showEvent(QShowEvent *) override;
	virtual void hideEvent(QHideEvent *) override;
	virtual void closeEvent(QCloseEvent *) override;

	// File -> New
	void NewFile();

	// File -> Open
	void OpenFile();

	// File -> Save
	void SaveFile();

	// File -> Save As
	void SaveFileAs();

	// load file
	bool OpenFile(const QString &file);

	// save file
	bool SaveFile(const QString &file);

	// adds a file to the recent files menu
	void AddRecentFile(const QString &file);

	// remember current file and set window title
	void SetCurrentFile(const QString &file);

	// sets the recent file menu
	void SetRecentFiles(const QStringList &files);

	// creates the "recent files" sub-menu
	void RebuildRecentFiles();


protected slots:
	// go to crystal coordinates (or set target angles)
	void GotoCoordinates(t_real h, t_real k, t_real l,
		t_real ki, t_real kf,
		bool only_set_target);

	// go to instrument angles
	void GotoAngles(std::optional<t_real> a1,
		std::optional<t_real> a3, std::optional<t_real> a4,
		std::optional<t_real> a5, bool only_set_target);

	// calculation of the meshes and paths
	void CalculatePathMesh();
	void CalculatePath();

	// called after the plotter has initialised
	void AfterGLInitialisation();

	// mouse coordinates on base plane
	void CursorCoordsChanged(t_real_gl x, t_real_gl y);

	// mouse is over an object
	void PickerIntersection(const t_vec3_gl* pos, std::string obj_name, const t_vec3_gl* posSphere);

	// clicked on an object
	void ObjectClicked(const std::string& obj, bool left, bool middle, bool right);

	// dragging an object
	void ObjectDragged(bool drag_start, const std::string& obj,
		t_real_gl x_start, t_real_gl y_start, t_real_gl x, t_real_gl y);

	// set temporary status message
	void SetTmpStatus(const std::string& msg);

	// update permanent status message
	void UpdateStatusLabel();

	// set instrument status (coordinates, collision flag)
	void SetInstrumentStatus(const std::optional<t_vec>& Q, t_real E,
		bool in_angluar_limits, bool colliding);

public:
	/**
	 * create UI
	 */
	PathsTool(QWidget* pParent=nullptr);
	~PathsTool() = default;

	void SetInitialInstrumentFile(const std::string& file)
	{ m_initialInstrFile = file;  }
};
// ----------------------------------------------------------------------------


#endif
