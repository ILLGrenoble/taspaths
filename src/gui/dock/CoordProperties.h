/**
 * xtal position properties dock widget
 * @author Tobias Weber <tweber@ill.fr>
 * @date apr-2021
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

#ifndef __COORD_PROP_WIDGET_H__
#define __COORD_PROP_WIDGET_H__

#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtWidgets/QWidget>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QCheckBox>

#include "src/core/types.h"


class CoordPropertiesWidget : public QWidget
{Q_OBJECT
public:
	CoordPropertiesWidget(QWidget *parent=nullptr);
	virtual ~CoordPropertiesWidget();

	CoordPropertiesWidget(const CoordPropertiesWidget&) = delete;
        CoordPropertiesWidget& operator=(const CoordPropertiesWidget&) = delete;

	// save and load the dock widget's settings
	boost::property_tree::ptree Save() const;
	bool Load(const boost::property_tree::ptree& prop);


public slots:
	void SetCoordinates(t_real h, t_real k, t_real l, t_real ki, t_real kf);
	void SetKfConstMode(bool kf_const = true);


signals:
	void CoordinatesChanged(t_real h, t_real k, t_real l, t_real ki, t_real kf);
	void GotoCoordinates(t_real h, t_real k, t_real l,
		t_real ki, t_real kf, bool only_set_target);

	// TODO: maybe move this to TASProperties
	void KfConstModeChanged(bool kf_const);


private:
	// number of coordinate elements
	static constexpr std::size_t m_num_coord_elems = 6;

	// (h, k, l, ki, kf, E) coordinates
	QDoubleSpinBox *m_spinCoords[m_num_coord_elems]
		{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	QCheckBox *m_checkKfFixed{nullptr};
};



class CoordPropertiesDockWidget : public QDockWidget
{
public:
	CoordPropertiesDockWidget(QWidget *parent=nullptr);
	virtual ~CoordPropertiesDockWidget();

	CoordPropertiesDockWidget(const CoordPropertiesDockWidget&) = delete;
	const CoordPropertiesDockWidget& operator=(const CoordPropertiesDockWidget&) = delete;

	std::shared_ptr<CoordPropertiesWidget> GetWidget()
	{ return m_widget; }


private:
    std::shared_ptr<CoordPropertiesWidget> m_widget;
};


#endif
