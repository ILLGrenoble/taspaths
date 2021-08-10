/**
 * xtal configuration space
 * @author Tobias Weber <tweber@ill.fr>
 * @date aug-2021
 * @license GPLv3, see 'LICENSE' file
 */

#ifndef __TAKIN_PATHS_XTAL_CFGSPACE_H__
#define __TAKIN_PATHS_XTAL_CFGSPACE_H__

#include <QtCore/QSettings>
#include <QtWidgets/QDialog>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QDoubleSpinBox>

#include <cstdint>
#include <memory>

#include "qcustomplot/qcustomplot.h"

#include "src/libs/img.h"
#include "src/core/InstrumentSpace.h"
#include "src/core/TasCalculator.h"


class XtalConfigSpaceDlg : public QDialog
{ Q_OBJECT
public:
	XtalConfigSpaceDlg(QWidget* parent = nullptr, QSettings *sett = nullptr);
	virtual ~XtalConfigSpaceDlg();

	void UpdatePlotRanges();
	void Calculate();

	// ------------------------------------------------------------------------
	// input instrument
	// ------------------------------------------------------------------------
	void SetInstrumentSpace(const InstrumentSpace* instr) { m_instrspace = instr; }
	const InstrumentSpace* GetInstrumentSpace() const { return m_instrspace; }

	void SetTasCalculator(const TasCalculator* tascalc) { m_tascalc = tascalc; }
	const TasCalculator* GetTasCalculator() const { return m_tascalc; }
	// ------------------------------------------------------------------------


protected:
	virtual void accept() override;

	void RedrawPlot();

	// either move instrument by clicking in the plot or enable plot zoom mode
	void SetInstrumentMovable(bool moveInstr);


private:
	QSettings *m_sett{nullptr};

	// plot curves
	std::shared_ptr<QCustomPlot> m_plot;
	QCPColorMap* m_colourMap{};

	QLabel *m_status{};
	QDoubleSpinBox *m_spinVec1Start{}, *m_spinVec1End{}, *m_spinVec1Delta{};
	QDoubleSpinBox *m_spinVec2Start{}, *m_spinVec2End{}, *m_spinVec2Delta{};
	QDoubleSpinBox *m_spinE{};

	const InstrumentSpace *m_instrspace{};
    const TasCalculator *m_tascalc{};

	geo::Image<std::uint8_t> m_img{};
	bool m_moveInstr = true;
};

#endif