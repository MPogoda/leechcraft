/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "vkentry.h"
#include <QStringList>
#include <QtDebug>
#include <QTimer>
#include <interfaces/azoth/azothutil.h>
#include "vkaccount.h"
#include "vkmessage.h"
#include "vkconnection.h"
#include "photostorage.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Murm
{
	VkEntry::VkEntry (const UserInfo& info, VkAccount *account)
	: QObject (account)
	, Account_ (account)
	, Info_ (info)
	, RemoteTypingTimer_ (new QTimer (this))
	, LocalTypingTimer_ (new QTimer (this))
	{
		RemoteTypingTimer_->setInterval (6000);
		connect (RemoteTypingTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (handleTypingTimeout ()));

		LocalTypingTimer_->setInterval (5000);
		connect (LocalTypingTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (sendTyping ()));

		auto storage = account->GetPhotoStorage ();
		Avatar_ = storage->GetImage (Info_.Photo_);
		if (Avatar_.isNull () && Info_.Photo_.isValid ())
			connect (storage,
					SIGNAL (gotImage (QUrl)),
					this,
					SLOT (handleGotStorageImage (QUrl)));
	}

	void VkEntry::UpdateInfo (const UserInfo& info)
	{
		const bool updateStatus = info.IsOnline_ != Info_.IsOnline_;
		Info_ = info;

		if (updateStatus)
		{
			emit statusChanged (GetStatus (""), "");
			emit availableVariantsChanged (Variants ());
		}
	}

	const UserInfo& VkEntry::GetInfo () const
	{
		return Info_;
	}

	void VkEntry::Send (VkMessage *msg)
	{
		Account_->Send (this, msg);
	}

	void VkEntry::Store (VkMessage *msg)
	{
		Messages_ << msg;
		emit gotMessage (msg);
	}

	VkMessage* VkEntry::FindMessage (qulonglong id) const
	{
		const auto pos = std::find_if (Messages_.begin (), Messages_.end (),
				[id] (VkMessage *msg) { return msg->GetID () == id; });
		return pos == Messages_.end () ? nullptr : *pos;
	}

	void VkEntry::HandleMessage (const MessageInfo& info)
	{
		if (FindMessage (info.ID_))
			return;

		const auto dir = info.Flags_ & MessageFlag::Outbox ?
				IMessage::DOut :
				IMessage::DIn;

		if (dir == IMessage::DIn)
		{
			emit chatPartStateChanged (CPSActive, "");
			RemoteTypingTimer_->stop ();
			HasUnread_ = true;
		}

		auto msg = new VkMessage (dir, IMessage::MTChatMessage, this);
		msg->SetBody (info.Text_);
		msg->SetDateTime (info.TS_);
		msg->SetID (info.ID_);
		Store (msg);
	}

	void VkEntry::HandleTypingNotification ()
	{
		emit chatPartStateChanged (CPSComposing, "");
		RemoteTypingTimer_->start ();
	}

	QObject* VkEntry::GetQObject ()
	{
		return this;
	}

	QObject* VkEntry::GetParentAccount () const
	{
		return Account_;
	}

	ICLEntry::Features VkEntry::GetEntryFeatures () const
	{
		return FPermanentEntry;
	}

	ICLEntry::EntryType VkEntry::GetEntryType () const
	{
		return ICLEntry::ETChat;
	}

	QString VkEntry::GetEntryName () const
	{
		QStringList components { Info_.FirstName_, Info_.Nick_, Info_.LastName_ };
		components.removeAll ({});
		return components.join (" ");
	}

	void VkEntry::SetEntryName (const QString&)
	{
	}

	QString VkEntry::GetEntryID () const
	{
		return Account_->GetAccountID () + QString::number (Info_.ID_);
	}

	QString VkEntry::GetHumanReadableID () const
	{
		return QString::number (Info_.ID_);
	}

	QStringList VkEntry::Groups () const
	{
		return {};
	}

	void VkEntry::SetGroups (const QStringList&)
	{
	}

	QStringList VkEntry::Variants () const
	{
		return Info_.IsOnline_ ? QStringList ("") : QStringList ();
	}

	QObject* VkEntry::CreateMessage (IMessage::MessageType type, const QString&, const QString& body)
	{
		auto msg = new VkMessage (IMessage::DOut, type, this);
		msg->SetBody (body);
		return msg;
	}

	QList<QObject*> VkEntry::GetAllMessages () const
	{
		QList<QObject*> result;
		for (auto obj : Messages_)
			result << obj;
		return result;
	}

	void VkEntry::PurgeMessages (const QDateTime& before)
	{
		Util::StandardPurgeMessages (Messages_, before);
	}

	void VkEntry::SetChatPartState (ChatPartState state, const QString&)
	{
		if (state == CPSComposing)
		{
			if (!LocalTypingTimer_->isActive ())
			{
				sendTyping ();
				LocalTypingTimer_->start ();
			}
		}
		else
			LocalTypingTimer_->stop ();
	}

	EntryStatus VkEntry::GetStatus (const QString&) const
	{
		return { Info_.IsOnline_ ? SOnline : SOffline, {} };
	}

	QImage VkEntry::GetAvatar () const
	{
		return Avatar_;
	}

	QString VkEntry::GetRawInfo () const
	{
		return {};
	}

	void VkEntry::ShowInfo ()
	{
	}

	QList<QAction*> VkEntry::GetActions () const
	{
		return {};
	}

	QMap<QString, QVariant> VkEntry::GetClientInfo (const QString&) const
	{
		return {};
	}

	void VkEntry::MarkMsgsRead ()
	{
		if (!HasUnread_)
			return;

		QList<qulonglong> ids;
		for (auto msg : Messages_)
			if (!msg->IsRead ())
			{
				ids << msg->GetID ();
				msg->SetRead ();
			}

		HasUnread_ = false;

		if (!ids.isEmpty ())
			Account_->GetConnection ()->MarkAsRead (ids);
	}

	void VkEntry::ChatTabClosed()
	{
	}

	void VkEntry::handleTypingTimeout ()
	{
		emit chatPartStateChanged (CPSPaused, "");
	}

	void VkEntry::sendTyping ()
	{
		Account_->GetConnection ()->SendTyping (Info_.ID_);
	}

	void VkEntry::handleGotStorageImage (const QUrl& url)
	{
		if (url != Info_.Photo_)
			return;

		disconnect (sender (),
				0,
				this,
				0);

		Avatar_ = Account_->GetPhotoStorage ()->GetImage (url);
		emit avatarChanged (Avatar_);
	}
}
}
}
