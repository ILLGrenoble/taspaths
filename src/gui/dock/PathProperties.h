/**
 * path properties dock widget
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

#ifndef __PATH_PROP_WIDGET_H__
#define __PATH_PROP_WIDGET_H__

#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QTimer>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSlider>

#include "src/core/types.h"


class PathPropertiesWidget : public QWidget
{Q_OBJECT
public:
	PathPropertiesWidget(QWidget *parent=nullptr);
	virtual ~PathPropertiesWidget();

	PathPropertiesWidget(const PathPropertiesWidget&) = delete;
	const PathPropertiesWidget& operator=(const PathPropertiesWidget&) = delete;

	// save and load the dock widget's settings
	boost::property_tree::ptree Save() const;
	bool Load(const boost::property_tree::ptree& prop);


private:
	// number of coordinate elements
	static constexpr std::size_t m_num_coord_elems = 2;

	// path target (a2, a4) coordinates
	QDoubleSpinBox *m_spinFinish[m_num_coord_elems]{nullptr, nullptr};
	QSlider *m_sliderPath = nullptr;
	QPushButton *m_btnCalcMesh = nullptr;
	//QPushButton *m_btnCalcPath = nullptr;
	QToolButton *m_btnGo = nullptr;

	QPalette m_paletteBtnNormal{}, m_paletteBtnFlash{};

	QTimer m_pathTrackTimer{};
	//QTimer m_meshButtonFlashTimer{};


protected:
	void SetGoButtonText(bool start=true);
	void TrackerTick();
	//void MeshButtonFlashTick();


public slots:
	void SetTarget(t_real a2, t_real a4);
	void PathMeshValid(bool valid);
	void PathMeshCalculation(CalculationState state, t_real progress);
	void PathAvailable(std::size_t numVertices);


signals:
	void TargetChanged(t_real a2, t_real a4);
	void GotoAngles(t_real a2, t_real a4);
	void CalculatePathMesh();
	//void CalculatePath();
	void TrackPath(std::size_t);
};



class PathPropertiesDockWidget : public QDockWidget
{
public:
	PathPropertiesDockWidget(QWidget *parent=nullptr);
	virtual ~PathPropertiesDockWidget();

	PathPropertiesDockWidget(const PathPropertiesDockWidget&) = delete;
	const PathPropertiesDockWidget& operator=(const PathPropertiesDockWidget&) = delete;

	std::shared_ptr<PathPropertiesWidget> GetWidget() { return m_widget; }


private:
    std::shared_ptr<PathPropertiesWidget> m_widget;
};


#endif
