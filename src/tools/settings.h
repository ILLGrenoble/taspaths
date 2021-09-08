/**
 * settings dialog
 * @author Tobias Weber <tweber@ill.fr>
 * @date aug-2021
 * @license GPLv3, see 'LICENSE' file
 */

#ifndef __TASPATHS_TOOLS_SETTINGS__
#define __TASPATHS_TOOLS_SETTINGS__

#include <QtCore/QSettings>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>

#include <string>
#include "tlibs2/libs/qt/gl.h"
#include "src/core/types.h"



// ----------------------------------------------------------------------------
// global settings variables
// ----------------------------------------------------------------------------
// gui theme
extern QString g_theme;

// gui font
extern QString g_font;
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// settings dialog
// ----------------------------------------------------------------------------
class GeoSettingsDlg : public QDialog
{
public:
	GeoSettingsDlg(QWidget* parent = nullptr, QSettings *sett = nullptr);
	virtual ~GeoSettingsDlg();

	GeoSettingsDlg(const GeoSettingsDlg&) = delete;
	const GeoSettingsDlg& operator=(const GeoSettingsDlg&) = delete;

	static void ReadSettings(QSettings* sett);


protected:
	virtual void accept() override;

	void ApplySettings();
	static void ApplyGuiSettings();


private:
	QSettings *m_sett{nullptr};

	QComboBox *m_comboTheme{nullptr};
	QLineEdit *m_editFont{nullptr};
};

#endif
// ----------------------------------------------------------------------------
