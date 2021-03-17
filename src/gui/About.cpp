/**
 * TAS paths tool
 * @author Tobias Weber <tweber@ill.fr>
 * @date mar-2021
 * @license GPLv3, see 'LICENSE' file
 */

#include "About.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QDialogButtonBox>

#include <boost/config.hpp>


AboutDlg::AboutDlg(QWidget* parent) : QDialog{parent}
{
    setWindowTitle("About");

    auto grid = new QGridLayout(this);
    grid->setSpacing(4);
    grid->setContentsMargins(16, 16, 16, 16);

    int y = 0;

    QLabel *labTitle = new QLabel("TAS Path Optimisation Tool", this);
    QFont fontTitle = labTitle->font();
    fontTitle.setPointSize(fontTitle.pointSize()*1.5);
    fontTitle.setWeight(QFont::Bold);
    labTitle->setFont(fontTitle);
    grid->addWidget(labTitle, y++,0,1,2);

    QSpacerItem *spacer1 = new QSpacerItem(1, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
    grid->addItem(spacer1, y++,0,1,2);

    QLabel *labAuthor1 = new QLabel("Author: ", this);
    QFont fontLabel1 = labAuthor1->font();
    fontLabel1.setWeight(QFont::Bold);
    labAuthor1->setFont(fontLabel1);
    grid->addWidget(labAuthor1, y,0,1,1);
    QLabel *labAuthor2 = new QLabel("Tobias Weber <tweber@ill.fr>.", this);
    grid->addWidget(labAuthor2, y++,1,1,1);

    QLabel *labDate1 = new QLabel("Date: ", this);
    labDate1->setFont(fontLabel1);
    grid->addWidget(labDate1, y,0,1,1);
    QLabel *labDate2 = new QLabel("February 2021 - March 2021.", this);
    grid->addWidget(labDate2, y++,1,1,1);

    QSpacerItem *spacer2 = new QSpacerItem(1, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
    grid->addItem(spacer2, y++,0,1,2);

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

    QSpacerItem *spacer3 = new QSpacerItem(1, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
    grid->addItem(spacer3, y++,0,1,2);

    QSpacerItem *spacer4 = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
    grid->addItem(spacer4, y++,0,1,2);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    buttons->setStandardButtons(QDialogButtonBox::Ok);
    grid->addWidget(buttons, y++,0,1,2);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}


AboutDlg::~AboutDlg()
{
}
