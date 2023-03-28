/**
 * TAS paths tool
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

#include "About.h"
#include "src/core/types.h"
#include "src/gui/settings_variables.h"

#include <QtGui/QIcon>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QDialogButtonBox>

#include <boost/config.hpp>
#include <boost/version.hpp>


/**
 * constructor
 */
AboutDlg::AboutDlg(QWidget* parent, QSettings *sett)
	: QDialog{parent}, m_sett{sett}
{
	setWindowTitle("About " TASPATHS_TITLE);
	setSizeGripEnabled(true);

	// restore dialog geometry
	if(m_sett && m_sett->contains("about/geo"))
		restoreGeometry(m_sett->value("about/geo").toByteArray());

	auto grid = new QGridLayout(this);
	grid->setSpacing(4);
	grid->setContentsMargins(12, 12, 12, 12);

	int y = 0;

	// icon and title
	QLabel *labTitle = new QLabel(TASPATHS_TITLE, this);
	QFont fontTitle = labTitle->font();
	fontTitle.setPointSize(fontTitle.pointSize()*1.5);
	fontTitle.setWeight(QFont::Bold);
	labTitle->setFont(fontTitle);

	QWidget *titleWidget = new QWidget(this);
	QGridLayout *titleGrid = new QGridLayout(titleWidget);
	titleGrid->setSpacing(4);
	titleGrid->setContentsMargins(0, 0, 0, 0);
	titleWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QLabel *labelIcon = new QLabel(titleWidget);
	std::string icon_file = g_res.FindFile("taspaths.png");
	if(icon_file == "")
		icon_file = g_res.FindFile("taspaths.svg");
	QIcon icon{icon_file.c_str()};
	QPixmap pixmap = icon.pixmap(48, 48);
	labelIcon->setPixmap(pixmap);
	labelIcon->setFrameShape(QFrame::StyledPanel);
	labelIcon->setFrameShadow(QFrame::Raised);
	labelIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	QSpacerItem *spacerIconTitle = new QSpacerItem(16, 1, QSizePolicy::Fixed, QSizePolicy::Fixed);
	titleGrid->addWidget(labelIcon, 0, 0, 1, 1, Qt::AlignVCenter);
	titleGrid->addItem(spacerIconTitle, 0, 1, 1, 1);
	titleGrid->addWidget(labTitle, 0, 2, 1, 1, Qt::AlignVCenter);

	grid->addWidget(titleWidget, y++, 0, 1, 2);

	QSpacerItem *spacerTitleSubtitle = new QSpacerItem(1, 4, QSizePolicy::Minimum, QSizePolicy::Fixed);
	grid->addItem(spacerTitleSubtitle, y++,0,1,2);

	// subtitle
	QLabel *labSubtitle = new QLabel("Pathfinding software for triple-axis spectrometers.", this);
	QFont fontSubtitle = labSubtitle->font();
	fontSubtitle.setWeight(QFont::Bold);
	labSubtitle->setFont(fontSubtitle);
	grid->addWidget(labSubtitle, y++,0,1,2);

	QSpacerItem *spacerBelowSubtitle = new QSpacerItem(1, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);
	grid->addItem(spacerBelowSubtitle, y++,0,1,2);

	QSpacerItem *spacerBelowSubtitle2 = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
	grid->addItem(spacerBelowSubtitle2, y++,0,1,2);

	QLabel *labDOI1 = new QLabel("DOI: ", this);
	QFont fontLabel1 = labDOI1->font();
	fontLabel1.setWeight(QFont::Bold);
	labDOI1->setFont(fontLabel1);
	grid->addWidget(labDOI1, y,0,1,1);
	QLabel *labDOI2 = new QLabel(
		"<a href=\"https://doi.org/10.5281/zenodo.4625649\">"
		"10.5281/zenodo.4625649</a>.",
		this);
	labDOI2->setOpenExternalLinks(1);
	grid->addWidget(labDOI2, y++,1,1,1);

	QLabel *labUrl1 = new QLabel("Repository: ", this);
	fontLabel1.setWeight(QFont::Bold);
	labUrl1->setFont(fontLabel1);
	grid->addWidget(labUrl1, y,0,1,1);
	QLabel *labUrl2 = new QLabel(
		"<a href=\"https://github.com/ILLGrenoble/taspaths\">https://github.com/ILLGrenoble/taspaths</a>.",
		this);
	labUrl2->setOpenExternalLinks(1);
	grid->addWidget(labUrl2, y++,1,1,1);

	QLabel *labVersion1 = new QLabel("Version: ", this);
	fontLabel1.setWeight(QFont::Bold);
	labVersion1->setFont(fontLabel1);
	grid->addWidget(labVersion1, y,0,1,1);
	QLabel *labVersion2 = new QLabel(TASPATHS_VERSION ".", this);
	grid->addWidget(labVersion2, y++,1,1,1);

	QSpacerItem *spacerAfterVersion = new QSpacerItem(1, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
	grid->addItem(spacerAfterVersion, y++,0,1,2);

	QLabel *labAuthor1 = new QLabel("Author: ", this);
	fontLabel1.setWeight(QFont::Bold);
	labAuthor1->setFont(fontLabel1);
	grid->addWidget(labAuthor1, y,0,1,1);
	QLabel *labAuthor2 = new QLabel("Tobias Weber <tweber@ill.fr>.", this);
	grid->addWidget(labAuthor2, y++,1,1,1);

	QLabel *labDate1 = new QLabel("Date: ", this);
	labDate1->setFont(fontLabel1);
	grid->addWidget(labDate1, y,0,1,1);
	QLabel *labDate2 = new QLabel("February 2021 - December 2021.", this);
	grid->addWidget(labDate2, y++,1,1,1);

	QLabel *labLic1 = new QLabel("License: ", this);
	fontLabel1.setWeight(QFont::Bold);
	labLic1->setFont(fontLabel1);
	grid->addWidget(labLic1, y,0,1,1);
	QLabel *labLic2 = new QLabel("GNU GPL Version 3.", this);
	grid->addWidget(labLic2, y++,1,1,1);

	QSpacerItem *spacerBeforeTimestamp = new QSpacerItem(1, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
	grid->addItem(spacerBeforeTimestamp, y++,0,1,2);

	QLabel *labBuildDate1 = new QLabel("Build Timestamp: ", this);
	labBuildDate1->setFont(fontLabel1);
	grid->addWidget(labBuildDate1, y,0,1,1);
	QString buildDate = QString{__DATE__} + QString{", "} + QString{__TIME__} + QString{"."};
	QLabel *labBuildDate2 = new QLabel(buildDate, this);
	grid->addWidget(labBuildDate2, y++,1,1,1);

	QLabel *labCompiler1 = new QLabel("Compiler: ", this);
	labCompiler1->setFont(fontLabel1);
	grid->addWidget(labCompiler1, y,0,1,1);
	QString compiler = QString{BOOST_COMPILER} + QString{"."};
	QLabel *labCompiler2 = new QLabel(compiler, this);
	grid->addWidget(labCompiler2, y++,1,1,1);

	QLabel *labCPPLib1 = new QLabel("C++ Library: ", this);
	labCPPLib1->setFont(fontLabel1);
	grid->addWidget(labCPPLib1, y,0,1,1);
	QString cpplib2 = QString{BOOST_STDLIB} + QString{"."};
	QLabel *labCPPLib = new QLabel(cpplib2, this);
	grid->addWidget(labCPPLib, y++,1,1,1);

	QLabel *labBoostLib1 = new QLabel("Boost Library: ", this);
	labBoostLib1->setFont(fontLabel1);
	grid->addWidget(labBoostLib1, y,0,1,1);
	QString boostlib2 = QString{"Version %1.%2.%3."}
		.arg(BOOST_VERSION / 100000)
		.arg((BOOST_VERSION % 100000) / 100)
		.arg(BOOST_VERSION % 100);
	QLabel *labBoostLib = new QLabel(boostlib2, this);
	grid->addWidget(labBoostLib, y++,1,1,1);

	QSpacerItem *spacerBeforeButtons = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
	grid->addItem(spacerBeforeButtons, y++,0,1,2);

	QDialogButtonBox *buttons = new QDialogButtonBox(this);
	buttons->setStandardButtons(QDialogButtonBox::Ok);
	grid->addWidget(buttons, y++,0,1,2);

	connect(buttons, &QDialogButtonBox::accepted, this, &AboutDlg::accept);
}


/**
 * copy constructor
 */
AboutDlg::AboutDlg(const AboutDlg& other) : QDialog(other.parentWidget())
{
	operator=(other);
}


/**
 * assignment operator
 */
const AboutDlg& AboutDlg::operator=(const AboutDlg&)
{
	m_sett = nullptr;
	return *this;
}


/**
 * destructor
 */
AboutDlg::~AboutDlg()
{
}


/**
 * 'OK' or 'Apply' button has been pressed
 */
void AboutDlg::accept()
{
	if(m_sett)
		m_sett->setValue("about/geo", saveGeometry());
	QDialog::accept();
}
