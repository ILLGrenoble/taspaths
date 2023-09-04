/**
 * TAS properties dock widget
 * @author Tobias Weber <tweber@ill.fr>
 * @date mar-2021
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

#ifndef __TAS_PROP_WIDGET_H__
#define __TAS_PROP_WIDGET_H__

#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtWidgets/QWidget>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QCheckBox>

#include "src/core/types.h"


class TASPropertiesWidget : public QWidget
{Q_OBJECT
public:
	TASPropertiesWidget(QWidget *parent=nullptr);
	virtual ~TASPropertiesWidget();

	TASPropertiesWidget(const TASPropertiesWidget&) = delete;
	const TASPropertiesWidget& operator=(const TASPropertiesWidget&) = delete;

	// save and load the dock widget's settings
	boost::property_tree::ptree Save() const;
	bool Load(const boost::property_tree::ptree& prop);


public slots:
	void SetMonoScatteringAngle(t_real angle);
	void SetSampleScatteringAngle(t_real angle);
	void SetAnaScatteringAngle(t_real angle);

	void SetMonoCrystalAngle(t_real angle);
	void SetSampleCrystalAngle(t_real angle);
	void SetAnaCrystalAngle(t_real angle);

	void SetAngles(t_real a1, t_real a2, t_real a3, t_real a4, t_real a5, t_real a6);

	t_real GetMonoScatteringAngle() const;
	t_real GetSampleScatteringAngle() const;
	t_real GetAnaScatteringAngle() const;

	t_real GetMonoCrystalAngle() const;
	t_real GetSampleCrystalAngle() const;
	t_real GetAnaCrystalAngle() const;

	void SetDSpacings(t_real dmono, t_real dana);
	void SetScatteringSenses(bool monoccw, bool sampleccw, bool anaccw);


signals:
	// angles are in deg
	void MonoScatteringAngleChanged(t_real angle);
	void SampleScatteringAngleChanged(t_real angle);
	void AnaScatteringAngleChanged(t_real angle);

	void MonoCrystalAngleChanged(t_real angle);
	void SampleCrystalAngleChanged(t_real angle);
	void AnaCrystalAngleChanged(t_real angle);

	void DSpacingsChanged(t_real dmono, t_real dana);
	void ScatteringSensesChanged(bool monoccw, bool sampleccw, bool anaccw);

	void GotoAngles(t_real a1, t_real a2, t_real a3, t_real a4, t_real a5, t_real a6, bool only_set_target);


private:
	// scattering angles
	QDoubleSpinBox *m_spinMonoScAngle{nullptr};
	QDoubleSpinBox *m_spinSampleScAngle{nullptr};
	QDoubleSpinBox *m_spinAnaScAngle{nullptr};

	// crystal angles
	QDoubleSpinBox *m_spinMonoXtalAngle{nullptr};
	QDoubleSpinBox *m_spinSampleXtalAngle{nullptr};
	QDoubleSpinBox *m_spinAnaXtalAngle{nullptr};

	// d spacings
	QDoubleSpinBox *m_spinMonoD{nullptr};
	QDoubleSpinBox *m_spinAnaD{nullptr};

	// scattering senses
	QCheckBox *m_checkScatteringSense[3]{nullptr, nullptr, nullptr};
};



class TASPropertiesDockWidget : public QDockWidget
{
public:
	TASPropertiesDockWidget(QWidget *parent=nullptr);
	virtual ~TASPropertiesDockWidget();

	TASPropertiesDockWidget(const TASPropertiesDockWidget&) = delete;
	const TASPropertiesDockWidget& operator=(const TASPropertiesDockWidget&) = delete;

	std::shared_ptr<TASPropertiesWidget> GetWidget() { return m_widget; }


private:
    std::shared_ptr<TASPropertiesWidget> m_widget;
};


#endif
