/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "playertab.h"
#include <algorithm>
#include <functional>
#include <QToolBar>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QMenu>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QTabBar>
#include <QMessageBox>
#include <util/util.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/media/iaudioscrobbler.h>
#include <interfaces/media/isimilarartists.h>
#include <interfaces/media/ipendingsimilarartists.h>
#include <interfaces/media/ilyricsfinder.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/iiconthememanager.h>
#include "player.h"
#include "util.h"
#include "core.h"
#include "localcollection.h"
#include "xmlsettingsmanager.h"
#include "aalabeleventfilter.h"
#include "nowplayingpixmaphandler.h"
#include "previewhandler.h"
#include "engine/sourceobject.h"
#include "engine/output.h"
#include "volumeslider.h"
#include "seekslider.h"
#include "palettefixerfilter.h"

#ifdef ENABLE_MPRIS
#include "mpris/instance.h"
#endif

namespace LeechCraft
{
namespace LMP
{
	PlayerTab::PlayerTab (const TabClassInfo& info, QObject *plugin, QWidget *parent)
	: QWidget (parent)
	, Plugin_ (plugin)
	, TC_ (info)
	, Player_ (Core::Instance ().GetPlayer ())
	, PreviewHandler_ (Core::Instance ().GetPreviewHandler ())
	, TabToolbar_ (new QToolBar ())
	, PlayPause_ (0)
	, TrayMenu_ (new QMenu ("LMP", this))
	, NPPixmapHandler_ (new NowPlayingPixmapHandler (this))
	{
		Ui_.setupUi (this);
		Ui_.MainSplitter_->setStretchFactor (0, 2);
		Ui_.MainSplitter_->setStretchFactor (1, 1);
		Ui_.RadioWidget_->SetPlayer (Player_);

		NPPixmapHandler_->AddSetter ([this] (const QPixmap& px, const QString&) { Ui_.NPWidget_->SetAlbumArt (px); });
		NPPixmapHandler_->AddSetter ([this] (const QPixmap& px, const QString& path)
				{
					const QPixmap& scaled = px.scaled (Ui_.NPArt_->minimumSize (),
							Qt::KeepAspectRatio, Qt::SmoothTransformation);
					Ui_.NPArt_->setPixmap (scaled);
					Ui_.NPArt_->setProperty ("LMP/CoverPath", path);
				});

		SetupNavButtons ();

		Ui_.FSBrowser_->AssociatePlayer (Player_);

		auto coverGetter = [this] () { return Ui_.NPArt_->property ("LMP/CoverPath").toString (); };
		Ui_.NPArt_->installEventFilter (new AALabelEventFilter (coverGetter, this));

		connect (Player_,
				SIGNAL (playerAvailable (bool)),
				this,
				SLOT (handlePlayerAvailable (bool)));
		connect (Player_,
				SIGNAL (songChanged (MediaInfo)),
				this,
				SLOT (handleSongChanged (MediaInfo)));
		connect (Player_,
				SIGNAL (songInfoUpdated (MediaInfo)),
				this,
				SLOT (handleSongInfoUpdated (MediaInfo)));
		connect (Player_,
				SIGNAL (indexChanged (QModelIndex)),
				Ui_.Playlist_,
				SLOT (focusIndex (QModelIndex)));

		TrayIcon_ = new LMPSystemTrayIcon (QIcon ("lcicons:/lmp/resources/images/lmp.svg"), this);
		connect (Player_,
				SIGNAL (songChanged (const MediaInfo&)),
				TrayIcon_,
				SLOT (handleSongChanged (const MediaInfo&)));
		SetupToolbar ();
		Ui_.PLManagerWidget_->SetPlayer (Player_);

		Ui_.Playlist_->SetPlayer (Player_);

		XmlSettingsManager::Instance ().RegisterObject ("ShowTrayIcon",
				this, "handleShowTrayIcon");
		handleShowTrayIcon ();

		XmlSettingsManager::Instance ().RegisterObject ("UseNavTabBar",
				this, "handleUseNavTabBar");
		handleUseNavTabBar ();

		connect (Ui_.NPWidget_,
				SIGNAL (gotArtistImage (QString, QUrl)),
				NPPixmapHandler_,
				SLOT (handleGotArtistImage (QString, QUrl)));

		connect (Ui_.HypesWidget_,
				SIGNAL (artistPreviewRequested (QString)),
				PreviewHandler_,
				SLOT (previewArtist (QString)));
		connect (Ui_.HypesWidget_,
				SIGNAL (trackPreviewRequested (QString, QString)),
				PreviewHandler_,
				SLOT (previewTrack (QString, QString)));
		connect (Ui_.ReleasesWidget_,
				SIGNAL (previewRequested (QString, QString, int)),
				PreviewHandler_,
				SLOT (previewTrack (QString, QString, int)));

#ifdef ENABLE_MPRIS
		new MPRIS::Instance (this, Player_);
#endif
	}

	PlayerTab::~PlayerTab ()
	{
		delete NavBar_;
		delete NavButtons_;
	}

	TabClassInfo PlayerTab::GetTabClassInfo () const
	{
		return TC_;
	}

	QObject* PlayerTab::ParentMultiTabs ()
	{
		return Plugin_;
	}

	void PlayerTab::Remove ()
	{
		emit removeTab (this);
	}

	QToolBar* PlayerTab::GetToolBar () const
	{
		return TabToolbar_;
	}

	Player* PlayerTab::GetPlayer () const
	{
		return Player_;
	}

	QByteArray PlayerTab::GetTabRecoverData () const
	{
		return "playertab";
	}

	QIcon PlayerTab::GetTabRecoverIcon () const
	{
		return QIcon ("lcicons:/lmp/resources/images/lmp.svg");
	}

	QString PlayerTab::GetTabRecoverName () const
	{
		return "LMP";
	}

	void PlayerTab::InitWithOtherPlugins ()
	{
		handleSongChanged (MediaInfo ());
		Ui_.DevicesBrowser_->InitializeDevices ();
		Ui_.EventsWidget_->InitializeProviders ();
		Ui_.ReleasesWidget_->InitializeProviders ();
		Ui_.HypesWidget_->InitializeProviders ();
		Ui_.RecommendationsWidget_->InitializeProviders ();
	}

	void PlayerTab::SetupNavButtons ()
	{
		NavBar_ = new QTabBar ();
		NavBar_->hide ();
		NavBar_->setShape (QTabBar::RoundedWest);
		NavBar_->setUsesScrollButtons (false);
		NavBar_->setElideMode (Qt::ElideRight);
		NavBar_->setExpanding (false);
		NavBar_->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Expanding);

		NavButtons_ = new QListWidget ();
		NavButtons_->hide ();
		NavButtons_->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Expanding);
		NavButtons_->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
		NavButtons_->setFrameShape (QFrame::NoFrame);
		NavButtons_->setFrameShadow (QFrame::Plain);

		const QSize iconSize { 48, 48 };

		NavButtons_->setIconSize (iconSize);
		NavButtons_->setGridSize (iconSize + QSize { 8, 8 });
		NavButtons_->setViewMode (QListView::IconMode);
		NavButtons_->setMovement (QListView::Static);
		NavButtons_->setFlow (QListView::TopToBottom);
		new PaletteFixerFilter (NavButtons_);

		NavButtons_->setFixedWidth (NavButtons_->gridSize ().width () + 5);

		auto mkButton = [this] (const QString& title, const QString& iconName)
		{
			const auto& icon = Core::Instance ().GetProxy ()->
					GetIconThemeManager ()->GetIcon (iconName);

			auto but = new QListWidgetItem ();
			NavButtons_->addItem (but);
			but->setToolTip (title);
			but->setSizeHint (NavButtons_->gridSize ());
			but->setTextAlignment (Qt::AlignCenter);
			but->setIcon (icon);

			NavBar_->addTab (icon, title);
			NavBar_->setTabToolTip (NavBar_->count () - 1, title);
		};

		mkButton (tr ("Current song"), "view-media-lyrics");
		mkButton (tr ("Collection"), "folder-sound");
		mkButton (tr ("Playlists"), "view-media-playlist");
		mkButton (tr ("Social"), "system-users");
		mkButton (tr ("Internet"), "applications-internet");
		mkButton (tr ("Filesystem"), "document-open");
		mkButton (tr ("Devices"), "drive-removable-media-usb");

		NavButtons_->setCurrentRow (0);

		connect (NavBar_,
				SIGNAL (currentChanged (int)),
				Ui_.WidgetsStack_,
				SLOT (setCurrentIndex (int)));
		connect (NavButtons_,
				SIGNAL (currentRowChanged (int)),
				Ui_.WidgetsStack_,
				SLOT (setCurrentIndex (int)));
		connect (Ui_.WidgetsStack_,
				SIGNAL (currentChanged (int)),
				NavBar_,
				SLOT (setCurrentIndex (int)));
	}

	void PlayerTab::SetupToolbar ()
	{
		QAction *previous = new QAction (tr ("Previous track"), this);
		previous->setProperty ("ActionIcon", "media-skip-backward");
		connect (previous,
				SIGNAL (triggered ()),
				Player_,
				SLOT (previousTrack ()));
		TabToolbar_->addAction (previous);

		PlayPause_ = new QAction (tr ("Play/Pause"), this);
		PlayPause_->setProperty ("ActionIcon", "media-playback-start");
		PlayPause_->setProperty ("WatchActionIconChange", true);
		connect (PlayPause_,
				SIGNAL (triggered ()),
				Player_,
				SLOT (togglePause ()));
		TabToolbar_->addAction (PlayPause_);

		QAction *stop = new QAction (tr ("Stop"), this);
		stop->setProperty ("ActionIcon", "media-playback-stop");
		connect (stop,
				SIGNAL (triggered ()),
				Player_,
				SLOT (stop ()));
		TabToolbar_->addAction (stop);

		QAction *next = new QAction (tr ("Next track"), this);
		next->setProperty ("ActionIcon", "media-skip-forward");
		connect (next,
				SIGNAL (triggered ()),
				Player_,
				SLOT (nextTrack ()));
		TabToolbar_->addAction (next);

		TabToolbar_->addSeparator ();

		QAction *love = new QAction (tr ("Love"), this);
		love->setProperty ("ActionIcon", "emblem-favorite");
		love->setShortcut (QString ("Ctrl+L"));
		connect (love,
				SIGNAL (triggered ()),
				this,
				SLOT (handleLoveTrack ()));
		TabToolbar_->addAction (love);

		QAction *ban = new QAction (tr ("Ban"), this);
		ban->setProperty ("ActionIcon", "dialog-cancel");
		ban->setShortcut (QString ("Ctrl+B"));
		connect (ban,
				 SIGNAL (triggered ()),
				 this,
		   SLOT (handleBanTrack ()));
		TabToolbar_->addAction (ban);

		TabToolbar_->addSeparator ();

		auto seekSlider = new SeekSlider (Player_->GetSourceObject ());
		TabToolbar_->addWidget (seekSlider);
		TabToolbar_->addSeparator ();

		auto volumeSlider = new VolumeSlider (Player_->GetAudioOutput ());
		volumeSlider->setMinimumWidth (100);
		volumeSlider->setMaximumWidth (160);
		TabToolbar_->addWidget (volumeSlider);

		// fill tray menu
		connect (TrayIcon_,
				SIGNAL (changedVolume (qreal)),
				this,
				SLOT (handleChangedVolume (qreal)));
		connect (TrayIcon_,
				SIGNAL (activated (QSystemTrayIcon::ActivationReason)),
				this,
				SLOT (handleTrayIconActivated (QSystemTrayIcon::ActivationReason)));

		QAction *closeLMP = new QAction (tr ("Close LMP"), TrayIcon_);
		closeLMP->setProperty ("ActionIcon", "edit-delete");
		connect (closeLMP,
				SIGNAL (triggered ()),
				this,
				SLOT (closeLMP ()));

		connect (Player_->GetSourceObject (),
				SIGNAL (stateChanged (SourceState, SourceState)),
				this,
				SLOT (handleStateChanged ()));
		TrayMenu_->addAction (previous);
		TrayMenu_->addAction (PlayPause_);
		TrayMenu_->addAction (stop);
		TrayMenu_->addAction (next);
		TrayMenu_->addSeparator ();
		TrayMenu_->addAction (love);
		TrayMenu_->addAction (ban);
		TrayMenu_->addSeparator ();
		TrayMenu_->addAction (closeLMP);
		TrayIcon_->setContextMenu (TrayMenu_);
	}

	void PlayerTab::SetNowPlaying (const MediaInfo& info, const QPixmap& px)
	{
		Ui_.NowPlaying_->clear ();
		NotifyCurrentTrack (info, px, false);
	}

	void PlayerTab::Scrobble (const MediaInfo& info)
	{
		if (!XmlSettingsManager::Instance ()
				.property ("EnableScrobbling").toBool ())
			return;

		auto scrobblers = Core::Instance ().GetProxy ()->
					GetPluginsManager ()->GetAllCastableTo<Media::IAudioScrobbler*> ();
		if (info.Title_.isEmpty () && info.Artist_.isEmpty ())
		{
			std::for_each (scrobblers.begin (), scrobblers.end (),
					[] (decltype (scrobblers.front ()) s) { s->PlaybackStopped (); });
			return;
		}

		const Media::AudioInfo aInfo = info;
		std::for_each (scrobblers.begin (), scrobblers.end (),
					[&aInfo] (decltype (scrobblers.front ()) s) { s->NowPlaying (aInfo); });
	}

	void PlayerTab::FillSimilar (const Media::SimilarityInfos_t& infos)
	{
		Ui_.NPWidget_->SetSimilarArtists (infos);
	}

	void PlayerTab::RequestLyrics (const MediaInfo& info)
	{
		Ui_.NPWidget_->SetLyrics ({});

		if (!XmlSettingsManager::Instance ().property ("RequestLyrics").toBool ())
			return;

		auto finders = Core::Instance ().GetProxy ()->
					GetPluginsManager ()->GetAllCastableRoots<Media::ILyricsFinder*> ();
		Q_FOREACH (auto finderObj, finders)
		{
			connect (finderObj,
					SIGNAL (gotLyrics (Media::LyricsResults)),
					this,
					SLOT (handleGotLyrics (Media::LyricsResults)),
					Qt::UniqueConnection);
			auto finder = qobject_cast<Media::ILyricsFinder*> (finderObj);
			finder->RequestLyrics ({ info.Artist_, info.Album_, info.Title_ },
					Media::QueryOption::NoOption);
		}
	}

	namespace
	{
		struct PixmapInfo
		{
			QPixmap PX_;
			QString CoverPath_;
			bool IsCorrect_;
		};

		PixmapInfo GetPixmap (const MediaInfo& info)
		{
			PixmapInfo pi;

			pi.CoverPath_ = FindAlbumArtPath (info.LocalPath_);
			if (!pi.CoverPath_.isEmpty ())
				pi.PX_ = QPixmap (pi.CoverPath_);

			pi.IsCorrect_ = !pi.PX_.isNull ();
			if (!pi.IsCorrect_)
			{
				pi.PX_ = QIcon::fromTheme ("media-optical").pixmap (128, 128);
				pi.CoverPath_.clear ();
			}

			return pi;
		}
	}

	void PlayerTab::NotifyCurrentTrack (const MediaInfo& info, QPixmap notifyPx, bool fromUser)
	{
		QString text;
		if (Player_->GetState () != SourceState::Stopped)
		{
			const auto& title = info.Title_.isEmpty () ? tr ("unknown song") : info.Title_;
			const auto& album = info.Album_.isEmpty () ? tr ("unknown album") : info.Album_;
			const auto& track = info.Artist_.isEmpty () ? tr ("unknown artist") : info.Artist_;

			text = tr ("Now playing: %1 from %2 by %3")
					.arg ("<em>" + title + "</em>")
					.arg ("<em>" + album + "</em>")
					.arg ("<em>" + track + "</em>");
			if (!fromUser)
				Ui_.NowPlaying_->setText (text);
		}
		else if (fromUser)
			text = tr ("Playback is stopped.");
		else
			return;

		if (!text.isEmpty () && (fromUser ||
				XmlSettingsManager::Instance ().property ("EnableNotifications").toBool ()))
		{
			int width = notifyPx.width ();
			if (width > 200)
			{
				while (width > 200)
					width /= 2;
				notifyPx = notifyPx.scaledToWidth (width);
			}

			Entity e = Util::MakeNotification ("LMP", text, PInfo_);
			e.Additional_ ["NotificationPixmap"] = notifyPx;
			emit gotEntity (e);
		}
	}

	namespace
	{
		QIcon GetIconFromState (SourceState state)
		{
			const auto mgr = Core::Instance ().GetProxy ()->GetIconThemeManager ();
			switch (state)
			{
			case SourceState::Playing:
				return mgr->GetIcon ("media-playback-start");
			case SourceState::Paused:
				return mgr->GetIcon ("media-playback-pause");
			default:
				return QIcon ();
			}
		}

		template<typename T>
		void UpdateIcon (T iconable, SourceState state, std::function<QSize (T)> iconSizeGetter)
		{
			const QSize& iconSize = iconSizeGetter (iconable);
			if (iconSize.isEmpty ())
				return;

			QIcon icon = GetIconFromState (state);
			QIcon baseIcon = icon.isNull () ?
				QIcon ("lcicons:/lmp/resources/images/lmp.svg") :
				iconable->icon ();

			QPixmap px = baseIcon.pixmap (iconSize);

			if (!icon.isNull ())
			{
				QPixmap statePx = icon.pixmap (iconSize);

				QPainter p (&px);
				p.drawPixmap (0 + iconSize.width () / 2,
						0 + iconSize.height () / 2 ,
						iconSize.width () / 2,
						iconSize.height () / 2,
						statePx);
				p.end ();
			}

			iconable->setIcon (QIcon (px));
		}
	}

	void PlayerTab::handleSongChanged (const MediaInfo& info)
	{
		const auto& pxInfo = GetPixmap (info);

		NPPixmapHandler_->HandleSongChanged (info, pxInfo.CoverPath_, pxInfo.PX_, pxInfo.IsCorrect_);

		Ui_.NPWidget_->SetTrackInfo (info);

		SetNowPlaying (info, pxInfo.PX_);
		Scrobble (info);
		RequestLyrics (info);

		if (info.Artist_.isEmpty ())
		{
			LastArtist_.clear ();
			FillSimilar (Media::SimilarityInfos_t ());
		}
		else if (!Similars_.contains (info.Artist_))
		{
			auto similars = Core::Instance ().GetProxy ()->
					GetPluginsManager ()->GetAllCastableTo<Media::ISimilarArtists*> ();
			Q_FOREACH (auto *similar, similars)
			{
				auto obj = similar->GetSimilarArtists (info.Artist_, 15);
				if (!obj)
					continue;
				connect (obj->GetQObject (),
						SIGNAL (error ()),
						this,
						SLOT (handleSimilarError ()));
				connect (obj->GetQObject (),
						SIGNAL (ready ()),
						this,
						SLOT (handleSimilarReady ()));
			}
		}
		else if (info.Artist_ != LastArtist_)
		{
			LastArtist_ = info.Artist_;
			FillSimilar (Similars_ [info.Artist_]);
		}
	}

	void PlayerTab::handleSongInfoUpdated (const MediaInfo& info)
	{
		Ui_.NPWidget_->SetTrackInfo (info);
	}

	namespace
	{
		void AddToLovedBanned (const QString& trackPath,
				LocalCollection::StaticRating rating,
				std::function<void (Media::IAudioScrobbler*)> marker)
		{
			const int trackId = Core::Instance ().GetLocalCollection ()->FindTrack (trackPath);
			if (trackId >= 0)
				Core::Instance ().GetLocalCollection ()->AddTrackTo (trackId, rating);

			if (!XmlSettingsManager::Instance ()
					.property ("EnableScrobbling").toBool ())
				return;

			auto scrobblers = Core::Instance ().GetProxy ()->
						GetPluginsManager ()->GetAllCastableTo<Media::IAudioScrobbler*> ();
			std::for_each (scrobblers.begin (), scrobblers.end (),
					[marker] (decltype (scrobblers.front ()) s) { marker (s); });
		}
	}

	void PlayerTab::handleLoveTrack ()
	{
		AddToLovedBanned (Player_->GetCurrentMediaInfo ().LocalPath_,
				LocalCollection::StaticRating::Loved,
				[] (Media::IAudioScrobbler *s) { s->LoveCurrentTrack (); });
	}

	void PlayerTab::handleBanTrack ()
	{
		AddToLovedBanned (Player_->GetCurrentMediaInfo ().LocalPath_,
				LocalCollection::StaticRating::Banned,
				[] (Media::IAudioScrobbler *s) { s->BanCurrentTrack (); });
	}

	void PlayerTab::handleSimilarError ()
	{
		qWarning () << Q_FUNC_INFO;
		sender ()->deleteLater ();
	}

	void PlayerTab::handleSimilarReady ()
	{
		sender ()->deleteLater ();
		auto obj = qobject_cast<Media::IPendingSimilarArtists*> (sender ());

		const auto& similar = obj->GetSimilar ();
		LastArtist_ = obj->GetSourceArtistName ();
		Similars_ [LastArtist_] = similar;
		FillSimilar (similar);
	}

	void PlayerTab::handleGotLyrics (const Media::LyricsResults& results)
	{
		if (results.Items_.isEmpty ())
			return;

		for (const auto& item : results.Items_)
			Ui_.NPWidget_->SetLyrics (item);
	}

	void PlayerTab::handlePlayerAvailable (bool available)
	{
		TabToolbar_->setEnabled (available);
		Ui_.Playlist_->setEnabled (available);
		Ui_.PlaylistsTab_->setEnabled (available);
		Ui_.CollectionTab_->setEnabled (available);
		Ui_.RadioTab_->setEnabled (available);
	}

	void PlayerTab::notifyCurrentTrack ()
	{
		const auto& info = Player_->GetCurrentMediaInfo ();
		NotifyCurrentTrack (info, GetPixmap (info).PX_, true);
	}

	void PlayerTab::closeLMP ()
	{
		Remove ();
	}

	void PlayerTab::handleStateChanged ()
	{
		const auto newState = Player_->GetSourceObject ()->GetState ();
		if (newState == SourceState::Playing)
			PlayPause_->setProperty ("ActionIcon", "media-playback-pause");
		else
		{
			if (newState == SourceState::Stopped)
				TrayIcon_->handleSongChanged (MediaInfo ());
			PlayPause_->setProperty ("ActionIcon", "media-playback-start");
		}
		UpdateIcon<LMPSystemTrayIcon*> (TrayIcon_, newState,
				[] (QSystemTrayIcon *icon) { return icon->geometry ().size (); });
	}

	void PlayerTab::handleShowTrayIcon ()
	{
		TrayIcon_->setVisible (XmlSettingsManager::Instance ().property ("ShowTrayIcon").toBool ());
	}

	void PlayerTab::handleUseNavTabBar ()
	{
		if (Ui_.WidgetsLayout_->count () == 2)
		{
			auto item = Ui_.WidgetsLayout_->takeAt (0);
			item->widget ()->hide ();
			delete item;
		}
		const bool useTabs = XmlSettingsManager::Instance ().property ("UseNavTabBar").toBool ();
		QWidget *widget = useTabs ?
				static_cast<QWidget*> (NavBar_) :
				static_cast<QWidget*> (NavButtons_);
		Ui_.WidgetsLayout_->insertWidget (0, widget, 0,
				useTabs ? Qt::AlignTop : Qt::Alignment ());
		widget->show ();
	}

	void PlayerTab::handleChangedVolume (qreal delta)
	{
		qreal volume = Player_->GetAudioOutput ()->GetVolume ();
		qreal dl = delta > 0 ? 0.05 : -0.05;
		if (volume != volume)
			volume = 0.0;

		volume += dl;
		if (volume < 0)
			volume = 0;
		else if (volume > 1)
			volume = 1;

		Player_->GetAudioOutput ()->setVolume (volume);
	}

	void PlayerTab::handleTrayIconActivated (QSystemTrayIcon::ActivationReason reason)
	{
		switch (reason)
		{
			case QSystemTrayIcon::MiddleClick:
				Player_->togglePause ();
				break;
			default:
				break;
		}
	}
}
}
