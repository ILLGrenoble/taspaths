/**
 * geo lib and tools
 * @author Tobias Weber <tweber@ill.fr>
 * @date nov-2020 - jun-2021
 * @license GPLv3, see 'LICENSE' file
 */

#ifndef __TAKIN_PATHS_TOOLS_ABOUT_H__
#define __TAKIN_PATHS_TOOLS_ABOUT_H__


#include <QtWidgets/QDialog>
#include <QtCore/QSettings>


class AboutDlg : public QDialog
{
public:
	AboutDlg(QWidget* parent=nullptr, QSettings *sett = nullptr);
	virtual ~AboutDlg();

protected:
	virtual void accept() override;

private:
	QSettings *m_sett{nullptr};
};


#endif
