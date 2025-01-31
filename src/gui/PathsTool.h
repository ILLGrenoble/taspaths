/**
 * TAS-Paths -- main window
 * @author Tobias Weber <tweber@ill.fr>
 * @date February-November 2021
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

#ifndef __PATHS_TOOL_H__
#define __PATHS_TOOL_H__

#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>

#include <string>
#include <memory>
#include <future>
#include <functional>

#include "tlibs2/libs/maths.h"
#include "tlibs2/libs/qt/recent.h"

#include "src/core/types.h"
#include "src/core/PathsBuilder.h"
#include "src/core/InstrumentSpace.h"
#include "src/core/TasCalculator.h"

#include "InstrumentStatus.h"
#include "common/Resources.h"
#include "settings_variables.h"
#include "PathsRenderer.h"

#include "dialogs/ConfigSpace.h"
#include "dialogs/XtalConfigSpace.h"
#include "dialogs/GeoBrowser.h"
#include "dialogs/TextureBrowser.h"
#include "dialogs/About.h"
#include "dialogs/Licenses.h"
#include "dialogs/Settings.h"

#include "dock/TASProperties.h"
#include "dock/XtalProperties.h"
#include "dock/CoordProperties.h"
#include "dock/PathProperties.h"
#include "dock/CamProperties.h"

#include "src/tools/lines.h"
#include "src/tools/hull.h"
#include "src/tools/poly.h"


class PathsTool : public QMainWindow
{ Q_OBJECT
public:
	/**
	 * create UI
	 */
	PathsTool(QWidget* pParent = nullptr);
	virtual ~PathsTool() = default;

	PathsTool(const PathsTool&) = delete;
	const PathsTool& operator=(const PathsTool&) = delete;

	void SetInitialInstrumentFile(const std::string& file);

	// load file
	bool OpenFile(const QString &file);

	// save file
	bool SaveFile(const QString &file);


private:
	QSettings m_sett{};
	QByteArray m_initial_state{};

	// renderer
	std::shared_ptr<PathsRenderer> m_renderer
		{ std::make_shared<PathsRenderer>(this) };
	int m_multisamples{ 8 };

	// gl info strings
	std::string m_gl_ver{}, m_gl_shader_ver{},
		m_gl_vendor{}, m_gl_renderer{};

	QStatusBar *m_statusbar{ nullptr };
	QProgressBar *m_progress{ nullptr };
	QToolButton *m_buttonStop{ nullptr };
	QLabel *m_labelStatus{ nullptr };
	QLabel *m_labelCollisionStatus{ nullptr };

	bool m_stop_requested{ false };
	std::future<bool> m_futCalc{};

	QMenu *m_menuOpenRecent{ nullptr };
	QMenuBar *m_menubar{ nullptr };

	// context menu for 3d objects
	QMenu *m_contextMenuObj{ nullptr };
	std::string m_curContextObj{};

	// dialogs
	// cannot directly use the type here because this causes a -Wsubobject-linkage warning
	//using t_SettingsDlg = SettingsDlg<g_settingsvariables.size(), &g_settingsvariables>;
	std::shared_ptr<AboutDlg> m_dlgAbout{};
	std::shared_ptr<LicensesDlg> m_dlgLicenses{};
	std::shared_ptr<QDialog> m_dlgSettings{};
	std::shared_ptr<GeometriesBrowser> m_dlgGeoBrowser{};
	std::shared_ptr<TextureBrowser> m_dlgTextureBrowser{};
	std::shared_ptr<ConfigSpaceDlg> m_dlgConfigSpace{};
	std::shared_ptr<XtalConfigSpaceDlg> m_dlgXtalConfigSpace{};

	// tool programs
	std::shared_ptr<LinesWnd> m_wndLines{};
	std::shared_ptr<HullWnd> m_wndHull{};
	std::shared_ptr<PolyWnd> m_wndPoly{};

	// docks
	std::shared_ptr<TASPropertiesDockWidget> m_tasProperties{};
	std::shared_ptr<XtalPropertiesDockWidget> m_xtalProperties{};
	std::shared_ptr<XtalInfoDockWidget> m_xtalInfos{};
	std::shared_ptr<CoordPropertiesDockWidget> m_coordProperties{};
	std::shared_ptr<PathPropertiesDockWidget> m_pathProperties{};
	std::shared_ptr<CamPropertiesDockWidget> m_camProperties{};

	std::string m_initialInstrFile = "instrument.taspaths";
	bool m_initialInstrFileModified = false;

	// recently opened files
	tl2::RecentFiles m_recent{};

	// function to call for the recent file menu items
	std::function<bool(const QString& filename)> m_open_func
		= [this](const QString& filename) -> bool
	{
		return this->OpenFile(filename);
	};

	// instrument configuration and paths builder
	InstrumentSpace m_instrspace{};
	PathsBuilder m_pathsbuilder{};
	bool m_autocalcpath{true};

	// calculated path vertices
	std::vector<t_vec2> m_pathvertices{};

	t_real m_targetMonoScatteringAngle = 0;
	t_real m_targetSampleScatteringAngle = 0;

	// mouse picker
	t_real m_mouseX{}, m_mouseY{};
	std::string m_curObj{};

	// tas calculations
	TasCalculator m_tascalc{};
	InstrumentStatus m_instrstatus{};

	// progress of active calculation
	t_real m_calculationprogress{};


protected:
	// events
	virtual void showEvent(QShowEvent *) override;
	virtual void hideEvent(QHideEvent *) override;
	virtual void closeEvent(QCloseEvent *) override;
	virtual void dragEnterEvent(QDragEnterEvent *) override;
	virtual void dropEvent(QDropEvent *) override;

	// File -> Export Path
	bool ExportPath(PathsExporterFormat fmt);

	// save a screenshot of the instrument 3d view
	bool SaveScreenshot(const QString& file);

	// save a combined screenshot of the instrument view and config space
	bool SaveCombinedScreenshot(const QString& file);

	// remember current file and set window title
	void SetCurrentFile(const QString &file);

	void UpdateUB();

	// (in)validates the path mesh if the obstacle configuration has changed
	void ValidatePathMesh(bool valid = true);
	// (in)validates the path
	void ValidatePath(bool valid = true);
	// increases the amount of frames
	void InterpolatePath(std::vector<t_vec2>& vertices);


protected slots:
	// File -> New
	void NewFile();
	bool LoadInitialInstrumentFile();

	// File -> Open
	void OpenFile();

	// File -> Save
	void SaveFile();

	// File -> Save As
	void SaveFileAs();

	// File -> Save Screenshot
	void SaveScreenshot();

	// go to crystal coordinates (or set target angles)
	void GotoCoordinates(t_real h, t_real k, t_real l,
		t_real ki, t_real kf,
		bool only_set_target);
	void GotoCoordinates(t_real h, t_real k, t_real l,
		t_real ki, t_real kf)
	{
		GotoCoordinates(h, k, l, ki, kf, false);
	}

	// set either kf=const or ki=const
	void SetKfConstMode(bool kf_const = true);

	// go to instrument angles
	void GotoAngles(std::optional<t_real> a1,
		std::optional<t_real> a3, std::optional<t_real> a4,
		std::optional<t_real> a5, bool only_set_target);

	// path available, e.g. from configuration space dialog
	void ExternalPathAvailable(const InstrumentPath& path);

	// calculation of the meshes and paths
	bool CalculatePathMesh();
	bool CalculatePath();

	// called after the plotter has initialised
	void AfterGLInitialisation();

	// mouse coordinates on base plane
	void CursorCoordsChanged(t_real_gl x, t_real_gl y);

	// mouse is over an object
	void PickerIntersection(const t_vec3_gl* pos, std::string obj_name);

	// clicked on an object
	void ObjectClicked(const std::string& obj, bool left, bool middle, bool right);

	// dragging an object
	void ObjectDragged(bool drag_start, const std::string& obj,
		t_real_gl x_start, t_real_gl y_start, t_real_gl x, t_real_gl y);

	// set temporary status message, by default for 2 seconds
	void SetTmpStatus(const std::string& msg, int msg_duration=2000);

	// update permanent status message
	void UpdateStatusLabel();

	// update instrument status display (e.g, coordinates, collision flag)
	void UpdateInstrumentStatus();

	// move the instrument to a position on the path
	void TrackPath(std::size_t idx);

	// add or delete 3d objects
	void AddWall();
	void AddPillar();

	void DeleteCurrentObject();
	void RotateCurrentObject(t_real angle);
	void ShowCurrentObjectProperties();

	void ShowGeometryBrowser();
	void ShowTextureBrowser();

	void CollectGarbage();

	void RotateObject(const std::string& id, t_real angle);
	void DeleteObject(const std::string& id);
	void RenameObject(const std::string& oldid, const std::string& newid);
	void ChangeObjectProperty(const std::string& id, const ObjectProperty& prop);

	// propagate (changed) global settings to each object
	void InitSettings();


signals:
	// signal when a path mesh is being calculated
	void PathMeshCalculation(CalculationState state, t_real progress);
	// signal when a new path has been calculated
	void PathAvailable(std::size_t numVertices);
	// signal if a path mesh is valid or invalid
	void PathMeshValid(bool valid);
};


#endif
