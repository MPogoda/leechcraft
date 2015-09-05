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

#include "avatarsstorage.h"
#include <QBuffer>
#include <QtDebug>
#include <util/threads/futures.h>
#include "avatarsstoragethread.h"
#include "interfaces/azoth/iclentry.h"

namespace LeechCraft
{
namespace Azoth
{
	AvatarsStorage::AvatarsStorage (QObject *parent)
	: QObject { parent }
	, StorageThread_ { new AvatarsStorageThread { this } }
	, Cache_ { 5 * 1024 * 1024 }
	{
	}

	namespace
	{
		int GetImageCost (const QImage& image)
		{
			if (image.isNull ())
				return 1;

			return image.width () * image.height () * image.depth () / 8;
		}
	}

	QFuture<void> AvatarsStorage::SetAvatar (const ICLEntry *entry,
			IHaveAvatars::Size size, const QImage& image)
	{
		QByteArray data;
		QBuffer buffer { &data };
		image.save (&buffer, "PNG", 0);

		const auto& entryId = entry->GetEntryID ();

		Cache_.insert (entryId, new CacheValue_t { image }, GetImageCost (image));

		return StorageThread_->SetAvatar (entryId, size, data);
	}

	QFuture<void> AvatarsStorage::SetAvatar (const QString& entryId,
			IHaveAvatars::Size size, const QByteArray& data)
	{
		Cache_.insert (entryId, new CacheValue_t { data }, data.size ());

		return StorageThread_->SetAvatar (entryId, size, data);
	}

	namespace
	{
		struct ToByteArray : boost::static_visitor<QByteArray>
		{
			QByteArray operator() (const QByteArray& array) const
			{
				return array;
			}

			QByteArray operator() (const QImage& image) const
			{
				qDebug () << Q_FUNC_INFO
						<< "cache semimiss";

				QByteArray data;
				QBuffer buffer { &data };
				image.save (&buffer, "PNG", 0);

				return data;
			}
		};

		struct ToImage : boost::static_visitor<QImage>
		{
			QImage operator() (const QImage& image) const
			{
				return image;
			}

			QImage operator() (const QByteArray& array) const
			{
				qDebug () << Q_FUNC_INFO
						<< "cache semimiss";

				QImage image;
				if (!image.loadFromData (array))
				{
					qWarning () << Q_FUNC_INFO
							<< "unable to load image";
					return {};
				}

				return image;
			}
		};
	}

	QFuture<MaybeImage> AvatarsStorage::GetAvatar (const ICLEntry *entry, IHaveAvatars::Size size)
	{
		const auto& entryId = entry->GetEntryID ();
		if (const auto value = Cache_ [entryId])
		{
			const auto& image = boost::apply_visitor (ToImage {}, *value);
			CacheValue_t convertedValue { image };
			if (convertedValue.which () != value->which ())
				Cache_.insert (entryId, new CacheValue_t { std::move (convertedValue) }, GetImageCost (image));

			return Util::MakeReadyFuture<MaybeImage> (image);
		}

		const auto& hrId = entry->GetHumanReadableID ();

		return Util::Sequence (this, [=] { return StorageThread_->GetAvatar (entryId, size); }) >>
				[=] (const MaybeByteArray& data)
				{
					if (!data)
						return Util::MakeReadyFuture<MaybeImage> ({});

					return QtConcurrent::run ([=] () -> MaybeImage
							{
								QImage image;
								if (!image.loadFromData (*data))
								{
									qWarning () << Q_FUNC_INFO
											<< "unable to load image from data for"
											<< entryId
											<< hrId;
									return {};
								}

								return image;
							});
				};
	}

	QFuture<MaybeByteArray> AvatarsStorage::GetAvatar (const QString& entryId, IHaveAvatars::Size size)
	{
		if (const auto value = Cache_ [entryId])
			return Util::MakeReadyFuture<MaybeByteArray> (boost::apply_visitor (ToByteArray {}, *value));

		return StorageThread_->GetAvatar (entryId, size);
	}

	QFuture<void> AvatarsStorage::DeleteAvatars (const QString& entryId)
	{
		Cache_.remove (entryId);

		return StorageThread_->DeleteAvatars (entryId);
	}
}
}