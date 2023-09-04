/**
 * crystal properties dock widget
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

#ifndef __XTAL_PROP_WIDGET_H__
#define __XTAL_PROP_WIDGET_H__

#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtWidgets/QWidget>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QPlainTextEdit>

#include "src/core/types.h"


class XtalPropertiesWidget : public QWidget
{Q_OBJECT
public:
	XtalPropertiesWidget(QWidget *parent=nullptr);
	virtual ~XtalPropertiesWidget();

	XtalPropertiesWidget(const XtalPropertiesWidget&) = delete;
	const XtalPropertiesWidget& operator=(const XtalPropertiesWidget&) = delete;

	// save and load the dock widget's settings
	boost::property_tree::ptree Save() const;
	bool Load(const boost::property_tree::ptree& prop);


public slots:
	void SetLattice(t_real a, t_real b, t_real c,
		t_real alpha, t_real beta, t_real gamma);
	void SetPlane(t_real vec1_x, t_real vec1_y, t_real vec1_z,
		t_real vec2_x, t_real vec2_y, t_real vec2_z);


signals:
	// angles are in rad
	void LatticeChanged(t_real a, t_real b, t_real c,
		t_real alpha, t_real beta, t_real gamma);
	void PlaneChanged(t_real vec1_x, t_real vec1_y, t_real vec1_z,
		t_real vec2_x, t_real vec2_y, t_real vec2_z);


private:
	// number of lattice constant elements
	static constexpr std::size_t m_num_lattice_elems = 3;

	// crystal lattice constants
	QDoubleSpinBox *m_spinLatticeConsts[m_num_lattice_elems]
		{nullptr, nullptr, nullptr};

	// crystal lattice angles
	QDoubleSpinBox *m_spinLatticeAngles[m_num_lattice_elems]
		{nullptr, nullptr, nullptr};

	// number of scattering plane elements
	static constexpr std::size_t m_num_plane_elems = 6;

	// scattering plane vectors
	QDoubleSpinBox *m_spinPlane[m_num_plane_elems]
		{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
};



class XtalPropertiesDockWidget : public QDockWidget
{
public:
	XtalPropertiesDockWidget(QWidget *parent=nullptr);
	virtual ~XtalPropertiesDockWidget();

	XtalPropertiesDockWidget(const XtalPropertiesDockWidget&) = delete;
	const XtalPropertiesDockWidget& operator=(const XtalPropertiesDockWidget&) = delete;

	std::shared_ptr<XtalPropertiesWidget> GetWidget() { return m_widget; }


private:
    std::shared_ptr<XtalPropertiesWidget> m_widget;
};



// ----------------------------------------------------------------------------



class XtalInfoWidget : public QWidget
{Q_OBJECT
public:
	XtalInfoWidget(QWidget *parent=nullptr);
	virtual ~XtalInfoWidget();

	XtalInfoWidget(const XtalInfoWidget&) = delete;
	const XtalInfoWidget& operator=(const XtalInfoWidget&) = delete;


public slots:
	void SetUB(const t_mat& matB, const t_mat& matUB);


private:
	QPlainTextEdit *m_txt{nullptr};
};



class XtalInfoDockWidget : public QDockWidget
{
public:
	XtalInfoDockWidget(QWidget *parent=nullptr);
	virtual ~XtalInfoDockWidget();

	XtalInfoDockWidget(const XtalInfoDockWidget&) = delete;
	const XtalInfoDockWidget& operator=(const XtalInfoDockWidget&) = delete;

	std::shared_ptr<XtalInfoWidget> GetWidget() { return m_widget; }


private:
    std::shared_ptr<XtalInfoWidget> m_widget;
};


#endif
