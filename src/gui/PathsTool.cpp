/**
 * TAS path tool, main window
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

#include "PathsTool.h"

#include <QtCore/QMetaObject>
#include <QtCore/QThread>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtGui/QDesktopServices>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
namespace pt = boost::property_tree;

#include "settings_variables.h"
#include "src/libs/proc.h"
#include "tlibs2/libs/maths.h"
#include "tlibs2/libs/str.h"
#include "tlibs2/libs/file.h"
#include "tlibs2/libs/algos.h"
#include "tlibs2/libs/helper.h"

#define MAX_RECENT_FILES 16
#define PROG_TITLE "Triple-Axis Path Calculator"


#if defined(__MINGW32__) || defined(__MINGW64__)
	#define EXEC_EXTENSION ".exe"
#else
	#define EXEC_EXTENSION ""
#endif


/**
 * event signalling that the crystal UB matrix needs an update
 */
void PathsTool::UpdateUB()
{
	m_tascalc.UpdateUB();

	if(m_xtalInfos)
		m_xtalInfos->GetWidget()->SetUB(m_tascalc.GetB(), m_tascalc.GetUB());
}


/**
 * the window is being shown
 */
void PathsTool::showEvent(QShowEvent *evt)
{
	m_renderer->EnableTimer(true);
	QMainWindow::showEvent(evt);
}


/**
 * the window is being hidden
 */
void PathsTool::hideEvent(QHideEvent *evt)
{
	m_renderer->EnableTimer(false);
	QMainWindow::hideEvent(evt);
}


/**
 * the window is being closed
 */
void PathsTool::closeEvent(QCloseEvent *evt)
{
	// save window size, position, and state
	m_sett.setValue("geo", saveGeometry());
	m_sett.setValue("state", saveState());

	m_recent.TrimEntries();
	m_sett.setValue("recent_files", m_recent.GetRecentFiles());

	QMainWindow::closeEvent(evt);
}


/**
 * File -> New
 */
void PathsTool::NewFile()
{
	SetCurrentFile("");
	m_instrspace.Clear();
	ValidatePathMesh(false);

	if(m_dlgGeoBrowser)
		m_dlgGeoBrowser->UpdateGeoTree(m_instrspace);
	if(m_renderer)
		m_renderer->LoadInstrument(m_instrspace);
}


/**
 * File -> Open
 */
void PathsTool::OpenFile()
{
	QString dirLast = m_sett.value("cur_dir", "~/").toString();

	QFileDialog filedlg(this, "Open File", dirLast,
		"TAS-Paths Files (*.taspaths)");
	filedlg.setAcceptMode(QFileDialog::AcceptOpen);
	filedlg.setDefaultSuffix("taspaths");
	filedlg.setFileMode(QFileDialog::AnyFile);

	if(!filedlg.exec())
		return;

	QStringList files = filedlg.selectedFiles();
	if(!files.size() || files[0]=="" || !QFile::exists(files[0]))
		return;

	if(OpenFile(files[0]))
		m_sett.setValue("cur_dir", QFileInfo(files[0]).path());
}


/**
 * File -> Save
 */
void PathsTool::SaveFile()
{
	if(m_recent.GetCurFile() == "")
		SaveFileAs();
	else
		SaveFile(m_recent.GetCurFile());
}


/**
 * File -> Save As
 */
void PathsTool::SaveFileAs()
{
	QString dirLast = m_sett.value("cur_dir", "~/").toString();

	QFileDialog filedlg(this, "Open File", dirLast,
		"TAS-Paths Files (*.taspaths)");
	filedlg.setAcceptMode(QFileDialog::AcceptSave);
	filedlg.setDefaultSuffix("taspaths");
	filedlg.setFileMode(QFileDialog::AnyFile);

	if(!filedlg.exec())
		return;

	QStringList files = filedlg.selectedFiles();
	if(!files.size() || files[0]=="" || !QFile::exists(files[0]))
		return;

	if(SaveFile(files[0]))
		m_sett.setValue("cur_dir", QFileInfo(files[0]).path());
}


/**
 * File -> Save Screenshot
 */
void PathsTool::SaveScreenshot()
{
	QString dirLast = m_sett.value("cur_dir", "~/").toString();

	QFileDialog filedlg(this, "Save Screenshot", dirLast,
		"PNG Images (*.png);;JPEG Images (*.jpg)");
	filedlg.setAcceptMode(QFileDialog::AcceptSave);
	filedlg.setDefaultSuffix("png");
	filedlg.setFileMode(QFileDialog::AnyFile);

	if(!filedlg.exec())
		return;

	QStringList files = filedlg.selectedFiles();
	if(!files.size() || files[0]=="" || !QFile::exists(files[0]))
		return;

	bool ok = false;
	if(g_combined_screenshots)
		ok = SaveCombinedScreenshot(files[0]);
	else
		ok = SaveScreenshot(files[0]);

	if(ok)
		m_sett.setValue("cur_dir", QFileInfo(files[0]).path());
}


/**
 * File -> Export Path
 */
bool PathsTool::ExportPath(PathsExporterFormat fmt)
{
	std::shared_ptr<PathsExporterBase> exporter;

	QString dirLast = m_sett.value("cur_dir", "~/").toString();

	QFileDialog filedlg(this, "Export Path", dirLast,
		"Text Files (*.txt)");
	filedlg.setAcceptMode(QFileDialog::AcceptSave);
	filedlg.setDefaultSuffix("txt");
	filedlg.setFileMode(QFileDialog::AnyFile);

	if(!filedlg.exec())
		return false;

	QStringList files = filedlg.selectedFiles();
	if(!files.size() || files[0]=="" || !QFile::exists(files[0]))
		return false;

	switch(fmt)
	{
		case PathsExporterFormat::RAW:
			exporter = std::make_shared<PathsExporterRaw>(files[0].toStdString());
			break;
		case PathsExporterFormat::NOMAD:
			exporter = std::make_shared<PathsExporterNomad>(files[0].toStdString());
			break;
		case PathsExporterFormat::NICOS:
			exporter = std::make_shared<PathsExporterNicos>(files[0].toStdString());
			break;
	}

	if(!exporter)
	{
		QMessageBox::critical(this, "Error", "No path is available.");
		return false;
	}

	if(!m_pathsbuilder.AcceptExporter(exporter.get(), m_pathvertices, true))
	{
		QMessageBox::critical(this, "Error", "path could not be exported.");
		return false;
	}

	m_sett.setValue("cur_dir", QFileInfo(files[0]).path());
	return true;
}


/**
 * load file
 */
bool PathsTool::OpenFile(const QString &file)
{
	try
	{
		NewFile();

		// get property tree
		if(file == "" || !QFile::exists(file))
		{
			QMessageBox::critical(this, "Error",
				"Instrument file \"" + file + "\" does not exist.");
			return false;
		}

		// open xml
		std::string filename = file.toStdString();
		std::ifstream ifstr{filename};
		if(!ifstr)
		{
			QMessageBox::critical(this, "Error",
				("Could not read instrument file \"" + filename + "\".").c_str());
			return false;
		}

		// read xml
		pt::ptree prop;
		pt::read_xml(ifstr, prop);
		// check format and version
		if(auto opt = prop.get_optional<std::string>(FILE_BASENAME "ident");
			!opt || *opt != PROG_IDENT)
		{
			QMessageBox::critical(this, "Error",
				("Instrument file \"" + filename +
				"\" has invalid identifier.").c_str());
			return false;
		}


		// load instrument definition file
		if(auto [instrok, msg] =
			InstrumentSpace::load(prop, m_instrspace, &filename); !instrok)
		{
			QMessageBox::critical(this, "Error", msg.c_str());
			return false;
		}
		else
		{
			std::ostringstream ostr;
			ostr << "Loaded \"" << QFileInfo{file}.fileName().toStdString() << "\" "
				<< "dated " << msg << ".";

			SetTmpStatus(ostr.str());
		}


		// load dock window settings
		if(auto prop_dock = prop.get_child_optional(FILE_BASENAME "configuration.tas"); prop_dock)
			m_tasProperties->GetWidget()->Load(*prop_dock);
		if(auto prop_dock = prop.get_child_optional(FILE_BASENAME "configuration.crystal"); prop_dock)
			m_xtalProperties->GetWidget()->Load(*prop_dock);
		if(auto prop_dock = prop.get_child_optional(FILE_BASENAME "configuration.coordinates"); prop_dock)
			m_coordProperties->GetWidget()->Load(*prop_dock);
		if(auto prop_dock = prop.get_child_optional(FILE_BASENAME "configuration.path"); prop_dock)
			m_pathProperties->GetWidget()->Load(*prop_dock);
		if(auto prop_dock = prop.get_child_optional(FILE_BASENAME "configuration.camera"); prop_dock)
			m_camProperties->GetWidget()->Load(*prop_dock);


		SetCurrentFile(file);
		m_recent.AddRecentFile(file);

		if(m_dlgGeoBrowser)
			m_dlgGeoBrowser->UpdateGeoTree(m_instrspace);
		if(m_renderer)
			m_renderer->LoadInstrument(m_instrspace);


		// is ki or kf fixed?
		bool kf_fixed = true;
		if(!std::get<1>(m_tascalc.GetKfix()))
			kf_fixed = false;


		// update slot for instrument space (e.g. walls) changes
		m_instrspace.AddUpdateSlot(
			[this](const InstrumentSpace& instrspace)
			{
				// invalidate the mesh
				ValidatePathMesh(false);

				if(m_renderer)
					m_renderer->UpdateInstrumentSpace(instrspace);
			});

		// update slot for instrument movements
		m_instrspace.GetInstrument().AddUpdateSlot(
			[this, kf_fixed](const Instrument& instr)
			{
				// old angles
				t_real oldA6 = m_tasProperties->GetWidget()->GetAnaScatteringAngle()/t_real{180}*tl2::pi<t_real>;
				t_real oldA2 = m_tasProperties->GetWidget()->GetMonoScatteringAngle()/t_real{180}*tl2::pi<t_real>;

				// get scattering angles
				t_real monoScAngle = m_instrspace.GetInstrument().GetMonochromator().GetAxisAngleOut();
				t_real sampleScAngle = m_instrspace.GetInstrument().GetSample().GetAxisAngleOut();
				t_real anaScAngle = m_instrspace.GetInstrument().GetAnalyser().GetAxisAngleOut();

				// set scattering angles
				m_tasProperties->GetWidget()->SetMonoScatteringAngle(monoScAngle*t_real{180}/tl2::pi<t_real>);
				m_tasProperties->GetWidget()->SetSampleScatteringAngle(sampleScAngle*t_real{180}/tl2::pi<t_real>);
				m_tasProperties->GetWidget()->SetAnaScatteringAngle(anaScAngle*t_real{180}/tl2::pi<t_real>);

				// get crystal rocking angles
				t_real monoXtalAngle = m_instrspace.GetInstrument().GetMonochromator().GetAxisAngleInternal();
				t_real sampleXtalAngle = m_instrspace.GetInstrument().GetSample().GetAxisAngleInternal();
				t_real anaXtalAngle = m_instrspace.GetInstrument().GetAnalyser().GetAxisAngleInternal();

				// set crystal rocking angles
				m_tasProperties->GetWidget()->SetMonoCrystalAngle(monoXtalAngle*t_real{180}/tl2::pi<t_real>);
				m_tasProperties->GetWidget()->SetSampleCrystalAngle(sampleXtalAngle*t_real{180}/tl2::pi<t_real>);
				m_tasProperties->GetWidget()->SetAnaCrystalAngle(anaXtalAngle*t_real{180}/tl2::pi<t_real>);

				auto [Qrlu, E] = m_tascalc.GetQE(monoXtalAngle, anaXtalAngle,
					sampleXtalAngle, sampleScAngle);

				bool in_angular_limits = m_instrspace.CheckAngularLimits();
				bool colliding = m_instrspace.CheckCollision2D();

				SetInstrumentStatus(Qrlu, E,
					in_angular_limits, colliding);

				// if the analyser or monochromator angle changes, the mesh also needs to be updated
				if(kf_fixed && !tl2::equals<t_real>(oldA6, anaScAngle, g_eps))
					ValidatePathMesh(false);
				if(!kf_fixed && !tl2::equals<t_real>(oldA2, monoScAngle, g_eps))
					ValidatePathMesh(false);

				if(this->m_dlgConfigSpace)
					this->m_dlgConfigSpace->UpdateInstrument(
						instr, m_tascalc.GetScatteringSenses());

				if(this->m_renderer)
				{
					this->m_renderer->SetInstrumentStatus(
						in_angular_limits, colliding);
					this->m_renderer->UpdateInstrument(instr);
				}
			});

		m_instrspace.GetInstrument().EmitUpdate();
	}
	catch(const std::exception& ex)
	{
		QMessageBox::critical(this, "Error",
			QString{"Instrument configuration error: "} + ex.what() + QString{"."});
		return false;
	}
	return true;
}


/**
 * save file
 */
bool PathsTool::SaveFile(const QString &file)
{
	if(file=="")
		return false;

	// save instrument space configuration
	pt::ptree prop = m_instrspace.Save();

	// save dock window settings
	prop.put_child(FILE_BASENAME "configuration.tas", m_tasProperties->GetWidget()->Save());
	prop.put_child(FILE_BASENAME "configuration.crystal", m_xtalProperties->GetWidget()->Save());
	prop.put_child(FILE_BASENAME "configuration.coordinates", m_coordProperties->GetWidget()->Save());
	prop.put_child(FILE_BASENAME "configuration.path", m_pathProperties->GetWidget()->Save());
	prop.put_child(FILE_BASENAME "configuration.camera", m_camProperties->GetWidget()->Save());

	// set format and version
	prop.put(FILE_BASENAME "ident", PROG_IDENT);
	prop.put(FILE_BASENAME "timestamp", tl2::var_to_str(tl2::epoch<t_real>()));

	std::ofstream ofstr{file.toStdString()};
	if(!ofstr)
	{
		QMessageBox::critical(this, "Error", "Could not save file.");
		return false;
	}

	ofstr.precision(g_prec);
	pt::write_xml(ofstr, prop, pt::xml_writer_make_settings('\t', 1, std::string{"utf-8"}));

	SetCurrentFile(file);
	m_recent.AddRecentFile(file);
	return true;
}


/**
 * save screenshot
 */
bool PathsTool::SaveScreenshot(const QString &file)
{
	if(file=="" || !m_renderer)
		return false;

	QImage img = m_renderer->grabFramebuffer();
	return img.save(file, nullptr, 90);
}


/**
 * save a combined screenshot of the instrument view and config space
 */
bool PathsTool::SaveCombinedScreenshot(const QString& filename)
{
	bool ok1 = SaveScreenshot(filename);

	bool ok2 = false;
	if(m_dlgConfigSpace)
	{
		fs::path file_pdf{filename.toStdString()};
		file_pdf.replace_extension(fs::path{".pdf"});

		ok2 = m_dlgConfigSpace->SaveFigure(file_pdf.string().c_str());
	}

	return ok1 && ok2;
}


/**
 * remember current file and set window title
 */
void PathsTool::SetCurrentFile(const QString &file)
{
	static const QString title(PROG_TITLE);
	m_recent.SetCurFile(file);

	if(m_recent.GetCurFile() == "")
		this->setWindowTitle(title);
	else
		this->setWindowTitle(title + " -- " + m_recent.GetCurFile());
}


/**
 * (in)validates the path mesh if the obstacle configuration has changed
 */
void PathsTool::ValidatePathMesh(bool valid)
{
	emit PathMeshValid(valid);
}


/**
 * set the instrument's energy selection mode to either kf=const or ki=const
 */
void PathsTool::SetKfConstMode(bool kf_const)
{
	m_tascalc.SetKfix(kf_const);
}


/**
 * go to crystal coordinates
 */
void PathsTool::GotoCoordinates(
	t_real h, t_real k, t_real l,
	t_real ki, t_real kf,
	bool only_set_target)
{
	TasAngles angles = m_tascalc.GetAngles(h, k, l, ki, kf);

	if(!angles.mono_ok)
	{
		QMessageBox::critical(this, "Error", "Invalid monochromator angle.");
		return;
	}

	if(!angles.ana_ok)
	{
		QMessageBox::critical(this, "Error", "Invalid analyser angle.");
		return;
	}

	if(!angles.sample_ok)
	{
		QMessageBox::critical(this, "Error", "Invalid scattering angles.");
		return;
	}

	// set target coordinate angles
	if(only_set_target)
	{
		if(!m_pathProperties)
			return;

		auto pathwidget = m_pathProperties->GetWidget();
		if(!pathwidget)
			return;

		const t_real *sensesCCW = m_tascalc.GetScatteringSenses();
		t_real a2_abs = angles.monoXtalAngle * 2. * sensesCCW[0];
		t_real a4_abs = angles.sampleScatteringAngle * sensesCCW[1];

		pathwidget->SetTarget(
			a2_abs / tl2::pi<t_real> * 180.,
			a4_abs / tl2::pi<t_real> * 180.);
	}

	// set instrument angles
	else
	{
		// set scattering angles
		m_instrspace.GetInstrument().GetMonochromator().SetAxisAngleOut(
			t_real{2} * angles.monoXtalAngle);
		m_instrspace.GetInstrument().GetSample().SetAxisAngleOut(
			angles.sampleScatteringAngle);
		m_instrspace.GetInstrument().GetAnalyser().SetAxisAngleOut(
			t_real{2} * angles.anaXtalAngle);

		// set crystal angles
		m_instrspace.GetInstrument().GetMonochromator().SetAxisAngleInternal(
			angles.monoXtalAngle);
		m_instrspace.GetInstrument().GetSample().SetAxisAngleInternal(
			angles.sampleXtalAngle);
		m_instrspace.GetInstrument().GetAnalyser().SetAxisAngleInternal(
			angles.anaXtalAngle);

		m_tascalc.SetKfix(kf);
	}
}


/**
 * set the instrument angles to the specified ones
 * (angles have to be positive as scattering senses are applied in the function)
 */
void PathsTool::GotoAngles(std::optional<t_real> a1,
	std::optional<t_real> a3, std::optional<t_real> a4,
	std::optional<t_real> a5, bool only_set_target)
{
	// set target coordinate angles
	if(only_set_target && (a1 || a5) && a4)
	{
		if(!m_pathProperties)
			return;
		auto pathwidget = m_pathProperties->GetWidget();
		if(!pathwidget)
			return;

		// is kf or ki fixed?
		bool kf_fixed = true;
		if(!std::get<1>(m_tascalc.GetKfix()))
			kf_fixed = false;

		// move either monochromator or analyser depending if kf=fixed
		t_real _a2 = kf_fixed ? *a1 * 2. : *a5 * 2.;
		t_real _a4 = *a4;

		pathwidget->SetTarget(
			_a2 / tl2::pi<t_real> * 180.,
			_a4 / tl2::pi<t_real> * 180.);
	}

	// set instrument angles
	else
	{
		const t_real *sensesCCW = m_tascalc.GetScatteringSenses();

		// set mono angle
		if(a1)
		{
			*a1 *= sensesCCW[0];
			m_instrspace.GetInstrument().GetMonochromator(). SetAxisAngleOut(t_real{2} * *a1);
			m_instrspace.GetInstrument().GetMonochromator().SetAxisAngleInternal(*a1);
		}

		// set sample crystal angle
		if(a3)
		{
			*a3 *= sensesCCW[1];
			m_instrspace.GetInstrument().GetSample().SetAxisAngleInternal(*a3);
		}

		// set sample scattering angle
		if(a4)
		{
			*a4 *= sensesCCW[1];
			m_instrspace.GetInstrument().GetSample().SetAxisAngleOut(*a4);
		}

		// set ana angle
		if(a5)
		{
			*a5 *= sensesCCW[2];
			m_instrspace.GetInstrument().GetAnalyser().SetAxisAngleOut(t_real{2} * *a5);
			m_instrspace.GetInstrument().GetAnalyser().SetAxisAngleInternal(*a5);
		}
	}
}


/**
 * called after the plotter has initialised
 */
void PathsTool::AfterGLInitialisation()
{
	// GL device info
	std::tie(m_gl_ver, m_gl_shader_ver, m_gl_vendor, m_gl_renderer)
		= m_renderer->GetGlDescr();

	// get viewing angle
	t_real viewingAngle = m_renderer ? m_renderer->GetCamViewingAngle() : tl2::pi<t_real>*0.5;
	m_camProperties->GetWidget()->SetViewingAngle(viewingAngle*t_real{180}/tl2::pi<t_real>);

	// get perspective projection flag
	bool persp = m_renderer ? m_renderer->GetPerspectiveProjection() : true;
	m_camProperties->GetWidget()->SetPerspectiveProj(persp);

	// get camera position
	t_vec3_gl campos = m_renderer ? m_renderer->GetCamPosition() : tl2::zero<t_vec3_gl>(3);
	m_camProperties->GetWidget()->SetCamPosition(t_real(campos[0]), t_real(campos[1]), t_real(campos[2]));

	// get camera rotation
	t_vec2_gl camrot = m_renderer ? m_renderer->GetCamRotation() : tl2::zero<t_vec2_gl>(2);
	m_camProperties->GetWidget()->SetCamRotation(
		t_real(camrot[0])*t_real{180}/tl2::pi<t_real>,
		t_real(camrot[1])*t_real{180}/tl2::pi<t_real>);

	// load an initial instrument definition
	if(std::string instrfile = g_res.FindResource(m_initialInstrFile); !instrfile.empty())
	{
		if(OpenFile(instrfile.c_str()))
			m_renderer->LoadInstrument(m_instrspace);
	}
}


/**
 * mouse coordinates on base plane
 */
void PathsTool::CursorCoordsChanged(t_real_gl x, t_real_gl y)
{
	m_mouseX = x;
	m_mouseY = y;
	UpdateStatusLabel();
}


/**
 * mouse is over an object
 */
void PathsTool::PickerIntersection(const t_vec3_gl* /*pos*/,
	std::string obj_name, const t_vec3_gl* /*posSphere*/)
{
	m_curObj = obj_name;
	UpdateStatusLabel();
}


/**
 * clicked on an object
 */
void PathsTool::ObjectClicked(const std::string& obj, bool /*left*/, bool middle, bool right)
{
	if(!m_renderer)
		return;

	// show context menu for object
	if(right && obj != "")
	{
		m_curContextObj = obj;

		QPoint pos = m_renderer->GetMousePosition(true);
		pos.setX(pos.x() + 8);
		pos.setY(pos.y() + 8);
		m_contextMenuObj->popup(pos);
	}

	// centre scene around object
	if(middle)
	{
		m_renderer->CentreCam(obj);
	}
}


/**
 * dragging an object
 */
void PathsTool::ObjectDragged(bool drag_start, const std::string& obj,
	t_real_gl x_start, t_real_gl y_start, t_real_gl x, t_real_gl y)
{
	/*std::cout << "Dragging " << obj
		<< " from (" << x_start << ", " << y_start << ")"
		<< " to (" << x << ", " << y << ")." << std::endl;*/

	m_instrspace.DragObject(drag_start, obj, x_start, y_start, x, y);
}


/**
 * set temporary status message, by default for 2 seconds
 */
void PathsTool::SetTmpStatus(const std::string& msg, int msg_duration)
{
	if(!m_statusbar)
		return;

	if(thread() == QThread::currentThread())
	{
		m_statusbar->showMessage(msg.c_str(), msg_duration);
	}
	else
	{
		// alternate call via meta object when coming from another thread
		QMetaObject::invokeMethod(m_statusbar, "showMessage", Qt::QueuedConnection,
			Q_ARG(QString, msg.c_str()),
			Q_ARG(int, msg_duration)
		);
	}
}


/**
 * update permanent status message
 */
void PathsTool::UpdateStatusLabel()
{
	const t_real maxRange = 1e6;

	if(!std::isfinite(m_mouseX) || !std::isfinite(m_mouseY))
		return;
	if(std::abs(m_mouseX) >= maxRange || std::abs(m_mouseY) >= maxRange)
		return;

	std::ostringstream ostr;
	ostr.precision(g_prec_gui);
	ostr << std::fixed << std::showpos
		<< "Cursor: (" << m_mouseX << ", " << m_mouseY << ") m";
	if(m_curObj != "")
		ostr << ", object: " << m_curObj;
	ostr << ".";
	m_labelStatus->setText(ostr.str().c_str());
}


/**
 * set permanent instrumetn status message
 */
void PathsTool::SetInstrumentStatus(const std::optional<t_vec>& Qopt, t_real E,
	bool in_angular_limits, bool colliding)
{
	using namespace tl2_ops;

	std::ostringstream ostr;
	ostr.precision(g_prec_gui);
	//ostr << "Position: ";
	if(Qopt)
	{
		t_vec Q = *Qopt;
		tl2::set_eps_0<t_vec>(Q, g_eps_gui);
		ostr << std::fixed << "Q = (" << Q << ") rlu, ";
	}
	else
		ostr << "Q invalid, ";

	tl2::set_eps_0<t_real>(E, g_eps_gui);
	ostr << std::fixed << "E = " << E << " meV, ";

	if(!in_angular_limits)
		ostr << "invalid angles, ";

	if(colliding)
		ostr << "collision detected!";
	else
		ostr << "no collision.";

	m_labelCollisionStatus->setText(ostr.str().c_str());
}


/**
 * create UI
 */
PathsTool::PathsTool(QWidget* pParent) : QMainWindow{pParent}
{
	setWindowTitle(PROG_TITLE);

	if(std::string icon_file = g_res.FindResource("res/taspaths.svg"); !icon_file.empty())
	{
		QIcon icon{icon_file.c_str()};
		setWindowIcon(icon);
	}

	// restore settings
	SettingsDlg::ReadSettings(&m_sett);


	// --------------------------------------------------------------------
	// rendering widget
	// --------------------------------------------------------------------
	// set gl surface format
	m_renderer->setFormat(tl2::gl_format(true, _GL_MAJ_VER, _GL_MIN_VER,
		m_multisamples, m_renderer->format()));

	auto plotpanel = new QWidget(this);

	connect(m_renderer.get(), &PathsRenderer::FloorPlaneCoordsChanged, this, &PathsTool::CursorCoordsChanged);
	connect(m_renderer.get(), &PathsRenderer::PickerIntersection, this, &PathsTool::PickerIntersection);
	connect(m_renderer.get(), &PathsRenderer::ObjectClicked, this, &PathsTool::ObjectClicked);
	connect(m_renderer.get(), &PathsRenderer::ObjectDragged, this, &PathsTool::ObjectDragged);
	connect(m_renderer.get(), &PathsRenderer::AfterGLInitialisation, this, &PathsTool::AfterGLInitialisation);

	// camera position
	connect(m_renderer.get(), &PathsRenderer::CamPositionChanged,
		[this](t_real_gl _x, t_real_gl _y, t_real_gl _z) -> void
		{
			t_real x = t_real(_x);
			t_real y = t_real(_y);
			t_real z = t_real(_z);

			if(m_camProperties)
				m_camProperties->GetWidget()->SetCamPosition(x, y, z);
		});

	// camera rotation
	connect(m_renderer.get(), &PathsRenderer::CamRotationChanged,
		[this](t_real_gl _phi, t_real_gl _theta) -> void
		{
			t_real phi = t_real(_phi);
			t_real theta = t_real(_theta);

			if(m_camProperties)
				m_camProperties->GetWidget()->SetCamRotation(
					phi*t_real{180}/tl2::pi<t_real>,
					theta*t_real{180}/tl2::pi<t_real>);
		});

	auto pGrid = new QGridLayout(plotpanel);
	pGrid->setSpacing(4);
	pGrid->setContentsMargins(4,4,4,4);

	pGrid->addWidget(m_renderer.get(), 0,0,1,4);

	setCentralWidget(plotpanel);
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// dock widgets
	// --------------------------------------------------------------------
	setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::VerticalTabs);

	m_tasProperties = std::make_shared<TASPropertiesDockWidget>(this);
	m_xtalProperties = std::make_shared<XtalPropertiesDockWidget>(this);
	m_xtalInfos = std::make_shared<XtalInfoDockWidget>(this);
	m_coordProperties = std::make_shared<CoordPropertiesDockWidget>(this);
	m_pathProperties = std::make_shared<PathPropertiesDockWidget>(this);
	m_camProperties = std::make_shared<CamPropertiesDockWidget>(this);

	addDockWidget(Qt::LeftDockWidgetArea, m_tasProperties.get());
	addDockWidget(Qt::LeftDockWidgetArea, m_xtalProperties.get());
	addDockWidget(Qt::RightDockWidgetArea, m_xtalInfos.get());
	addDockWidget(Qt::RightDockWidgetArea, m_coordProperties.get());
	addDockWidget(Qt::RightDockWidgetArea, m_pathProperties.get());
	addDockWidget(Qt::RightDockWidgetArea, m_camProperties.get());

	auto* taswidget = m_tasProperties->GetWidget().get();
	auto* xtalwidget = m_xtalProperties->GetWidget().get();
	auto* coordwidget = m_coordProperties->GetWidget().get();
	auto* pathwidget = m_pathProperties->GetWidget().get();
	auto* camwidget = m_camProperties->GetWidget().get();

	// scattering angles
	connect(taswidget, &TASPropertiesWidget::MonoScatteringAngleChanged,
		[this](t_real angle) -> void
		{
			m_instrspace.GetInstrument().GetMonochromator().
				SetAxisAngleOut(angle/t_real{180}*tl2::pi<t_real>);
		});
	connect(taswidget, &TASPropertiesWidget::SampleScatteringAngleChanged,
		[this](t_real angle) -> void
		{
			m_instrspace.GetInstrument().GetSample().
				SetAxisAngleOut(angle/t_real{180}*tl2::pi<t_real>);
		});
	connect(taswidget, &TASPropertiesWidget::AnaScatteringAngleChanged,
		[this](t_real angle) -> void
		{
			m_instrspace.GetInstrument().GetAnalyser().
				SetAxisAngleOut(angle/t_real{180}*tl2::pi<t_real>);
		});

	// crystal angles
	connect(taswidget, &TASPropertiesWidget::MonoCrystalAngleChanged,
		[this](t_real angle) -> void
		{
			m_instrspace.GetInstrument().GetMonochromator().
				SetAxisAngleInternal(angle/t_real{180}*tl2::pi<t_real>);
		});
	connect(taswidget, &TASPropertiesWidget::SampleCrystalAngleChanged,
		[this](t_real angle) -> void
		{
			m_instrspace.GetInstrument().GetSample().
				SetAxisAngleInternal(angle/t_real{180}*tl2::pi<t_real>);
		});
	connect(taswidget, &TASPropertiesWidget::AnaCrystalAngleChanged,
		[this](t_real angle) -> void
		{
			m_instrspace.GetInstrument().GetAnalyser().
				SetAxisAngleInternal(angle/t_real{180}*tl2::pi<t_real>);
		});

	// d spacings
	connect(taswidget, &TASPropertiesWidget::DSpacingsChanged,
		[this](t_real dmono, t_real dana) -> void
		{
			m_tascalc.SetMonochromatorD(dmono);
			m_tascalc.SetAnalyserD(dana);
		});

	// scattering senses
	connect(taswidget, &TASPropertiesWidget::ScatteringSensesChanged,
		[this](bool monoccw, bool sampleccw, bool anaccw) -> void
		{
			m_tascalc.SetScatteringSenses(monoccw, sampleccw, anaccw);
		});

	// set current to target angles
	connect(taswidget, &TASPropertiesWidget::GotoAngles,
		[this](
			t_real a1, t_real a2,
			t_real a3, t_real a4,
			t_real a5, t_real a6,
			bool only_set_target) -> void
		{
			const t_real *sensesCCW = this->m_tascalc.GetScatteringSenses();

			a1 = a1 / 180. * tl2::pi<t_real> * sensesCCW[0];
			a2 = a2 / 180. * tl2::pi<t_real> * sensesCCW[0];
			a3 = a3 / 180. * tl2::pi<t_real> * sensesCCW[1];
			a4 = a4 / 180. * tl2::pi<t_real> * sensesCCW[1];
			a5 = a5 / 180. * tl2::pi<t_real> * sensesCCW[2];
			a6 = a6 / 180. * tl2::pi<t_real> * sensesCCW[2];

			this->GotoAngles(a1, a3, a4, a5, only_set_target);
		});

	// camera viewing angle
	connect(camwidget, &CamPropertiesWidget::ViewingAngleChanged,
		[this](t_real angle) -> void
		{
			if(m_renderer)
				m_renderer->SetCamViewingAngle(angle/t_real{180}*tl2::pi<t_real>);
		});

	// camera projection
	connect(camwidget, &CamPropertiesWidget::PerspectiveProjChanged,
		[this](bool persp) -> void
		{
			if(m_renderer)
				m_renderer->SetPerspectiveProjection(persp);
		});

	// camera position
	connect(camwidget, &CamPropertiesWidget::CamPositionChanged,
		[this](t_real _x, t_real _y, t_real _z) -> void
		{
			t_real_gl x = t_real_gl(_x);
			t_real_gl y = t_real_gl(_y);
			t_real_gl z = t_real_gl(_z);

			if(m_renderer)
				m_renderer->SetCamPosition(tl2::create<t_vec3_gl>({x, y, z}));
		});

	// camera rotation
	connect(camwidget, &CamPropertiesWidget::CamRotationChanged,
		[this](t_real _phi, t_real _theta) -> void
		{
			t_real_gl phi = t_real_gl(_phi);
			t_real_gl theta = t_real_gl(_theta);

			if(m_renderer)
				m_renderer->SetCamRotation(tl2::create<t_vec2_gl>(
					{phi/t_real_gl{180}*tl2::pi<t_real_gl>,
					theta/t_real_gl{180}*tl2::pi<t_real_gl>}));
		});

	// lattice constants and angles
	connect(xtalwidget, &XtalPropertiesWidget::LatticeChanged,
		[this](t_real a, t_real b, t_real c, t_real alpha, t_real beta, t_real gamma) -> void
		{
			m_tascalc.SetSampleLatticeConstants(a, b, c);
			m_tascalc.SetSampleLatticeAngles(alpha, beta, gamma, false);

			m_tascalc.UpdateB();
			UpdateUB();
		});

	connect(xtalwidget, &XtalPropertiesWidget::PlaneChanged,
		[this](t_real vec1_x, t_real vec1_y, t_real vec1_z, t_real vec2_x, t_real vec2_y, t_real vec2_z) -> void
		{
			m_tascalc.SetSampleScatteringPlane(
				vec1_x, vec1_y, vec1_z,
				vec2_x, vec2_y, vec2_z);

			UpdateUB();
		});

	// goto coordinates
	using t_gotocoords =
		void (PathsTool::*)(t_real, t_real, t_real, t_real, t_real, bool);

	connect(coordwidget, &CoordPropertiesWidget::GotoCoordinates,
		this, static_cast<t_gotocoords>(&PathsTool::GotoCoordinates));

	// kf=const mode selection, TODO: maybe move this to TASProperties
	connect(coordwidget, &CoordPropertiesWidget::KfConstModeChanged,
		this, &PathsTool::SetKfConstMode);

	// goto angles
	connect(pathwidget, &PathPropertiesWidget::GotoAngles,
		[this](t_real a2, t_real a4)
		{
			a2 = a2 / 180. * tl2::pi<t_real>;
			a4 = a4 / 180. * tl2::pi<t_real>;

			this->GotoAngles(a2/2., std::nullopt, a4, std::nullopt, false);
		});

	// target angles have changed
	connect(pathwidget, &PathPropertiesWidget::TargetChanged,
		[this](t_real a2, t_real a4)
		{
			//std::cout << "target angles: " << a2 << ", " << a4 << std::endl;
			const t_real *sensesCCW = m_tascalc.GetScatteringSenses();

			a2 = a2 / 180. * tl2::pi<t_real> * sensesCCW[0];
			a4 = a4 / 180. * tl2::pi<t_real> * sensesCCW[1];

			m_targetMonoScatteringAngle = a2;
			m_targetSampleScatteringAngle = a4;

			if(this->m_dlgConfigSpace)
				this->m_dlgConfigSpace->UpdateTarget(a2, a4, sensesCCW);
		});


	// calculate path mesh
	connect(pathwidget, &PathPropertiesWidget::CalculatePathMesh,
		this, &PathsTool::CalculatePathMesh);

	// calculate path
	connect(pathwidget, &PathPropertiesWidget::CalculatePath,
		this, &PathsTool::CalculatePath);

	// a new path has been calculated
	connect(this, &PathsTool::PathAvailable,
		pathwidget, &PathPropertiesWidget::PathAvailable);

	// a path mesh has been (in)validated
	connect(this, &PathsTool::PathMeshValid,
		pathwidget, &PathPropertiesWidget::PathMeshValid);

	// a new path vertex has been chosen on the path slider
	connect(pathwidget, &PathPropertiesWidget::TrackPath,
		this, &PathsTool::TrackPath);
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// menu bar
	// --------------------------------------------------------------------
	m_menubar = new QMenuBar(this);


	// file menu
	QMenu *menuFile = new QMenu("File", m_menubar);

	QAction *actionNew = new QAction(QIcon::fromTheme("document-new"), "New", menuFile);
	QAction *actionOpen = new QAction(QIcon::fromTheme("document-open"), "Open...", menuFile);
	QAction *actionSave = new QAction(QIcon::fromTheme("document-save"), "Save", menuFile);
	QAction *actionSaveAs = new QAction(QIcon::fromTheme("document-save-as"), "Save As...", menuFile);
	QAction *actionScreenshot = new QAction(QIcon::fromTheme("image-x-generic"), "Save Screenshot...", menuFile);
	QAction *actionGarbage = new QAction(QIcon::fromTheme("user-trash-full"), "Collect Garbage", menuFile);
	QAction *actionSettings = new QAction(QIcon::fromTheme("preferences-system"), "Settings...", menuFile);
	QAction *actionQuit = new QAction(QIcon::fromTheme("application-exit"), "Quit", menuFile);

	// export menu
	QMenu *menuExportPath = new QMenu("Export Path", m_menubar);

	QAction *acExportRaw = new QAction("To Raw...", menuExportPath);
	QAction *acExportNomad = new QAction("To Nomad...", menuExportPath);
	QAction *acExportNicos = new QAction("To Nicos...", menuExportPath);

	menuExportPath->addAction(acExportRaw);
	menuExportPath->addAction(acExportNomad);
	menuExportPath->addAction(acExportNicos);

	// shortcuts
	actionNew->setShortcut(QKeySequence::New);
	actionOpen->setShortcut(QKeySequence::Open);
	actionSave->setShortcut(QKeySequence::Save);
	actionSaveAs->setShortcut(QKeySequence::SaveAs);
	actionSettings->setShortcut(QKeySequence::Preferences);
	actionQuit->setShortcut(QKeySequence::Quit);

	m_menuOpenRecent = new QMenu("Open Recent", menuFile);
	m_menuOpenRecent->setIcon(QIcon::fromTheme("document-open-recent"));

	m_recent.SetRecentFilesMenu(m_menuOpenRecent);
	m_recent.SetMaxRecentFiles(MAX_RECENT_FILES);
	m_recent.SetOpenFunc(&m_open_func);

	actionSettings->setMenuRole(QAction::PreferencesRole);
	actionQuit->setMenuRole(QAction::QuitRole);

	// connections
	connect(actionNew, &QAction::triggered, this, [this]() { this->NewFile(); });
	connect(actionOpen, &QAction::triggered, this, [this]() { this->OpenFile(); });
	connect(actionSave, &QAction::triggered, this, [this]() { this->SaveFile(); });
	connect(actionSaveAs, &QAction::triggered, this, [this]() { this->SaveFileAs(); });
	connect(actionScreenshot, &QAction::triggered, this, [this]() { this->SaveScreenshot(); });
	connect(actionQuit, &QAction::triggered, this, &PathsTool::close);

	// collect garbage
	connect(actionGarbage, &QAction::triggered, this, [this]()
	{
		// remove any open dialogs
		if(this->m_dlgSettings)
			this->m_dlgSettings.reset();

		if(this->m_dlgGeoBrowser)
			this->m_dlgGeoBrowser.reset();

		if(this->m_dlgConfigSpace)
			this->m_dlgConfigSpace.reset();

		if(this->m_dlgXtalConfigSpace)
			this->m_dlgXtalConfigSpace.reset();

		if(this->m_dlgAbout)
			this->m_dlgAbout.reset();

		if(this->m_dlgLicenses)
			this->m_dlgLicenses.reset();
	});

	// show settings dialog
	connect(actionSettings, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgSettings)
		{
			this->m_dlgSettings = std::make_shared<SettingsDlg>(this, &m_sett);
			connect(&*this->m_dlgSettings, &SettingsDlg::SettingsHaveChanged,
				this, &PathsTool::InitSettings);
		}

		m_dlgSettings->show();
		m_dlgSettings->raise();
		m_dlgSettings->activateWindow();
	});

	connect(acExportRaw, &QAction::triggered, [this]() -> void
	{
		ExportPath(PathsExporterFormat::RAW);
	});

	connect(acExportNomad, &QAction::triggered, [this]() -> void
	{
		ExportPath(PathsExporterFormat::NOMAD);
	});

	connect(acExportNicos, &QAction::triggered, [this]() -> void
	{
		ExportPath(PathsExporterFormat::NICOS);
	});


	menuFile->addAction(actionNew);
	menuFile->addSeparator();
	menuFile->addAction(actionOpen);
	menuFile->addMenu(m_menuOpenRecent);
	menuFile->addSeparator();
	menuFile->addAction(actionSave);
	menuFile->addAction(actionSaveAs);
	menuFile->addAction(actionScreenshot);
	menuFile->addMenu(menuExportPath);
	menuFile->addSeparator();
	menuFile->addAction(actionGarbage);
	menuFile->addAction(actionSettings);
	menuFile->addSeparator();
	menuFile->addAction(actionQuit);


	// view menu
	QMenu *menuView = new QMenu("View", m_menubar);

	menuView->addAction(m_tasProperties->toggleViewAction());
	menuView->addAction(m_xtalProperties->toggleViewAction());
	menuView->addAction(m_xtalInfos->toggleViewAction());
	menuView->addAction(m_coordProperties->toggleViewAction());
	menuView->addAction(m_pathProperties->toggleViewAction());
	menuView->addAction(m_camProperties->toggleViewAction());
	//menuView->addSeparator();
	//menuView->addAction(acPersp);


	// geometry menu
	QMenu *menuGeo = new QMenu("Geometry", m_menubar);

	QAction *actionAddCuboidWall = new QAction(QIcon::fromTheme("insert-object"), "Add Wall", menuGeo);
	QAction *actionAddCylindricalWall = new QAction(QIcon::fromTheme("insert-object"), "Add Pillar", menuGeo);
	QAction *actionGeoBrowser = new QAction(QIcon::fromTheme("document-properties"), "Object Browser...", menuGeo);

	connect(actionAddCuboidWall, &QAction::triggered, this, &PathsTool::AddWall);
	connect(actionAddCylindricalWall, &QAction::triggered, this, &PathsTool::AddPillar);
	connect(actionGeoBrowser, &QAction::triggered, this, &PathsTool::ShowGeometriesBrowser);

	menuGeo->addAction(actionAddCuboidWall);
	menuGeo->addAction(actionAddCylindricalWall);
	menuGeo->addSeparator();
	menuGeo->addAction(actionGeoBrowser);



	// calculate menu
	QMenu *menuCalc = new QMenu("Calculation", m_menubar);

	QAction *actionConfigSpace = new QAction("Angular Configuration Space...", menuCalc);
	QAction *actionXtalConfigSpace = new QAction("Crystal Configuration Space...", menuCalc);

	connect(actionConfigSpace, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgConfigSpace)
		{
			this->m_dlgConfigSpace = std::make_shared<ConfigSpaceDlg>(this, &m_sett);
			this->m_dlgConfigSpace->SetPathsBuilder(&this->m_pathsbuilder);

			this->connect(this->m_dlgConfigSpace.get(), &ConfigSpaceDlg::GotoAngles,
				this, &PathsTool::GotoAngles);

			this->connect(this->m_dlgConfigSpace.get(), &ConfigSpaceDlg::PathMeshAvailable,
				[this]() { this->ValidatePathMesh(true); } );
		}

		m_dlgConfigSpace->show();
		m_dlgConfigSpace->raise();
		m_dlgConfigSpace->activateWindow();
	});

	connect(actionXtalConfigSpace, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgXtalConfigSpace)
		{
			this->m_dlgXtalConfigSpace = std::make_shared<XtalConfigSpaceDlg>(this, &m_sett);
			this->m_dlgXtalConfigSpace->SetInstrumentSpace(&this->m_instrspace);
			this->m_dlgXtalConfigSpace->SetTasCalculator(&this->m_tascalc);

			using t_gotocoords =
				void (PathsTool::*)(t_real, t_real, t_real, t_real, t_real);

			this->connect(
				this->m_dlgXtalConfigSpace.get(), &XtalConfigSpaceDlg::GotoCoordinates,
				this, static_cast<t_gotocoords>(&PathsTool::GotoCoordinates));
		}

		m_dlgXtalConfigSpace->show();
		m_dlgXtalConfigSpace->raise();
		m_dlgXtalConfigSpace->activateWindow();
	});

	menuCalc->addAction(actionConfigSpace);
	menuCalc->addAction(actionXtalConfigSpace);


	// tools menu
	QMenu *menuTools = new QMenu("Tools", m_menubar);

	fs::path hullpath = fs::path(g_apppath) / fs::path("taspaths_hull" EXEC_EXTENSION);
	fs::path linespath = fs::path(g_apppath) / fs::path("taspaths_lines" EXEC_EXTENSION);
	fs::path polypath = fs::path(g_apppath) / fs::path("taspaths_poly" EXEC_EXTENSION);

	std::size_t num_tools = 0;
	if(fs::exists(linespath))
	{
		QAction *acLinesTool = new QAction("Line Segment Voronoi Diagrams...", menuTools);
		menuTools->addAction(acLinesTool);
		++num_tools;

		connect(acLinesTool, &QAction::triggered, this, [linespath]()
		{
			create_process(linespath.string());
		});
	}

	if(fs::exists(hullpath))
	{
		QAction *acHullTool = new QAction("Vertex Voronoi Diagrams and Convex Hull...", menuTools);
		menuTools->addAction(acHullTool);
		++num_tools;

		connect(acHullTool, &QAction::triggered, this, [hullpath]()
		{
			create_process(hullpath.string());
		});
	}

	if(fs::exists(polypath))
	{
		QAction *acPolyTool = new QAction("Polygons...", menuTools);
		menuTools->addAction(acPolyTool);
		++num_tools;

		connect(acPolyTool, &QAction::triggered, this, [polypath]()
		{
			create_process(polypath.string());
		});
	}


	// help menu
	QMenu *menuHelp = new QMenu("Help", m_menubar);

	// if the help files were not found, remove its menu item
	std::string dev_docfile{};
	bool show_dev_doc = true;
	if(dev_docfile = g_res.FindResource("dev_doc/html/index.html"); dev_docfile.empty())
		show_dev_doc = false;

	QAction *actionDevDoc = nullptr;
	if(show_dev_doc)
		actionDevDoc = new QAction(QIcon::fromTheme("help-contents"), "Developer Documentation...", menuHelp);
	QAction *actionAboutQt = new QAction(QIcon::fromTheme("help-about"), "About Qt Libraries...", menuHelp);
	QAction *actionAboutGl = new QAction(QIcon::fromTheme("help-about"), "About Renderer...", menuHelp);
	QAction *actionLicenses = new QAction("Licenses...", menuHelp);
	QAction *actionBug = new QAction("Report Bug...", menuHelp);
	QAction *actionAbout = new QAction(QIcon::fromTheme("help-about"), "About TAS-Paths...", menuHelp);

	actionAboutQt->setMenuRole(QAction::AboutQtRole);
	actionAbout->setMenuRole(QAction::AboutRole);

	// open developer help
	if(actionDevDoc)
	{
		connect(actionDevDoc, &QAction::triggered, this, [this, dev_docfile]()
		{
			std::string dev_docfile_abs = fs::absolute(dev_docfile).string();
			QUrl url(("file://" + dev_docfile_abs).c_str(), QUrl::StrictMode);
			if(!QDesktopServices::openUrl(url))
			{
				QMessageBox::critical(this, "Error",
					"Cannot open developer documentation.");
			}
		});
	}

	connect(actionAboutQt, &QAction::triggered, this, []() { qApp->aboutQt(); });

	// show infos about renderer hardware
	connect(actionAboutGl, &QAction::triggered, this, [this]()
	{
		std::ostringstream ostrInfo;
		ostrInfo << "Rendering using the following device:\n\n";
		ostrInfo << "GL Vendor: " << m_gl_vendor << "\n";
		ostrInfo << "GL Renderer: " << m_gl_renderer << "\n";
		ostrInfo << "GL Version: " << m_gl_ver << "\n";
		ostrInfo << "GL Shader Version: " << m_gl_shader_ver << "\n";
		ostrInfo << "Device pixel ratio: " << devicePixelRatio() << "\n";
		QMessageBox::information(this, "About Renderer", ostrInfo.str().c_str());
	});

	// show licenses dialog
	connect(actionLicenses, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgLicenses)
			this->m_dlgLicenses =
				std::make_shared<LicensesDlg>(this, &m_sett);

		m_dlgLicenses->show();
		m_dlgLicenses->raise();
		m_dlgLicenses->activateWindow();
	});

	// show about dialog
	connect(actionAbout, &QAction::triggered, this, [this]()
	{
		if(!this->m_dlgAbout)
			this->m_dlgAbout = std::make_shared<AboutDlg>(this, &m_sett);

		m_dlgAbout->show();
		m_dlgAbout->raise();
		m_dlgAbout->activateWindow();
	});

	// go to bug report url
	connect(actionBug, &QAction::triggered, this, []()
	{
		QUrl url("https://code.ill.fr/scientific-software/takin/paths/-/issues");
		QDesktopServices::openUrl(url);
	});

	if(actionDevDoc)
	{
		menuHelp->addAction(actionDevDoc);
		menuHelp->addSeparator();
	}
	menuHelp->addAction(actionAboutQt);
	menuHelp->addAction(actionAboutGl);
	menuHelp->addSeparator();
	menuHelp->addAction(actionLicenses);
	menuHelp->addSeparator();
	menuHelp->addAction(actionBug);
	menuHelp->addAction(actionAbout);


	// menu bar
	m_menubar->addMenu(menuFile);
	m_menubar->addMenu(menuView);
	m_menubar->addMenu(menuGeo);
	m_menubar->addMenu(menuCalc);
	if(num_tools)
		m_menubar->addMenu(menuTools);
	m_menubar->addMenu(menuHelp);
	//m_menubar->setNativeMenuBar(false);
	setMenuBar(m_menubar);
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// context menu
	// --------------------------------------------------------------------
	m_contextMenuObj = new QMenu(this);

	QAction *actionObjRotP10 = new QAction(QIcon::fromTheme("object-rotate-left"), "Rotate Object by +10°", m_contextMenuObj);
	QAction *actionObjRotM10 = new QAction(QIcon::fromTheme("object-rotate-right"), "Rotate Object by -10°", m_contextMenuObj);
	QAction *actionObjRotP45 = new QAction(QIcon::fromTheme("object-rotate-left"), "Rotate Object by +45°", m_contextMenuObj);
	QAction *actionObjRotM45 = new QAction(QIcon::fromTheme("object-rotate-right"), "Rotate Object by -45°", m_contextMenuObj);
	QAction *actionObjCentreCam = new QAction(QIcon::fromTheme("camera-video"), "Centre Camera on Object", m_contextMenuObj);
	QAction *actionObjDel = new QAction(QIcon::fromTheme("edit-delete"), "Delete Object", m_contextMenuObj);
	QAction *actionObjProp = new QAction(QIcon::fromTheme("document-properties"), "Object Properties...", m_contextMenuObj);

	m_contextMenuObj->addAction(actionObjRotP10);
	m_contextMenuObj->addAction(actionObjRotM10);
	m_contextMenuObj->addAction(actionObjRotP45);
	m_contextMenuObj->addAction(actionObjRotM45);
	m_contextMenuObj->addSeparator();
	m_contextMenuObj->addAction(actionObjCentreCam);
	m_contextMenuObj->addSeparator();
	m_contextMenuObj->addAction(actionObjDel);
	m_contextMenuObj->addAction(actionObjProp);

	connect(actionObjRotP10, &QAction::triggered,
		[this]() { RotateCurrentObject(10./180.*tl2::pi<t_real>); });
	connect(actionObjRotM10, &QAction::triggered,
		[this]() { RotateCurrentObject(-10./180.*tl2::pi<t_real>); });
	connect(actionObjRotP45, &QAction::triggered,
		[this]() { RotateCurrentObject(45./180.*tl2::pi<t_real>); });
	connect(actionObjRotM45, &QAction::triggered,
		[this]() { RotateCurrentObject(-45./180.*tl2::pi<t_real>); });
	connect(actionObjDel, &QAction::triggered, this,
		&PathsTool::DeleteCurrentObject);
	connect(actionObjProp, &QAction::triggered, this,
		&PathsTool::ShowCurrentObjectProperties);
	connect(actionObjCentreCam, &QAction::triggered,
		[this]() { if(m_renderer) m_renderer->CentreCam(m_curContextObj); });
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// status bar
	// --------------------------------------------------------------------
	m_progress = new QProgressBar(this);
	m_progress->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_progress->setMinimum(0);
	m_progress->setMaximum(1000);

	QIcon stopIcon = QIcon::fromTheme("media-playback-stop");
	m_buttonStop = new QToolButton(this);
	m_buttonStop->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	if(stopIcon.isNull())
		m_buttonStop->setText("X");
	else
		m_buttonStop->setIcon(stopIcon);
	m_buttonStop->setToolTip("Stop calculation.");

	m_labelStatus = new QLabel(this);
	m_labelStatus->setSizePolicy(QSizePolicy::/*Ignored*/Expanding, QSizePolicy::Fixed);
	m_labelStatus->setFrameStyle(int(QFrame::Sunken) | int(QFrame::Panel));
	m_labelStatus->setLineWidth(1);

	m_labelCollisionStatus = new QLabel(this);
	m_labelCollisionStatus->setSizePolicy(QSizePolicy::/*Ignored*/Expanding, QSizePolicy::Fixed);
	m_labelCollisionStatus->setFrameStyle(int(QFrame::Sunken) | int(QFrame::Panel));
	m_labelCollisionStatus->setLineWidth(1);

	m_statusbar = new QStatusBar(this);
	m_statusbar->setSizeGripEnabled(true);
	m_statusbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_statusbar->addPermanentWidget(m_progress);
	m_statusbar->addPermanentWidget(m_buttonStop);
	m_statusbar->addPermanentWidget(m_labelCollisionStatus);
	m_statusbar->addPermanentWidget(m_labelStatus);
	setStatusBar(m_statusbar);

	connect(m_buttonStop, &QToolButton::clicked, [this]()
	{
		this->m_stop_requested = true;
		SetTmpStatus("Stop requested...");
	});
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// restory window size, position, and state
	// --------------------------------------------------------------------
	if(m_sett.contains("geo"))
		restoreGeometry(m_sett.value("geo").toByteArray());
	else
		resize(1200, 800);

	if(m_sett.contains("state"))
		restoreState(m_sett.value("state").toByteArray());

	// recent files
	if(m_sett.contains("recent_files"))
		m_recent.SetRecentFiles(m_sett.value("recent_files").toStringList());

	//QFont font = this->font();
	//font.setPointSizeF(14.);
	//this->setFont(font);
	// --------------------------------------------------------------------


	// --------------------------------------------------------------------
	// initialisations
	// --------------------------------------------------------------------
	InitSettings();

	m_pathsbuilder.SetInstrumentSpace(&this->m_instrspace);
	m_pathsbuilder.SetTasCalculator(&this->m_tascalc);

	m_pathsbuilder.AddProgressSlot(
		[this](bool /*start*/, bool /*end*/, t_real progress, const std::string& /*message*/)
		{
			//std::cout << "Progress: " << int(progress*100.) << " \%." << std::endl;
			if(!m_progress)
				return true;

			if(this->m_stop_requested)
				return false;

			if(this->thread() == QThread::currentThread())
			{
				//m_progress->setFormat((std::string("%p% -- ") + message).c_str());
				m_progress->setValue((int)(progress * m_progress->maximum()));
			}
			else
			{
				// alternate call via meta object when coming from another thread
				QMetaObject::invokeMethod(m_progress, "setValue", Qt::QueuedConnection,
					Q_ARG(int, (int)(progress * m_progress->maximum())));
				//QMetaObject::invokeMethod(m_progress, "update", Qt::QueuedConnection);
			}

			return true;
		});

	UpdateUB();
	// --------------------------------------------------------------------
}


/**
 * propagate (changed) global settings to each object
 */
void PathsTool::InitSettings()
{
	m_tascalc.SetSampleAngleOffset(g_a3_offs);

	m_instrspace.SetEpsilon(g_eps);
	m_instrspace.SetPolyIntersectionMethod(g_poly_intersection_method);

	m_pathsbuilder.SetMaxNumThreads(g_maxnum_threads);
	m_pathsbuilder.SetEpsilon(g_eps);
	m_pathsbuilder.SetAngularEpsilon(g_eps_angular);
	m_pathsbuilder.SetVoronoiEdgeEpsilon(g_eps_voronoiedge);
	m_pathsbuilder.SetSubdivisionLength(g_line_subdiv_len);
	m_pathsbuilder.SetVerifyPath(g_verifypath != 0);
	//m_pathsbuilder.SetUseRegionFunction(g_use_region_function != 0);

	if(m_renderer)
	{
		m_renderer->SetLightFollowsCursor(g_light_follows_cursor);
		m_renderer->EnableShadowRendering(g_enable_shadow_rendering);
	}
}


/**
 * add a wall to the instrument space
 */
void PathsTool::AddWall()
{
	auto wall = std::make_shared<BoxGeometry>();
	wall->SetHeight(4.);
	wall->SetDepth(0.5);
	wall->SetCentre(tl2::create<t_vec>({0, 0, wall->GetHeight()*0.5}));
	wall->SetLength(4.);
	wall->UpdateTrafo();

	static std::size_t wallcnt = 1;
	std::ostringstream ostrId;
	ostrId << "new wall " << wallcnt++;

	// invalidate the path mesh
	ValidatePathMesh(false);

	// add wall to instrument space
	m_instrspace.AddWall(std::vector<std::shared_ptr<Geometry>>{{wall}}, ostrId.str());

	// update object browser tree
	if(m_dlgGeoBrowser)
		m_dlgGeoBrowser->UpdateGeoTree(m_instrspace);

	// add a 3d representation of the wall
	if(m_renderer)
		m_renderer->AddWall(*wall, true);
}


/**
 * add a pillar to the instrument space
 */
void PathsTool::AddPillar()
{
	auto wall = std::make_shared<CylinderGeometry>();
	wall->SetHeight(4.);
	wall->SetCentre(tl2::create<t_vec>({0, 0, wall->GetHeight()*0.5}));
	wall->SetRadius(0.5);
	wall->UpdateTrafo();

	static std::size_t wallcnt = 1;
	std::ostringstream ostrId;
	ostrId << "new pillar " << wallcnt++;

	// invalidate the path mesh
	ValidatePathMesh(false);

	// add pillar to instrument space
	m_instrspace.AddWall(std::vector<std::shared_ptr<Geometry>>{{wall}}, ostrId.str());

	// update object browser tree
	if(m_dlgGeoBrowser)
		m_dlgGeoBrowser->UpdateGeoTree(m_instrspace);

	// add a 3d representation of the pillar
	if(m_renderer)
		m_renderer->AddWall(*wall, true);
}


/**
 * delete 3d object under the cursor
 */
void PathsTool::DeleteCurrentObject()
{
	DeleteObject(m_curContextObj);
}


/**
 * delete the given object from the instrument space
 */
void PathsTool::DeleteObject(const std::string& obj)
{
	if(obj == "")
		return;

	// remove object from instrument space
	if(m_instrspace.DeleteObject(obj))
	{
		// invalidate the path mesh
		ValidatePathMesh(false);

		// update object browser tree
		if(m_dlgGeoBrowser)
			m_dlgGeoBrowser->UpdateGeoTree(m_instrspace);

		// remove 3d representation of object
		if(m_renderer)
		{
			m_renderer->DeleteObject(obj);
			m_renderer->update();
		}
	}
	else
	{
		QMessageBox::warning(this, "Warning",
			QString("Object \"") + obj.c_str() + QString("\" cannot be deleted."));
	}
}


/**
 * rotate 3d object under the cursor
 */
void PathsTool::RotateCurrentObject(t_real angle)
{
	RotateObject(m_curContextObj, angle);
}


/**
 * rotate the given object
 */
void PathsTool::RotateObject(const std::string& objname, t_real angle)
{
	if(objname == "")
		return;

	// rotate the given object
	if(auto [ok, objgeo] = m_instrspace.RotateObject(objname, angle); ok)
	{
		// invalidate the path mesh
		ValidatePathMesh(false);

		// update object browser tree
		if(m_dlgGeoBrowser)
			m_dlgGeoBrowser->UpdateGeoTree(m_instrspace);

		// remove old 3d representation of object and create a new one
		// TODO: handle other cases besides walls
		if(m_renderer && objgeo)
		{
			m_renderer->DeleteObject(objname);
			m_renderer->AddWall(*objgeo, true);
		}
	}
	else
	{
		QMessageBox::warning(this, "Warning",
			QString("Object \"") + objname.c_str() + QString("\" cannot be rotated."));
	}
}


/**
 * open geometries browser and point to currently selected object
 */
void PathsTool::ShowCurrentObjectProperties()
{
	ShowGeometriesBrowser();

	if(m_dlgGeoBrowser)
	{
		m_dlgGeoBrowser->SelectObject(m_curContextObj);
	}
}


/**
 * open the geometry browser dialog
 */
void PathsTool::ShowGeometriesBrowser()
{
	if(!m_dlgGeoBrowser)
	{
		m_dlgGeoBrowser = std::make_shared<GeometriesBrowser>(this, &m_sett);

		connect(m_dlgGeoBrowser.get(), &GeometriesBrowser::SignalDeleteObject,
				this, &PathsTool::DeleteObject);
		connect(m_dlgGeoBrowser.get(), &GeometriesBrowser::SignalRenameObject,
				this, &PathsTool::RenameObject);
		connect(m_dlgGeoBrowser.get(), &GeometriesBrowser::SignalChangeObjectProperty,
				this, &PathsTool::ChangeObjectProperty);

		this->m_dlgGeoBrowser->UpdateGeoTree(this->m_instrspace);
	}

	m_dlgGeoBrowser->show();
	m_dlgGeoBrowser->raise();
	m_dlgGeoBrowser->activateWindow();
}


/**
 * rename the given object in the instrument space
 */
void PathsTool::RenameObject(const std::string& oldid, const std::string& newid)
{
	if(oldid == "" || newid == "" || oldid == newid)
		return;

	if(m_instrspace.RenameObject(oldid, newid))
	{
		// invalidate the path mesh
		ValidatePathMesh(false);

		// update object browser tree
		if(m_dlgGeoBrowser)
			m_dlgGeoBrowser->UpdateGeoTree(m_instrspace);

		// remove 3d representation of object
		if(m_renderer)
		{
			m_renderer->RenameObject(oldid, newid);
			m_renderer->update();
		}
	}
}


/**
 * change the properties of the given object in instrument space
 */
void PathsTool::ChangeObjectProperty(const std::string& objname, const ObjectProperty& prop)
{
	if(objname == "")
		return;

	// change object properties
	if(auto [ok, objgeo] = m_instrspace.SetProperties(objname, { prop} ); ok)
	{
		// invalidate the path mesh
		ValidatePathMesh(false);

		// update object browser tree
		if(m_dlgGeoBrowser)
			m_dlgGeoBrowser->UpdateGeoTree(m_instrspace);

		// remove old 3d representation of object and create a new one
		// TODO: handle other cases besides walls
		if(m_renderer && objgeo)
		{
			m_renderer->DeleteObject(objname);
			m_renderer->AddWall(*objgeo, true);
		}
	}
	else
	{
		QMessageBox::warning(this, "Warning",
			QString("Properties of object \"") + objname.c_str() + QString("\" cannot be changed."));
	}
}


/**
 * calculate the mesh of possible paths
 */
void PathsTool::CalculatePathMesh()
{
	m_stop_requested = false;

	// start calculation in a background thread
	m_futCalc = std::async(std::launch::async, [this]()
	{
		// check if a stop has been requested
		#define CHECK_STOP \
			if(this->m_stop_requested) \
			{ \
				SetTmpStatus("Calculation aborted."); \
				return; \
			}

		// invalidate the current mesh
		ValidatePathMesh(false);

		const auto& instr = m_instrspace.GetInstrument();

		// get the angular limits from the instrument model
		t_real starta2 = instr.GetMonochromator().GetAxisAngleOutLowerLimit();
		t_real enda2 = instr.GetMonochromator().GetAxisAngleOutUpperLimit();
		t_real starta4 = instr.GetSample().GetAxisAngleOutLowerLimit();
		t_real enda4 = instr.GetSample().GetAxisAngleOutUpperLimit();

		// angular padding
		t_real padding = 4;
		starta2 -= padding * g_a2_delta;
		enda2 += padding * g_a2_delta;
		starta4 -= padding * g_a4_delta;
		enda4 += padding * g_a4_delta;

		SetTmpStatus("Clearing old paths.", 0);
		m_pathsbuilder.Clear();

		CHECK_STOP

		SetTmpStatus("Calculating configuration space.", 0);
		if(!m_pathsbuilder.CalculateConfigSpace(
			g_a2_delta, g_a4_delta,
			starta2, enda2, starta4, enda4))
		{
			SetTmpStatus("Error: Configuration space calculation failed.");
			return;
		}

		CHECK_STOP

		SetTmpStatus("Calculating wall positions index tree.", 0);
		if(!m_pathsbuilder.CalculateWallsIndexTree())
		{
			SetTmpStatus("Error: Wall positions index tree calculation failed.");
			return;
		}

		CHECK_STOP

		SetTmpStatus("Calculating obstacle contour lines.", 0);
		if(!m_pathsbuilder.CalculateWallContours(true, false))
		{
			SetTmpStatus("Error: Obstacle contour lines calculation failed.");
			return;
		}

		CHECK_STOP

		SetTmpStatus("Calculating line segments.", 0);
		if(!m_pathsbuilder.CalculateLineSegments(g_use_region_function!=0))
		{
			SetTmpStatus("Error: Line segment calculation failed.");
			return;
		}

		CHECK_STOP

		SetTmpStatus("Calculating Voronoi regions.", 0);

		// voronoi backend
		VoronoiBackend backend{VoronoiBackend::BOOST};
		if(g_voronoi_backend == 1)
			backend = VoronoiBackend::CGAL;

		if(!m_pathsbuilder.CalculateVoronoi(false, backend, g_use_region_function!=0))
		{
			SetTmpStatus("Error: Voronoi regions calculation failed.");
			return;
		}

		CHECK_STOP

		// validate the new path mesh
		ValidatePathMesh(true);
		SetTmpStatus("Path mesh calculated.");
	});

	// block till the calculations are finished
	//m_futCalc.get();
}


/**
 * calculate the path from the current to the target position
 */
void PathsTool::CalculatePath()
{
	m_stop_requested = false;
	m_pathvertices.clear();

	// get the scattering angles
	const Instrument& instr = m_instrspace.GetInstrument();
	t_real curMonoScatteringAngle = instr.GetMonochromator().GetAxisAngleOut();
	t_real curSampleScatteringAngle = instr.GetSample().GetAxisAngleOut();

	// adjust scattering senses
	const t_real* sensesCCW = m_tascalc.GetScatteringSenses();

	curMonoScatteringAngle *= sensesCCW[0];
	curSampleScatteringAngle *= sensesCCW[1];
	t_real targetMonoScatteringAngle = m_targetMonoScatteringAngle * sensesCCW[0];
	t_real targetSampleScatteringAngle = m_targetSampleScatteringAngle * sensesCCW[1];

	// path options
	PathStrategy pathstrategy{PathStrategy::SHORTEST};
	if(g_pathstrategy == 1)
		pathstrategy = PathStrategy::PENALISE_WALLS;

	// find path from current to target position
	SetTmpStatus("Calculating path.");
	InstrumentPath path = m_pathsbuilder.FindPath(
		curMonoScatteringAngle, curSampleScatteringAngle,
		targetMonoScatteringAngle, targetSampleScatteringAngle,
		pathstrategy);

	if(!path.ok)
	{
		QMessageBox::critical(this, "Error", "No path could be found.");
		SetTmpStatus("Error: No path could be found.");
		return;
	}

	// get the vertices on the path
	SetTmpStatus("Retrieving path vertices.");
	m_pathvertices = m_pathsbuilder.GetPathVertices(path, true, false);
	emit PathAvailable(m_pathvertices.size());

	SetTmpStatus("Path calculated.");
}


/**
 * move the instrument to a position on the path
 */
void PathsTool::TrackPath(std::size_t idx)
{
	if(idx >= m_pathvertices.size())
		return;

	bool kf_fixed = true;
	if(!std::get<1>(m_tascalc.GetKfix()))
		kf_fixed = false;

	// get a vertex on the instrument path
	const t_vec2& vert = m_pathvertices[idx];

	// move either the monochromator or the analyser
	if(kf_fixed)
		GotoAngles(vert[1]*0.5, std::nullopt, vert[0], std::nullopt, false);
	else
		GotoAngles(std::nullopt, std::nullopt, vert[0], vert[1]*0.5, false);

	// automatically take a screenshot
	if(g_automatic_screenshots)
	{
		std::ostringstream ostrfilename;
		ostrfilename << "screenshot_" << std::setfill('0') << std::setw(8) << idx << ".png";
		if(g_combined_screenshots)
			SaveCombinedScreenshot(ostrfilename.str().c_str());
		else
			SaveScreenshot(ostrfilename.str().c_str());
	}
}
