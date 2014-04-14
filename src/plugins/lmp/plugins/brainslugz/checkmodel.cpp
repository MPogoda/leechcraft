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

#include "checkmodel.h"
#include <functional>
#include <QtDebug>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <interfaces/media/idiscographyprovider.h>
#include <interfaces/media/ialbumartprovider.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/iiconthememanager.h>
#include <util/util.h>
#include <util/sll/onetimerunner.h>

namespace LeechCraft
{
namespace LMP
{
namespace BrainSlugz
{
	namespace
	{
		const int AASize = 130;

		class ReleasesSubmodel : public QStandardItemModel
		{
		public:
			enum Role
			{
				ReleaseName = Qt::UserRole + 1,
				ReleaseYear,
				ReleaseArt
			};

			ReleasesSubmodel (QObject *parent)
			: QStandardItemModel { parent }
			{
				QHash<int, QByteArray> roleNames;
				roleNames [ReleaseName] = "releaseName";
				roleNames [ReleaseYear] = "releaseYear";
				roleNames [ReleaseArt] = "releaseArt";
				setRoleNames (roleNames);
			}
		};
	}

	CheckModel::CheckModel (const Collection::Artists_t& artists,
			const ICoreProxy_ptr& proxy, QObject *parent)
	: QStandardItemModel { parent }
	, AllArtists_ { artists }
	, DefaultAlbumIcon_ { Util::GetAsBase64Src (proxy->GetIconThemeManager ()->
					GetIcon ("media-optical").pixmap (AASize * 2, AASize * 2).toImage ()) }
	, AAProvObj_ { proxy->GetPluginsManager ()->
				GetAllCastableRoots<Media::IAlbumArtProvider*> ().value (0) }
	, AAProv_ { qobject_cast<Media::IAlbumArtProvider*> (AAProvObj_) }
	{
		QHash<int, QByteArray> roleNames;
		roleNames [Role::ArtistId] = "artistId";
		roleNames [Role::ArtistName] = "artistName";
		roleNames [Role::ScheduledToCheck] = "scheduled";
		roleNames [Role::IsChecked] = "isChecked";
		roleNames [Role::Releases] = "releases";
		setRoleNames (roleNames);

		for (const auto& artist : artists)
		{
			if (artist.Name_.contains (" vs. ") ||
					artist.Name_.contains (" with "))
				continue;

			auto item = new QStandardItem { artist.Name_ };
			item->setData (artist.ID_, Role::ArtistId);
			item->setData (artist.Name_, Role::ArtistName);
			item->setData (true, Role::ScheduledToCheck);
			item->setData (false, Role::IsChecked);

			const auto submodel = new ReleasesSubmodel { this };
			item->setData (QVariant::fromValue<QObject*> (submodel), Role::Releases);

			appendRow (item);

			Artist2Submodel_ [artist.ID_] = submodel;
			Artist2Item_ [artist.ID_] = item;

			Scheduled_ << artist.ID_;
		}
	}

	Collection::Artists_t CheckModel::GetSelectedArtists () const
	{
		Collection::Artists_t result = AllArtists_;
		result.erase (std::remove_if (result.begin (), result.end (),
					[this] (const Collection::Artist& artist)
					{
						return !Scheduled_.contains (artist.ID_);
					}),
				result.end ());
		return result;
	}

	void CheckModel::SetMissingReleases (const QList<Media::ReleaseInfo>& releases,
			const Collection::Artist& artist)
	{
		qDebug () << Q_FUNC_INFO << artist.Name_ << releases.size ();

		const auto item = Artist2Item_.value (artist.ID_);
		if (!item)
		{
			qWarning () << Q_FUNC_INFO
					<< "no item for artist"
					<< artist.Name_;
			return;
		}

		const auto model = Artist2Submodel_.value (artist.ID_);
		for (const auto& release : releases)
		{
			auto item = new QStandardItem;
			item->setData (release.Name_, ReleasesSubmodel::ReleaseName);
			item->setData (release.Year_, ReleasesSubmodel::ReleaseYear);
			item->setData (DefaultAlbumIcon_, ReleasesSubmodel::ReleaseArt);
			model->appendRow (item);

			const auto proxy = AAProv_->RequestAlbumArt ({ artist.Name_, release.Name_ });
			new Util::OneTimeRunner
			{
				[item, proxy] () -> void
				{
					const auto& image = proxy->GetImages ().value (0);
					if (image.isNull ())
						return;

					auto watcher = new QFutureWatcher<QString>;
					watcher->setFuture (QtConcurrent::run ([image]
							{
								return Util::GetAsBase64Src (image.scaled (AASize, AASize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
							}));

					new Util::OneTimeRunner
					{
						[watcher, item] { item->setData (watcher->result (), ReleasesSubmodel::ReleaseArt); },
						watcher,
						SIGNAL (finished ())
					};
				},
				proxy->GetQObject (),
				SIGNAL (ready (Media::AlbumInfo, QList<QImage>))
			};
		}

		item->setData (true, Role::IsChecked);
	}

	void CheckModel::MarkNoNews (const Collection::Artist& artist)
	{
		const auto item = Artist2Item_.take (artist.ID_);
		if (!item)
		{
			qWarning () << Q_FUNC_INFO
					<< "no item for artist"
					<< artist.Name_;
			return;
		}

		removeRow (item->row ());
		Artist2Submodel_.take (artist.ID_)->deleteLater ();
	}

	void CheckModel::setArtistScheduled (int id, bool scheduled)
	{
		Artist2Item_ [id]->setData (true, Role::ScheduledToCheck);

		if (scheduled)
			Scheduled_ << id;
		else
			Scheduled_.remove (id);
	}
}
}
}
