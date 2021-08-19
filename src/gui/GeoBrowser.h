/**
 * geometries browser
 * @author Tobias Weber <tweber@ill.fr>
 * @date jun-2021
 * @license GPLv3, see 'LICENSE' file
 */

#ifndef __TAKIN_PATHS_GEOBROWSER_H__
#define __TAKIN_PATHS_GEOBROWSER_H__


#include <QtCore/QSettings>
#include <QtWidgets/QDialog>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QMenu>

#include "src/core/InstrumentSpace.h"


class GeometriesBrowser : public QDialog
{ Q_OBJECT
public:
	GeometriesBrowser(QWidget* parent = nullptr, QSettings *sett = nullptr);
	virtual ~GeometriesBrowser();

	void UpdateGeoTree(const InstrumentSpace& instrspace);
	void SelectObject(const std::string& obj);


protected:
	virtual void accept() override;

	void ShowGeoTreeContextMenu(const QPoint& pt);
	void RenameCurrentGeoTreeObject();
	void DeleteCurrentGeoTreeObject();
	void GeoTreeItemChanged(QTreeWidgetItem *item, int col);
	void GeoTreeCurrentItemChanged(QTreeWidgetItem *item, QTreeWidgetItem *previtem);


private:
	const InstrumentSpace* m_instrspace{nullptr};
	QSettings *m_sett{nullptr};

	QTreeWidget *m_geotree{nullptr};
	QTableWidget *m_geosettings{nullptr};
	QSplitter *m_splitter{nullptr};

	QMenu *m_contextMenuGeoTree{nullptr};
	QTreeWidgetItem *m_curContextItem{nullptr};


signals:
	void SignalDeleteObject(const std::string& id);
	void SignalRenameObject(const std::string& oldId, const std::string& newId);
};


#endif
