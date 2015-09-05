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

#include "avatarsmanager.h"
#include <util/threads/futures.h>
#include "interfaces/azoth/iaccount.h"
#include "avatarsstorage.h"

namespace LeechCraft
{
namespace Azoth
{
	AvatarsManager::AvatarsManager (QObject *parent)
	: QObject { parent }
	, Storage_ { new AvatarsStorage { this } }
	{
	}

	QFuture<QImage> AvatarsManager::GetAvatar (QObject *entryObj, IHaveAvatars::Size size)
	{
		const auto entry = qobject_cast<ICLEntry*> (entryObj);
		const auto iha = qobject_cast<IHaveAvatars*> (entryObj);
		if (!iha)
			return Util::MakeReadyFuture (entry->GetAvatar ());

		return Util::Sequence (this, [=] { return Storage_->GetAvatar (entry, size); }) >>
				[=] (const MaybeImage& image)
				{
					if (image)
						return Util::MakeReadyFuture (*image);

					const auto& refreshFuture = iha->RefreshAvatar (size);

					Util::Sequence (this, [=] { return refreshFuture; }) >>
							[=] (const QImage& img) { Storage_->SetAvatar (entry, size, img); };

					return refreshFuture;
				};
	}

	bool AvatarsManager::HasAvatar (QObject *entryObj) const
	{
		const auto iha = qobject_cast<IHaveAvatars*> (entryObj);
		return iha ?
				iha->HasAvatar () :
				!qobject_cast<ICLEntry*> (entryObj)->GetAvatar ().isNull ();
	}

	void AvatarsManager::handleAccount (QObject *accObj)
	{
		connect (accObj,
				SIGNAL (gotCLItems (QList<QObject*>)),
				this,
				SLOT (handleEntries (QList<QObject*>)));

		handleEntries (qobject_cast<IAccount*> (accObj)->GetCLEntries ());
	}

	void AvatarsManager::handleEntries (const QList<QObject*>& entries)
	{
		for (const auto entryObj : entries)
		{
			const auto iha = qobject_cast<IHaveAvatars*> (entryObj);
			if (!iha)
				continue;

			connect (entryObj,
					SIGNAL (avatarChanged (QObject*)),
					this,
					SLOT (invalidateAvatar (QObject*)));
		}
	}

	void AvatarsManager::invalidateAvatar (QObject *that)
	{
		const auto entry = qobject_cast<ICLEntry*> (that);
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< "object is not an entry:"
					<< sender ()
					<< that;
			return;
		}

		Storage_->DeleteAvatars (entry->GetEntryID ());
	}
}
}