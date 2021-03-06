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

#include "mtpsync.h"
#include <map>
#include <thread>
#include <QIcon>
#include <QTimer>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QBuffer>
#include <QAbstractItemModel>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/devices/iremovabledevmanager.h>
#include <interfaces/devices/deviceroles.h>

namespace LeechCraft
{
namespace LMP
{
namespace MTPSync
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;

		LIBMTP_Init ();

		QTimer::singleShot (5000,
				this,
				SLOT (pollDevices ()));
	}

	void Plugin::SecondInit ()
	{
	}

	void Plugin::Release ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.LMP.MTPSync";
	}

	QString Plugin::GetName () const
	{
		return "LMP MTPSync";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Adds support for synchronization with MTP-enabled portable media players.");
	}

	QIcon Plugin::GetIcon () const
	{
		return QIcon ();
	}

	QSet<QByteArray> Plugin::GetPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.LMP.CollectionSync";
		return result;
	}

	void Plugin::SetLMPProxy (ILMPProxy_ptr proxy)
	{
		LMPProxy_ = proxy;
	}

	QString Plugin::GetSyncSystemName () const
	{
		return "MTP";
	}

	QObject* Plugin::GetQObject ()
	{
		return this;
	}

	UnmountableDevInfos_t Plugin::AvailableDevices () const
	{
		UnmountableDevInfos_t result;
		result.reserve (Infos_.size ());
		for (const auto& item : Infos_)
			result << item.Info_;
		return result;
	}

	void Plugin::SetFileInfo (const QString& origLocalPath, const UnmountableFileInfo& info)
	{
		OrigInfos_ [origLocalPath] = info;
	}

	void Plugin::Upload (const QString& localPath, const QString& origPath, const QByteArray& devId, const QByteArray& storageId)
	{
		qDebug () << Q_FUNC_INFO << IsPolling_ << localPath << devId;
		if (IsPolling_)
		{
			UploadQueue_.append ({ localPath, origPath, devId, storageId });
			return;
		}

		if (!DevicesCache_.contains (devId))
		{
			qDebug () << "device not in cache, opening...";

			bool found = false;

			LIBMTP_raw_device_t *rawDevices;
			int numRawDevices = 0;
			LIBMTP_Detect_Raw_Devices (&rawDevices, &numRawDevices);
			for (int i = 0; i < numRawDevices; ++i)
			{
				std::shared_ptr<LIBMTP_mtpdevice_t> device (LIBMTP_Open_Raw_Device (&rawDevices [i]), LIBMTP_Release_Device);
				if (!device)
					continue;

				const auto& serial = LIBMTP_Get_Serialnumber (device.get ());
				qDebug () << "matching against" << serial;
				if (serial == devId)
				{
					DevicesCache_ [devId] = DeviceCacheEntry { std::move (device) };
					found = true;
					break;
				}
			}
			free (rawDevices);

			if (!found)
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to find device"
						<< devId;
				emit uploadFinished (localPath,
						QFile::ResourceError,
						tr ("Unable to find the requested device."));
				return;
			}
		}

		const auto& entry = DevicesCache_ [devId];
		UploadTo (entry.Device_.get (), storageId, localPath, origPath);
	}

	void Plugin::Refresh ()
	{
		pollDevices ();
	}

	void Plugin::HandleTransfer (quint64 sent, quint64 total)
	{
		emit uploadProgress (sent, total);
	}

	namespace
	{
		struct CallbackData
		{
			Plugin *Plugin_;
			mutable QElapsedTimer Timer_;
		};

		int TransferCallback (uint64_t sent, uint64_t total, const void *rawData)
		{
			auto data = static_cast<const CallbackData*> (rawData);

			if (data->Timer_.elapsed () > 100)
			{
				data->Plugin_->HandleTransfer (sent, total);
				data->Timer_.restart ();
			}

			return 0;
		}

		LIBMTP_filetype_t GetFileType (const QString& format)
		{
			QMap<QString, LIBMTP_filetype_t> map;
			map ["mp3"] = LIBMTP_FILETYPE_MP3;
			map ["ogg"] = LIBMTP_FILETYPE_OGG;
			map ["aac"] = LIBMTP_FILETYPE_AAC;
			map ["aac-free"] = LIBMTP_FILETYPE_AAC;
			map ["aac-nonfree"] = LIBMTP_FILETYPE_AAC;
			map ["flac"] = LIBMTP_FILETYPE_FLAC;
			map ["wma"] = LIBMTP_FILETYPE_WMA;
			return map.value (format, LIBMTP_FILETYPE_UNDEF_AUDIO);
		}

		struct UploadInfo
		{
			int Res_;
			LIBMTP_mtpdevice_t *Device_;
			QString LocalPath_;
			LIBMTP_track_t *Track_;
			UnmountableFileInfo Info_;
		};
	}

	void Plugin::UploadTo (LIBMTP_mtpdevice_t *device, const QByteArray& storageId,
			const QString& localPath, const QString& origPath)
	{
		if (!device->storage)
			LIBMTP_Get_Storage (device, 0);

		auto storage = device->storage;

		while (storage)
		{
			if (QByteArray::number (storage->id) == storageId)
				break;
			storage = storage->next;
		}

		if (!storage)
		{
			qWarning () << Q_FUNC_INFO
					<< "could not find storage"
					<< storageId;
			emit uploadFinished (localPath,
					QFile::ResourceError,
					tr ("Unable to find the requested storage."));
			return;
		}

		IsUploading_ = true;

		const auto id = storage->id;
		const auto& info = OrigInfos_.take (origPath);

		qDebug () << "uploading" << localPath << "of type" << GetFileType (info.FileFormat_) << "to" << storage->id;

		auto track = LIBMTP_new_track_t ();
		track->storage_id = id;

		auto getStr = [] (const QString& str) { return strdup (str.toUtf8 ().constData ()); };

		track->filename = getStr (QFileInfo (localPath).fileName ());
		track->album = getStr (info.Album_);
		track->title = getStr (info.TrackTitle_);
		track->genre = getStr (info.Genres_.join ("; "));
		track->artist = getStr (info.Artist_);
		track->tracknumber = info.TrackNumber_;
		track->filetype = GetFileType (info.FileFormat_);
		track->filesize = QFileInfo (localPath).size ();
		track->date = getStr (QString::number (info.AlbumYear_) + "0101T0000.0");

		auto watcher = new QFutureWatcher<UploadInfo> ();
		connect (watcher,
				SIGNAL (finished ()),
				this,
				SLOT (handleUploadFinished ()));
		const auto future = QtConcurrent::run ([=] () -> UploadInfo
			{
				const auto cbData = new CallbackData { this, {} };
				cbData->Timer_.start ();
				const auto res = LIBMTP_Send_Track_From_File (device,
						localPath.toUtf8 ().constData (), track,
						TransferCallback, cbData);
				delete cbData;

				if (!res)
					AppendAlbum (device, track, info);

				return { res, device, localPath, track, info };
			});
		watcher->setFuture (future);
	}

	namespace
	{
		void SetAlbumArt (LIBMTP_mtpdevice_t *device, LIBMTP_album_t *album, const QString& path)
		{
			if (path.isEmpty ())
				return;

			QImage image (path);
			if (image.isNull ())
				return;

			QBuffer buffer;
			buffer.open (QIODevice::WriteOnly);
			image.save (&buffer, "JPG", 90);

			auto albumArt = LIBMTP_new_filesampledata_t ();
			albumArt->data = static_cast<char*> (malloc (buffer.buffer ().size ()));
			memcpy (albumArt->data, buffer.buffer ().constData (), buffer.buffer ().size ());
			albumArt->size = buffer.buffer ().size ();
			albumArt->filetype = LIBMTP_FILETYPE_JPEG;
			albumArt->width = image.width ();
			albumArt->height = image.height ();

			if (LIBMTP_Send_Representative_Sample (device, album->album_id, albumArt))
			{
				LIBMTP_Dump_Errorstack (device);
				LIBMTP_Clear_Errorstack (device);
			}

			LIBMTP_destroy_filesampledata_t (albumArt);
		}
	}

	void Plugin::AppendAlbum (LIBMTP_mtpdevice_t *device, LIBMTP_track_t *track, const UnmountableFileInfo& info)
	{
		auto albuminfo = LIBMTP_new_album_t ();
		albuminfo->artist = strdup (info.Artist_.toUtf8 ().constData ());
		albuminfo->name = strdup (info.Album_.toUtf8 ().constData ());
		albuminfo->genre = strdup (info.Genres_.join ("; ").toUtf8 ().constData ());

		auto album = LIBMTP_Get_Album_List (device);
		auto albumOrig = album;

		decltype (album) foundAlbum = nullptr, resultingAlgum = nullptr;

		while (album)
		{
			if (album->name &&
					(album->artist || album->composer) &&
					QString::fromUtf8 (album->name) == info.Album_ &&
					(QString::fromUtf8 (album->artist) == info.Artist_ ||
					 QString::fromUtf8 (album->composer) == info.Artist_))
			{
				foundAlbum = album;
				album = album->next;
				foundAlbum->next = nullptr;
			}
			else
				album = album->next;
		}

		if (foundAlbum)
		{
			auto tracks = static_cast<uint32_t*> (malloc ((foundAlbum->no_tracks + 1) * sizeof (uint32_t)));

			++foundAlbum->no_tracks;

			if (foundAlbum->tracks)
			{
				memcpy (tracks, foundAlbum->tracks, foundAlbum->no_tracks * sizeof (uint32_t));
				free (foundAlbum->tracks);
			}

			tracks [foundAlbum->no_tracks - 1] = track->item_id;
			foundAlbum->tracks = tracks;

			if (LIBMTP_Update_Album (device, foundAlbum))
			{
				LIBMTP_Dump_Errorstack (device);
				LIBMTP_Clear_Errorstack (device);
			}

			resultingAlgum = foundAlbum;
		}
		else
		{
			auto trackId = static_cast<uint32_t*> (malloc (sizeof (uint32_t)));
			*trackId = track->item_id;
			albuminfo->tracks = trackId;
			albuminfo->no_tracks = 1;
			albuminfo->storage_id = track->storage_id;

			if (LIBMTP_Create_New_Album (device, albuminfo))
			{
				LIBMTP_Dump_Errorstack (device);
				LIBMTP_Clear_Errorstack (device);
			}

			resultingAlgum = albuminfo;
		}

		SetAlbumArt (device, resultingAlgum, info.AlbumArtPath_);

		while (albumOrig)
		{
			auto tmp = albumOrig;
			albumOrig = albumOrig->next;
			LIBMTP_destroy_album_t (tmp);
		}

		LIBMTP_destroy_album_t (albuminfo);
	}

	void Plugin::Subscribe2Devs ()
	{
		for (auto mgr : Proxy_->GetPluginsManager ()->GetAllCastableTo<IRemovableDevManager*> ())
		{
			if (!mgr->SupportsDevType (USBDevice))
				continue;

			Model_ = mgr->GetDevicesModel ();
			connect (Model_,
					SIGNAL (rowsInserted (QModelIndex, int, int)),
					this,
					SLOT (handleRowsInserted (QModelIndex, int, int)));
			connect (Model_,
					SIGNAL (rowsAboutToBeRemoved (QModelIndex, int, int)),
					this,
					SLOT (handleRowsRemoved (QModelIndex, int, int)));
			break;
		}
	}

	namespace
	{
		QStringList GetSupportedFormats (LIBMTP_mtpdevice_t *device)
		{
			static const std::map<uint16_t, QString> formatID2Format =
			{
				{ LIBMTP_FILETYPE_MP3, "mp3" },
				{ LIBMTP_FILETYPE_MP4, "mp4" },
				{ LIBMTP_FILETYPE_OGG, "ogg" },
				{ LIBMTP_FILETYPE_ASF, "asf" },
				{ LIBMTP_FILETYPE_AAC, "aac" },
				{ LIBMTP_FILETYPE_FLAC, "flac" },
				{ LIBMTP_FILETYPE_WMA, "wma" },

				// uninteresting formats go here
				{ LIBMTP_FILETYPE_FOLDER, {} },
				{ LIBMTP_FILETYPE_WMV, {} },
				{ LIBMTP_FILETYPE_AVI, {} },
				{ LIBMTP_FILETYPE_MPEG, {} },
				{ LIBMTP_FILETYPE_JPEG, {} }

			};

			QStringList result;

			uint16_t *formats = 0;
			uint16_t formatsLength = 0;
			LIBMTP_Get_Supported_Filetypes (device, &formats, &formatsLength);
			for (uint16_t i = 0; i < formatsLength; ++i)
			{
				auto format = formats [i];
				const auto pos = formatID2Format.find (format);
				if (pos == formatID2Format.end ())
				{
					qWarning () << Q_FUNC_INFO
							<< "unknown format"
							<< format;
					continue;
				}
				result << pos->second;
			}
			free (formats);

			result.removeAll ({});

			return result;
		}

		QList<UnmountablePartition> GetPartitions (LIBMTP_mtpdevice_t *device)
		{
			LIBMTP_Get_Storage (device, LIBMTP_STORAGE_SORTBY_MAXSPACE);
			auto storage = device->storage;

			qDebug () << Q_FUNC_INFO << "initial storage:" << storage;
			QList<UnmountablePartition> result;
			while (storage)
			{
				const UnmountablePartition part
				{
					QByteArray::number (storage->id),
					QString::fromUtf8 (storage->StorageDescription),
					storage->FreeSpaceInBytes,
					storage->MaxCapacity
				};
				result << part;
				storage = storage->next;

				qDebug () << Q_FUNC_INFO << "next storage:" << storage;
			}

			return result;
		}

		UnmountableDevInfo InfoFromDevice (LIBMTP_mtpdevice_t *device)
		{
			const auto& devName = QString::fromUtf8 (LIBMTP_Get_Manufacturername (device)) + " " +
					QString::fromUtf8 (LIBMTP_Get_Modelname (device)) + " " +
					LIBMTP_Get_Friendlyname (device);

			int battPercentage = -1;
			uint8_t maxBattLevel = 0, curBattLevel = 0;
			if (!LIBMTP_Get_Batterylevel (device, &maxBattLevel, &curBattLevel) && curBattLevel)
				battPercentage = 100 * curBattLevel / maxBattLevel;

			qDebug () << Q_FUNC_INFO << curBattLevel << maxBattLevel << battPercentage;

			return
			{
				LIBMTP_Get_Serialnumber (device),
				LIBMTP_Get_Manufacturername (device),
				devName.simplified ().trimmed (),
				GetPartitions (device),
				GetSupportedFormats (device),
				battPercentage
			};
		}

		USBDevInfos_t EnumerateWorker ()
		{
			qDebug () << Q_FUNC_INFO;
			USBDevInfos_t infos;

			LIBMTP_raw_device_t *rawDevices;
			int numRawDevices = 0;
			LIBMTP_Detect_Raw_Devices (&rawDevices, &numRawDevices);
			qDebug () << Q_FUNC_INFO << "detected" << numRawDevices << "devices";
			for (int i = 0; i < numRawDevices; ++i)
			{
				auto device = LIBMTP_Open_Raw_Device (&rawDevices [i]);
				if (!device)
					continue;

				infos.push_back ({
						InfoFromDevice (device),
						static_cast<int> (rawDevices [i].bus_location),
						static_cast<int> (rawDevices [i].devnum)
					});
				LIBMTP_Release_Device (device);
			}
			free (rawDevices);
			qDebug () << Q_FUNC_INFO << "done";

			return infos;
		}
	}

	void Plugin::handleUploadFinished ()
	{
		auto watcher = dynamic_cast<QFutureWatcher<UploadInfo>*> (sender ());
		watcher->deleteLater ();

		const auto& info = watcher->result ();

		qDebug () << "send result:" << info.Res_;
		if (info.Res_)
		{
			LIBMTP_Dump_Errorstack (info.Device_);
			LIBMTP_Clear_Errorstack (info.Device_);
		}

		LIBMTP_destroy_track_t (info.Track_);

		IsUploading_ = false;

		emit uploadFinished (info.LocalPath_, QFile::NoError, {});
	}

	void Plugin::pollDevices ()
	{
		if (IsPolling_)
			return;

		if (IsUploading_)
		{
			qDebug () << Q_FUNC_INFO
					<< "uploading in progress, not polling";
			QTimer::singleShot (120000,
					this,
					SLOT (pollDevices ()));
			return;
		}

		auto watcher = new QFutureWatcher<USBDevInfos_t> ();
		connect (watcher,
				SIGNAL (finished ()),
				this,
				SLOT (handlePollFinished ()));
		auto future = QtConcurrent::run (EnumerateWorker);
		watcher->setFuture (future);

		IsPolling_ = true;
	}

	void Plugin::handlePollFinished ()
	{
		IsPolling_ = false;
		while (!UploadQueue_.isEmpty ())
		{
			const auto& item = UploadQueue_.takeFirst ();
			Upload (item.LocalPath_, item.OrigLocalPath_, item.To_, item.StorageID_);
		}

		auto watcher = dynamic_cast<QFutureWatcher<USBDevInfos_t>*> (sender ());
		watcher->deleteLater ();

		const auto& infos = watcher->result ();
		if (!infos.isEmpty ())
		{
			Infos_ = infos;
			emit availableDevicesChanged ();
		}

		if (FirstPoll_)
		{
			Subscribe2Devs ();
			FirstPoll_ = false;
		}

		QTimer::singleShot (120000,
				this,
				SLOT (pollDevices ()));
	}

	void Plugin::handleRowsInserted (const QModelIndex& parent, int start, int end)
	{
		if (parent.isValid ())
			return;

		bool hasNew = false;
		for (auto i = start; i <= end; ++i)
		{
			const auto& idx = Model_->index (i, 0);

			const auto busnum = idx.data (USBDeviceRole::Busnum).toInt ();
			const auto devnum = idx.data (USBDeviceRole::Devnum).toInt ();

			if (LIBMTP_Check_Specific_Device (busnum, devnum))
			{
				hasNew = true;
				break;
			}
		}

		if (!hasNew)
			return;

		QTimer::singleShot (1000,
				this,
				SLOT (pollDevices ()));
	}

	void Plugin::handleRowsRemoved (const QModelIndex& parent, int start, int end)
	{
		if (parent.isValid ())
			return;

		bool changed = false;
		for (auto i = start; i <= end; ++i)
		{
			const auto& idx = Model_->index (i, 0);

			const auto busnum = idx.data (USBDeviceRole::Busnum).toInt ();
			const auto devnum = idx.data (USBDeviceRole::Devnum).toInt ();

			const auto pos = std::find_if (Infos_.begin (), Infos_.end (),
					[&busnum, &devnum] (const USBDevInfo& info)
						{ return info.Busnum_ == busnum && info.Devnum_ == devnum; });
			if (pos == Infos_.end ())
				continue;

			DevicesCache_.remove (pos->Info_.ID_);

			Infos_.erase (pos);
			changed = true;
		}

		if (changed)
			emit availableDevicesChanged ();
	}
}
}
}

LC_EXPORT_PLUGIN (leechcraft_lmp_mtpsync, LeechCraft::LMP::MTPSync::Plugin);
