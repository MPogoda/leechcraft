#include <limits>
#include <list>
#include <QMainWindow>
#include <QTreeWidgetItem>
#include <QMessageBox>
#include <QtDebug>
#include <QTimer>
#include <QNetworkProxy>
#include <QApplication>
#include <QAction>
#include <QKeyEvent>
#include <QClipboard>
#include <QDir>
#include <QLocalServer>
#include <QTextCodec>
#include <QDesktopServices>
#include <QFileDialog>
#include <plugininterface/util.h>
#include "mainwindow.h"
#include "pluginmanager.h"
#include "core.h"
#include "xmlsettingsmanager.h"
#include "mergemodel.h"
#include "filtermodel.h"
#include "historymodel.h"

using namespace LeechCraft;
using namespace LeechCraft::Util;

Core::Core ()
: Server_ (new QLocalServer)
, MergeModel_ (new MergeModel (QStringList (tr ("Name"))
			<< tr ("State")
			<< tr ("Progress")
			<< tr ("Speed")))
, HistoryMergeModel_ (new MergeModel (QStringList (tr ("Filename"))
			<< tr ("Path")
			<< tr ("Size")
			<< tr ("Date")
			<< tr ("Tags")))
, FilterModel_ (new FilterModel)
, HistoryFilterModel_ (new FilterModel)
{
	PluginManager_ = new PluginManager (this);
	connect (PluginManager_,
			SIGNAL (loadProgress (const QString&)),
			this,
			SIGNAL (loadProgress (const QString&)));

	FilterModel_->setSourceModel (MergeModel_.get ());
	HistoryFilterModel_->setSourceModel (HistoryMergeModel_.get ());

    ClipboardWatchdog_ = new QTimer (this);
    connect (ClipboardWatchdog_,
			SIGNAL (timeout ()),
			this,
			SLOT (handleClipboardTimer ()));
    ClipboardWatchdog_->start (2000);

	Server_->listen ("LeechCraft local socket");

	XmlSettingsManager::Instance ()->RegisterObject ("ProxyEnabled",
			this, "handleProxySettings");
	XmlSettingsManager::Instance ()->RegisterObject ("ProxyHost",
			this, "handleProxySettings");
	XmlSettingsManager::Instance ()->RegisterObject ("ProxyPort",
			this, "handleProxySettings");
	XmlSettingsManager::Instance ()->RegisterObject ("ProxyLogin",
			this, "handleProxySettings");
	XmlSettingsManager::Instance ()->RegisterObject ("ProxyPassword",
			this, "handleProxySettings");
	XmlSettingsManager::Instance ()->RegisterObject ("ProxyType",
			this, "handleProxySettings");

	handleProxySettings ();
}

Core::~Core ()
{
}

Core& Core::Instance ()
{
	static Core core;
	return core;
}

void Core::Release ()
{
	XmlSettingsManager::Instance ()->setProperty ("FirstStart", "false");
	MergeModel_.reset ();
	HistoryMergeModel_.reset ();

    PluginManager_->Release ();
    delete PluginManager_;
    ClipboardWatchdog_->stop ();
    delete ClipboardWatchdog_;

	Server_.reset ();
}

void Core::SetReallyMainWindow (MainWindow *win)
{
    ReallyMainWindow_ = win;
	ReallyMainWindow_->GetTabWidget ()->installEventFilter (this);
}

QObjectList Core::GetSettables () const
{
	return PluginManager_->GetAllCastableTo<IHaveSettings*> ();
}

QAbstractItemModel* Core::GetPluginsModel () const
{
	return PluginManager_;
}

QAbstractProxyModel* Core::GetTasksModel () const
{
	return FilterModel_.get ();
}

QAbstractProxyModel* Core::GetHistoryModel () const
{
	return HistoryFilterModel_.get ();
}

MergeModel* Core::GetUnfilteredTasksModel () const
{
	return MergeModel_.get ();
}

MergeModel* Core::GetUnfilteredHistoryModel () const
{
	return HistoryMergeModel_.get ();
}

QWidget* Core::GetControls (const QModelIndex& index) const
{
	QAbstractItemModel *model = *MergeModel_->
		GetModelForRow (FilterModel_->mapToSource (index).row ());
	IJobHolder *ijh =
		qobject_cast<IJobHolder*> (Representation2Object_ [model]);

	return ijh->GetControls ();
}

QWidget* Core::GetAdditionalInfo (const QModelIndex& index) const
{
	QAbstractItemModel *model = *MergeModel_->
		GetModelForRow (FilterModel_->mapToSource (index).row ());
	IJobHolder *ijh =
		qobject_cast<IJobHolder*> (Representation2Object_ [model]);

	return ijh->GetAdditionalInfo ();
}

QStringList Core::GetTagsForIndex (int index,
		QAbstractItemModel *model) const
{
	MergeModel::const_iterator modIter =
		dynamic_cast<MergeModel*> (model)->GetModelForRow (index);

	int starting = dynamic_cast<MergeModel*> (model)->
		GetStartingRow (modIter);

	return (*modIter)->
		data ((*modIter)->index (index - starting, 0), LeechCraft::TagsRole)
		.toStringList ();
}

void Core::DelayedInit ()
{
	connect (this,
			SIGNAL (error (QString)),
			ReallyMainWindow_,
			SLOT (catchError (QString)));

	TabContainer_.reset (new TabContainer (ReallyMainWindow_->GetTabWidget ()));
	XmlSettingsManager::Instance ()->RegisterObject ("ShowTabNames",
			TabContainer_.get (), "handleTabNames");

	emit loadProgress (tr ("Preinitialization..."));
    PluginManager_->InitializePlugins ();
	emit loadProgress (tr ("Calculation dependencies..."));
    PluginManager_->CalculateDependencies ();
    QObjectList plugins = PluginManager_->GetAllPlugins ();
    foreach (QObject *plugin, plugins)
    {
        IInfo *info = qobject_cast<IInfo*> (plugin);
		emit loadProgress (tr ("Setting up %1...").arg (info->GetName ()));

		IJobHolder *ijh = qobject_cast<IJobHolder*> (plugin);
		IEmbedTab *iet = qobject_cast<IEmbedTab*> (plugin);
		IMultiTabs *imt = qobject_cast<IMultiTabs*> (plugin);

		const QMetaObject *qmo = plugin->metaObject ();

		if (qmo->indexOfSignal (QMetaObject::
					normalizedSignature ("gotEntity (const "
						"QByteArray&)").constData ()) != -1)
			connect (plugin,
					SIGNAL (gotEntity (const QByteArray&)),
					this,
					SLOT (handleGotEntity (const QByteArray&)));
		if (qmo->indexOfSignal (QMetaObject::
					normalizedSignature ("downloadFinished (const "
						"QString&)").constData ()) != -1)
			connect (plugin,
					SIGNAL (downloadFinished (const QString&)),
					this,
					SIGNAL (downloadFinished (const QString&)));
		if (qmo->indexOfSignal (QMetaObject::
					normalizedSignature ("log (const "
						"QString&)").constData ()) != -1)
			connect (plugin,
					SIGNAL (log (const QString&)),
					this,
					SLOT (handleLog (const QString&)));

		if (ijh && ijh->GetControls ())
			InitJobHolder (plugin);

		if (iet)
			InitEmbedTab (plugin);

		if (imt)
			InitMultiTab (plugin);
	}

	TabContainer_->handleTabNames ();
}

bool Core::ShowPlugin (int id)
{
    QObject *plugin = PluginManager_->data (PluginManager_->index (id, 0)).value <QObject*> ();
    IWindow *w = qobject_cast<IWindow*> (plugin);
    if (w)
    {
        w->ShowWindow ();
        return true;
    }
    else
        return false;
}

void Core::TryToAddJob (const QString& name, const QString& where)
{
    QObjectList plugins = PluginManager_->GetAllPlugins ();
    foreach (QObject *plugin, plugins)
    {
        IDownload *di = qobject_cast<IDownload*> (plugin);
		TaskParameters tp = FromCommonDialog | Autostart;
        if (di && di->CouldDownload (name.toUtf8 (), tp))
        {
			DownloadParams ddp =
			{
				name.toUtf8 (),
				where
			};
			di->AddJob (ddp, tp);
			return;
        }
    }
    emit error (tr ("No plugins are able to download \"%1\"").arg (name));
}

void Core::Activated (const QModelIndex& index)
{
	ShowPlugin (index.row ());
}

void Core::SetNewRow (const QModelIndex& index)
{
	QModelIndex mapped = FilterModel_->mapToSource (index);
	MergeModel::const_iterator modIter = MergeModel_->GetModelForRow (mapped.row ());
	QObject *plugin = Representation2Object_ [*modIter];

	IJobHolder *ijh = qobject_cast<IJobHolder*> (plugin);
	if (ijh)
		ijh->ItemSelected (MergeModel_->mapToSource (mapped));

	QObjectList watchers = PluginManager_->GetSelectedDownloaderWatchers ();
	foreach (QObject *pEntity, watchers)
		QMetaObject::invokeMethod (pEntity,
				"selectedDownloaderChanged",
				Q_ARG (QObject*, plugin));
}

bool Core::SameModel (const QModelIndex& i1, const QModelIndex& i2) const
{
	QModelIndex mapped1 = FilterModel_->mapToSource (i1);
	MergeModel::const_iterator modIter1 = MergeModel_->GetModelForRow (mapped1.row ());

	QModelIndex mapped2 = FilterModel_->mapToSource (i2);
	MergeModel::const_iterator modIter2 = MergeModel_->GetModelForRow (mapped2.row ());

	return modIter1 == modIter2;
}

void Core::UpdateFiltering (const QString& text,
		Core::FilterType ft, bool caseSensitive, bool history)
{
	if (!history)
		FilterModel_->setFilterCaseSensitivity (caseSensitive ?
				Qt::CaseSensitive : Qt::CaseInsensitive);
	else
		HistoryFilterModel_->setFilterCaseSensitivity (caseSensitive ?
				Qt::CaseSensitive : Qt::CaseInsensitive);

	switch (ft)
	{
		case FTFixedString:
			if (!history)
			{
				FilterModel_->SetTagsMode (false);
				FilterModel_->setFilterFixedString (text);
			}
			else
			{
				HistoryFilterModel_->SetTagsMode (false);
				HistoryFilterModel_->setFilterFixedString (text);
			}
			break;
		case FTWildcard:
			if (!history)
			{
				FilterModel_->SetTagsMode (false);
				FilterModel_->setFilterWildcard (text);
			}
			else
			{
				HistoryFilterModel_->SetTagsMode (false);
				HistoryFilterModel_->setFilterWildcard (text);
			}
			break;
		case FTRegexp:
			if (!history)
			{
				FilterModel_->SetTagsMode (false);
				FilterModel_->setFilterRegExp (text);
			}
			else
			{
				HistoryFilterModel_->SetTagsMode (false);
				HistoryFilterModel_->setFilterRegExp (text);
			}
			break;
		case FTTags:
			if (!history)
			{
				FilterModel_->SetTagsMode (true);
				FilterModel_->setFilterFixedString (text);
			}
			else
			{
				HistoryFilterModel_->SetTagsMode (true);
				HistoryFilterModel_->setFilterFixedString (text);
			}
			break;
	}
}

void Core::HistoryActivated (int historyRow)
{
	QString name = HistoryFilterModel_->index (historyRow, 0).data ().toString ();
	QString path = HistoryFilterModel_->index (historyRow, 1).data ().toString ();

	QFileInfo pathInfo (path);
	QString file;
	if (pathInfo.isDir ())
	{
		QDir dir (path);
		if (!dir.exists (name))
			return;
		file = dir.filePath (name);
	}
	else if (pathInfo.isFile ())
		file = path;
	else
		return;

	QObject *videoProvider = PluginManager_->GetProvider ("media");
	if (!videoProvider)
		return;

	QMetaObject::invokeMethod (videoProvider, "setFile", Q_ARG (QString, file));
	QMetaObject::invokeMethod (videoProvider, "play");
}

QPair<qint64, qint64> Core::GetSpeeds () const
{
    qint64 download = 0;
    qint64 upload = 0;
    QObjectList plugins = PluginManager_->GetAllPlugins ();
    foreach (QObject *plugin, plugins)
    {
        IDownload *di = qobject_cast<IDownload*> (plugin);
        if (di)
        {
            download += di->GetDownloadSpeed ();
            upload += di->GetUploadSpeed ();
        }
    }

    return QPair<qint64, qint64> (download, upload);
}

int Core::CountUnremoveableTabs () const
{
	// + 2 because of tabs with downloaders and history
	return PluginManager_->GetAllCastableTo<IEmbedTab*> ().size () + 2;
}

bool Core::eventFilter (QObject *watched, QEvent *e)
{
	if (ReallyMainWindow_ &&
			watched == ReallyMainWindow_->GetTabWidget ())
	{
		if (e->type () == QEvent::KeyRelease)
		{
			QKeyEvent *key = static_cast<QKeyEvent*> (e);

			if (key->modifiers () & Qt::ControlModifier)
			{
				if (key->key () == Qt::Key_W)
				{
					if (TabContainer_->RemoveCurrent ())
						return true;
				}
				else if (key->key () == Qt::Key_BracketLeft)
				{
					TabContainer_->RotateLeft ();
					return true;
				}
				else if (key->key () == Qt::Key_BracketRight)
				{
					TabContainer_->RotateRight ();
					return true;
				}
			}
		}
	}
	return QObject::eventFilter (watched, e);
}

void Core::handleProxySettings () const
{
	bool enabled = XmlSettingsManager::Instance ()->property ("ProxyEnabled").toBool ();
	QNetworkProxy pr;
	if (enabled)
	{
		pr.setHostName (XmlSettingsManager::Instance ()->property ("ProxyHost").toString ());
		pr.setPort (XmlSettingsManager::Instance ()->property ("ProxyPort").toInt ());
		pr.setUser (XmlSettingsManager::Instance ()->property ("ProxyLogin").toString ());
		pr.setPassword (XmlSettingsManager::Instance ()->property ("ProxyPassword").toString ());
		QString type = XmlSettingsManager::Instance ()->property ("ProxyType").toString ();
		QNetworkProxy::ProxyType pt = QNetworkProxy::HttpProxy;
		if (type == "socks5")
			pt = QNetworkProxy::Socks5Proxy;
		else if (type == "tphttp")
			pt = QNetworkProxy::HttpProxy;
		else if (type == "chttp")
			pr = QNetworkProxy::HttpCachingProxy;
		else if (type == "cftp")
			pr = QNetworkProxy::FtpCachingProxy;
		pr.setType (pt);
	}
	else
		pr.setType (QNetworkProxy::NoProxy);
	QNetworkProxy::setApplicationProxy (pr);
}

namespace
{
	void PassSelectionsByOne (QObject *object,
			const QModelIndexList& selected,
			int startingRow,
			int endingRow,
			const QByteArray& function)
	{
		for (QModelIndexList::const_iterator i = selected.begin (),
				end = selected.end (); i != end; ++i)
		{
			if (i->row () < startingRow)
				continue;
			if (i->row () >= endingRow)
				break;

			QMetaObject::invokeMethod (object,
					function,
					Q_ARG (int, i->row ()));
		}
	}
};

void Core::handlePluginAction ()
{
	QAction *source = qobject_cast<QAction*> (sender ());
	QString slot = source->property ("Slot").toString ();
	QString signal = source->property ("Signal").toString ();
	QVariant varWhole = source->property ("WholeSelection");
	bool whole = false;
	if (varWhole.isValid ())
		whole = varWhole.toBool ();

	if (slot.isEmpty () && signal.isEmpty ())
		return;

	QObject *object = source->property ("Object").value<QObject*> ();

	QModelIndexList origSelection = ReallyMainWindow_->GetSelectedRows ();
	QModelIndexList selected;
	for (QModelIndexList::const_iterator i = origSelection.begin (),
			end = origSelection.end (); i != end; ++i)
		selected.push_back (MapToSource (*i));

	QAbstractItemModel *model = Action2Model_ [source];
	int startingRow = MergeModel_->GetStartingRow (MergeModel_->FindModel (model));
	int endingRow = startingRow + model->rowCount ();
	if (whole)
	{
		std::deque<int> selections;
		for (QModelIndexList::const_iterator i = selected.begin (),
				end = selected.end (); i != end; ++i)
		{
			if (i->row () < startingRow)
				continue;
			if (i->row () >= endingRow)
				break;
			selections.push_back (i->row ());
		}

		if (!slot.isEmpty ())
			QMetaObject::invokeMethod (object,
					slot.toLatin1 (),
					Q_ARG (std::deque<int>, selections));

		if (!signal.isEmpty ())
			QMetaObject::invokeMethod (object,
					signal.toLatin1 (),
					Q_ARG (std::deque<int>, selections));
	}
	else
	{
		if (!slot.isEmpty ())
			PassSelectionsByOne (object, selected, startingRow, endingRow, slot.toLatin1 ());
		if (!signal.isEmpty ())
			PassSelectionsByOne (object, selected, startingRow, endingRow, signal.toLatin1 ());
	}
}

void Core::toggleMultiwindow ()
{
	TabContainer_->ToggleMultiwindow ();
}

void Core::deleteSelectedHistory (const QModelIndex& index)
{
	QModelIndex mapped = FilterModel_->mapToSource (index);
	HistoryModel *model =
		static_cast<HistoryModel*> (*HistoryMergeModel_->
				GetModelForRow (mapped.row ()));
	model->RemoveItem (HistoryMergeModel_->mapToSource (mapped));
}

void Core::handleGotEntity (const QByteArray& file, bool fromBuffer)
{
    if (!fromBuffer &&
			!XmlSettingsManager::Instance ()->
			property ("QueryPluginsToHandleFinished").toBool ())
        return;

    QObjectList plugins = PluginManager_->GetAllCastableTo<IDownload*> ();
    for (int i = 0; i < plugins.size (); ++i)
    {
        IDownload *id = qobject_cast<IDownload*> (plugins.at (i));
        IInfo *ii = qobject_cast<IInfo*> (plugins.at (i));
		TaskParameters tp = Autostart | FromAutomatic;
		if (fromBuffer)
			tp |= FromClipboard;
        if (id->CouldDownload (file, tp))
        {
			QString string = QTextCodec::codecForName ("UTF-8")->
				toUnicode (file);
			QString question = tr ("%1 could be handled by plugin %2, "
					"would you like to?")
				.arg (string)
				.arg (ii->GetName ());

			QMessageBox::StandardButton b = QMessageBox::question (0,
						tr ("Question"),
						question,
						QMessageBox::Yes |
						QMessageBox::No |
						QMessageBox::NoToAll);
			if (b == QMessageBox::No)
				continue;
			else if (b == QMessageBox::NoToAll)
				break;

			QString dir = QFileDialog::getExistingDirectory (0,
					tr ("Select save path"),
					XmlSettingsManager::Instance ()->
						Property ("EntitySavePath",
							QDesktopServices::storageLocation (QDesktopServices::DocumentsLocation))
						.toString ());
			
			if (dir.isEmpty ())
				break;

			XmlSettingsManager::Instance ()->
				setProperty ("EntitySavePath", dir);

			DownloadParams ddp =
			{
				file,
				dir
			};
			id->AddJob (ddp, tp);
        }
    }
}

void Core::handleClipboardTimer ()
{
    QString text = QApplication::clipboard ()->text ();
    if (text.isEmpty () || text == PreviousClipboardContents_)
        return;

    PreviousClipboardContents_ = text;

    if (XmlSettingsManager::Instance ()->property ("WatchClipboard").toBool ())
        handleGotEntity (text.toUtf8 (), true);
}

void Core::embeddedTabWantsToFront ()
{
	IEmbedTab *iet = qobject_cast<IEmbedTab*> (sender ());
	if (!iet)
		return;

	ReallyMainWindow_->show ();
	TabContainer_->bringToFront (iet->GetTabContents ());
}

void Core::handleStatusBarChanged (QWidget *contents, const QString& msg)
{
	if (contents->visibleRegion ().isEmpty ())
		return;

	ReallyMainWindow_->statusBar ()->showMessage (msg, 30000);
}

void Core::handleLog (const QString& message)
{
	IInfo *ii = qobject_cast<IInfo*> (sender ());
	emit log (ii->GetName () + ": " + message);
}

QModelIndex Core::MapToSource (const QModelIndex& index) const
{
	return MergeModel_->mapToSource (FilterModel_->mapToSource (index));
}

void Core::InitJobHolder (QObject *plugin)
{
	IJobHolder *ijh = qobject_cast<IJobHolder*> (plugin);
	QAbstractItemModel *model = ijh->GetRepresentation ();
	Representation2Object_ [model] = plugin;
	MergeModel_->AddModel (model);

	QList<QAction*> actions = ijh->GetControls ()->actions ();
	for (QList<QAction*>::iterator i = actions.begin (),
			end = actions.end (); i != end; ++i)
	{
		connect (*i,
				SIGNAL (triggered ()),
				this,
				SLOT (handlePluginAction ()));
		Action2Model_ [*i] = model;
	}

	ijh->GetControls ()->setParent (ReallyMainWindow_);
	if (ijh->GetAdditionalInfo ())
		ijh->GetAdditionalInfo ()->setParent (ReallyMainWindow_);

	QAbstractItemModel *historyModel = ijh->GetHistory ();
	if (historyModel)
	{
		History2Object_ [historyModel] = plugin;
		HistoryMergeModel_->AddModel (historyModel);
	}
}

void Core::InitEmbedTab (QObject *plugin)
{
	IInfo *ii = qobject_cast<IInfo*> (plugin);
	IEmbedTab *iet = qobject_cast<IEmbedTab*> (plugin);
	TabContainer_->add (ii->GetName (),
			iet->GetTabContents (),
			ii->GetIcon ());
	connect (plugin,
			SIGNAL (bringToFront ()),
			this,
			SLOT (embeddedTabWantsToFront ()));
}

void Core::InitMultiTab (QObject *plugin)
{
	connect (plugin,
			SIGNAL (addNewTab (const QString&, QWidget*)),
			TabContainer_.get (),
			SLOT (add (const QString&, QWidget*)));
	connect (plugin,
			SIGNAL (removeTab (QWidget*)),
			TabContainer_.get (),
			SLOT (remove (QWidget*)));
	connect (plugin,
			SIGNAL (changeTabName (QWidget*, const QString&)),
			TabContainer_.get (),
			SLOT (changeTabName (QWidget*, const QString&)));
	connect (plugin,
			SIGNAL (changeTabIcon (QWidget*, const QIcon&)),
			TabContainer_.get (),
			SLOT (changeTabIcon (QWidget*, const QIcon&)));
	connect (plugin,
			SIGNAL (statusBarChanged (QWidget*, const QString&)),
			this,
			SLOT (handleStatusBarChanged (QWidget*, const QString&)));
	connect (plugin,
			SIGNAL (raiseTab (QWidget*)),
			TabContainer_.get (),
			SLOT (bringToFront (QWidget*)));
}

