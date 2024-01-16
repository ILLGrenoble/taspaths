/**
 * angular configuration space dialog
 * @author Tobias Weber <tweber@ill.fr>
 * @date may-2021
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

#ifndef __TAKIN_PATHS_CFGSPACE_H__
#define __TAKIN_PATHS_CFGSPACE_H__

#include <QtCore/QSettings>
#include <QtWidgets/QDialog>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QDoubleSpinBox>

#include <cstdint>
#include <memory>

#include "src/core/PathsBuilder.h"
#include "qcp_wrapper.h"


class ConfigSpaceDlg : public QDialog
{ Q_OBJECT
public:
	ConfigSpaceDlg(QWidget* parent = nullptr, QSettings *sett = nullptr);
	virtual ~ConfigSpaceDlg();

	ConfigSpaceDlg(const ConfigSpaceDlg&) = delete;
	const ConfigSpaceDlg& operator=(const ConfigSpaceDlg&) = delete;

	void SetPathsBuilder(PathsBuilder* builder);
	void UnsetPathsBuilder();

	void CalculatePathMesh();
	void CalculatePath();

	void SetPlotRanges();
	void UpdatePlotRanges();

	// receivers for instrument (space) and target position update signals
	void UpdateInstrument(const Instrument& instr, const t_real* sensesCCW = nullptr);
	void UpdateTarget(t_real monoScAngle, t_real sampleScAngle, const t_real* sensesCCW = nullptr);

	void EmitGotoAngles(std::optional<t_real> a1,
		std::optional<t_real> a3, std::optional<t_real> a4,
		std::optional<t_real> a5);

	// block path calculations, e.g. during path tracking
	void SetBlockCalc(bool b) { m_block_calc = b; }
	bool GetBlockCalc() const { return m_block_calc; }


protected:
	virtual void accept() override;

	// voronoi / path mesh plot curves
	void ClearVoronoiPlotCurves();
	void AddVoronoiPlotCurve(const QVector<t_real>& x, const QVector<t_real>& y,
		t_real width = 1., QColor colour = QColor::fromRgbF(1., 1., 1.));
	void RedrawVoronoiPlot();

	// path plot curve
	void ClearPathPlotCurve();
	void SetPathPlotCurve(const QVector<t_real>& x, const QVector<t_real>& y,
		t_real width = 1., QColor colour = QColor::fromRgbF(1., 1., 1.));
	void RedrawPathPlot();

	// either move instrument by clicking in the plot or enable plot zoom mode
	void SetInstrumentMovable(bool moveInstr);


private:
	QSettings *m_sett{nullptr};
	std::unique_ptr<QProgressDialog> m_progress{};

	// angular ranges
	t_real m_starta2 = 0.;
	t_real m_enda2 = tl2::pi<t_real>;
	t_real m_starta4 = -tl2::pi<t_real>;
	t_real m_enda4 = tl2::pi<t_real>;

	// plot curves
	std::shared_ptr<QCustomPlot> m_plot{};
	QCPColorMap* m_colourMap{};
	std::vector<QCPCurve*> m_vorocurves{};
	QCPCurve* m_pathcurve = nullptr;
	std::vector<t_vec2> m_pathvertices{};

	// current (start) instrument position
	t_real m_curMonoScatteringAngle{};
	t_real m_curSampleScatteringAngle{};
	QCPGraph *m_instrposplot{};

	// target instrument position
	t_real m_targetMonoScatteringAngle{};
	t_real m_targetSampleScatteringAngle{};
	QCPGraph *m_targetposplot{};

	QLabel *m_status{};
	QDoubleSpinBox *m_spinDelta2ThS{}, *m_spinDelta2ThM{};

	PathsBuilder *m_pathsbuilder{};
	boost::signals2::connection m_pathsbuilderslot{};

	// path mesh options
	ContourBackend m_contourbackend{ContourBackend::INTERNAL};
	VoronoiBackend m_voronoibackend{VoronoiBackend::BOOST};
	bool m_use_region_function = true;
	bool m_grouplines = false;
	bool m_simplifycontour = true;
	bool m_splitcontour = false;
	bool m_calcvoronoi = true;
	bool m_subdivide_path = false;

	// path options
	PathStrategy m_pathstrategy{PathStrategy::SHORTEST};
	bool m_autocalcpath = true;
	bool m_syncpath = true;
	bool m_movetarget = false;
	bool m_moveInstr = true;

	// block path calculation
	bool m_block_calc = false;


signals:
	void GotoAngles(std::optional<t_real> a1,
		std::optional<t_real> a3, std::optional<t_real> a4,
		std::optional<t_real> a5, bool move_target);

	void PathMeshAvailable();
	void PathAvailable(const InstrumentPath& path);


protected slots:
	bool PathsBuilderProgress(CalculationState state, t_real progress, const std::string& message);


public slots:
	bool SaveFigure(const QString& file);
	bool CopyFigure();
};


#endif
