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

#include "entrybase.h"
#include <QImage>
#include <QStringList>
#include <QInputDialog>
#include <QtDebug>
#include <QBuffer>
#include <QCryptographicHash>
#include <QXmppVCardIq.h>
#include <QXmppPresence.h>
#include <QXmppClient.h>
#include <QXmppRosterManager.h>
#include <QXmppDiscoveryManager.h>
#include <QXmppGlobal.h>
#include <QXmppEntityTimeIq.h>
#include <QXmppEntityTimeManager.h>
#include <QXmppVersionManager.h>
#include <util/util.h>
#include <util/xpc/util.h>
#include <util/sll/qtutil.h>
#include <util/sll/delayedexecutor.h>
#include <util/threads/futures.h>
#include <interfaces/azoth/iproxyobject.h>
#include <interfaces/azoth/azothutil.h>
#include "glooxmessage.h"
#include "glooxclentry.h"
#include "glooxprotocol.h"
#include "vcarddialog.h"
#include "glooxaccount.h"
#include "clientconnection.h"
#include "util.h"
#include "core.h"
#include "capsmanager.h"
#include "useractivity.h"
#include "usermood.h"
#include "usertune.h"
#include "userlocation.h"
#include "pepmicroblog.h"
#include "adhoccommandmanager.h"
#include "executecommanddialog.h"
#include "roomclentry.h"
#include "roomhandler.h"
#include "useravatardata.h"
#include "useravatarmetadata.h"
#include "capsdatabase.h"
#include "avatarsstorage.h"
#include "inforequestpolicymanager.h"
#include "pingmanager.h"
#include "pingreplyobject.h"
#include "pendingversionquery.h"
#include "discomanagerwrapper.h"
#include "vcardstorage.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	EntryBase::EntryBase (const QString& humanReadableId, GlooxAccount *parent)
	: QObject (parent)
	, Account_ (parent)
	, HumanReadableId_ (humanReadableId)
	, Commands_ (new QAction (tr ("Commands..."), this))
	, DetectNick_ (new QAction (tr ("Detect nick"), this))
	, StdSep_ (Util::CreateSeparator (this))
	, VCardPhotoHash_ (parent->GetParentProtocol ()->GetVCardStorage ()->
				GetVCardPhotoHash (humanReadableId).get_value_or ({}))
	{
		connect (this,
				SIGNAL (locationChanged (const QString&, QObject*)),
				parent,
				SIGNAL (geolocationInfoChanged (const QString&, QObject*)));

		connect (Commands_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleCommands ()));
		connect (DetectNick_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleDetectNick ()));
	}

	EntryBase::~EntryBase ()
	{
		qDeleteAll (AllMessages_);
		qDeleteAll (Actions_);
		delete VCardDialog_;
	}

	QObject* EntryBase::GetQObject ()
	{
		return this;
	}

	QList<IMessage*> EntryBase::GetAllMessages () const
	{
		QList<IMessage*> result;
		std::copy (AllMessages_.begin (), AllMessages_.end (), std::back_inserter (result));
		return result;
	}

	void EntryBase::PurgeMessages (const QDateTime& before)
	{
		AzothUtil::StandardPurgeMessages (AllMessages_, before);
	}

	namespace
	{
		bool CheckPartFeature (EntryBase *base, const QString& variant, CapsDatabase *capsDB)
		{
			return XooxUtil::CheckUserFeature (base,
					variant,
					"http://jabber.org/protocol/chatstates",
					capsDB);
		}
	}

	void EntryBase::SetChatPartState (ChatPartState state, const QString& variant)
	{
		if (!CheckPartFeature (this, variant, Account_->GetParentProtocol ()->GetCapsDatabase ()))
			return;

		QXmppMessage msg;
		msg.setTo (GetJID () + (variant.isEmpty () ?
						QString () :
						('/' + variant)));
		msg.setState (static_cast<QXmppMessage::State> (state));
		Account_->GetClientConnection ()->
				GetClient ()->sendPacket (msg);
	}

	EntryStatus EntryBase::GetStatus (const QString& variant) const
	{
		if (!variant.isEmpty () &&
				CurrentStatus_.contains (variant))
			return CurrentStatus_ [variant];

		if (CurrentStatus_.size ())
			return *CurrentStatus_.begin ();

		return EntryStatus ();
	}

	QList<QAction*> EntryBase::GetActions () const
	{
		QList<QAction*> additional;
		additional << Commands_;
		if (GetEntryFeatures () & FSupportsRenames)
			additional << DetectNick_;
		additional << StdSep_;

		return additional + Actions_;
	}

	QImage EntryBase::GetAvatar () const
	{
		return {};
	}

	QString EntryBase::GetHumanReadableID () const
	{
		return HumanReadableId_;
	}

	void EntryBase::ShowInfo ()
	{
		if (!VCardDialog_)
		{
			VCardDialog_ = new VCardDialog (this);
			VCardDialog_->setAttribute (Qt::WA_DeleteOnClose);
			VCardDialog_->UpdateInfo (GetVCard ());
		}
		VCardDialog_->show ();

		if (Account_->GetState ().State_ == SOffline)
		{
			Entity e = LeechCraft::Util::MakeNotification ("Azoth",
					tr ("Can't view info while offline"),
					PCritical_);
			Core::Instance ().SendEntity (e);

			return;
		}

		const auto& ptr = VCardDialog_;
		Account_->GetClientConnection ()->FetchVCard (GetJID (),
				[ptr] (const QXmppVCardIq& iq)
				{
					if (ptr)
						ptr->UpdateInfo (iq);
				},
				true);
	}

	QMap<QString, QVariant> EntryBase::GetClientInfo (const QString& var) const
	{
		auto res = Variant2ClientInfo_ [var];

		if (Variant2SecsDiff_.contains (var))
		{
			auto now = QDateTime::currentDateTimeUtc ();
			now.setTimeSpec (Qt::LocalTime);
			const auto& secsDiff = Variant2SecsDiff_.value (var);
			res ["client_time"] = now
					.addSecs (secsDiff.Diff_)
					.addSecs (secsDiff.Tzo_);
			res ["client_tzo"] = secsDiff.Tzo_;
		}

		const auto& version = Variant2Version_ [var];
		if (version.name ().isEmpty ())
			return res;

		res ["client_remote_name"] = version.name ();
		if (!version.version ().isEmpty ())
			res ["client_version"] = version.version ();
		if (!version.os ().isEmpty ())
			res ["client_os"] = version.os ();
		if (res ["client_name"].toString ().isEmpty ())
			res ["client_name"] = version.name ();

		return res;
	}

	void EntryBase::MarkMsgsRead ()
	{
		HasUnreadMsgs_ = false;
		UnreadMessages_.clear ();

		Account_->GetParentProtocol ()->GetProxyObject ()->MarkMessagesAsRead (this);
	}

	void EntryBase::ChatTabClosed ()
	{
		emit chatTabClosed ();
	}

	IAdvancedCLEntry::AdvancedFeatures EntryBase::GetAdvancedFeatures () const
	{
		return AFSupportsAttention;
	}

	void EntryBase::DrawAttention (const QString& text, const QString& variant)
	{
		const QString& to = variant.isEmpty () ?
				GetJID () :
				GetJID () + '/' + variant;
		QXmppMessage msg;
		msg.setBody (text);
		msg.setTo (to);
		msg.setType (QXmppMessage::Headline);
		msg.setAttentionRequested (true);
		Account_->GetClientConnection ()->GetClient ()->sendPacket (msg);
	}

	QVariant EntryBase::GetMetaInfo (DataField field) const
	{
		switch (field)
		{
		case DataField::BirthDate:
			return GetVCard ().birthday ();
		}

		qWarning () << Q_FUNC_INFO
				<< "unknown data field"
				<< static_cast<int> (field);

		return QVariant ();
	}

	QList<QPair<QString, QVariant>> EntryBase::GetVCardRepresentation () const
	{
		Account_->GetClientConnection ()->FetchVCard (GetJID ());

		const auto vcard = GetVCard ();

		QList<QPair<QString, QVariant>> result
		{
			{ tr ("Photo"), QImage::fromData (vcard.photo ()) },
			{ "JID", vcard.from () },
			{ tr ("Real name"), vcard.fullName () },
			{ tr ("Birthday"), vcard.birthday () },
			{ "URL", vcard.url () },
			{ tr ("About"), vcard.description () }
		};

		for (const auto& phone : vcard.phones ())
		{
			if (phone.number ().isEmpty ())
				continue;

			QStringList attrs;
			if (phone.type () & QXmppVCardPhone::Preferred)
				attrs << tr ("preferred");
			if (phone.type () & QXmppVCardPhone::Home)
				attrs << tr ("home");
			if (phone.type () & QXmppVCardPhone::Work)
				attrs << tr ("work");
			if (phone.type () & QXmppVCardPhone::Cell)
				attrs << tr ("cell");

			result.append ({ tr ("Phone"), attrs.isEmpty () ?
						phone.number () :
						phone.number () + " (" + attrs.join (", ") + ")" });
		}

		for (const auto& email : vcard.emails ())
		{
			if (email.address ().isEmpty ())
				continue;

			QStringList attrs;
			if (email.type () == QXmppVCardEmail::Preferred)
				attrs << tr ("preferred");
			if (email.type () == QXmppVCardEmail::Home)
				attrs << tr ("home");
			if (email.type () == QXmppVCardEmail::Work)
				attrs << tr ("work");
			if (email.type () == QXmppVCardEmail::X400)
				attrs << "X400";

			result.append ({ "Email", attrs.isEmpty () ?
						email.address () :
						email.address () + " (" + attrs.join (", ") + ")" });
		}

		for (const auto& address : vcard.addresses ())
		{
			if ((address.country () + address.locality () + address.postcode () +
					address.region () + address.street ()).isEmpty ())
				continue;

			QStringList attrs;
			if (address.type () & QXmppVCardAddress::Home)
				attrs << tr ("home");
			if (address.type () & QXmppVCardAddress::Work)
				attrs << tr ("work");
			if (address.type () & QXmppVCardAddress::Postal)
				attrs << tr ("postal");
			if (address.type () & QXmppVCardAddress::Preferred)
				attrs << tr ("preferred");

			QString str;
			QStringList fields;
			auto addField = [&fields] (const QString& label, const QString& val)
			{
				if (!val.isEmpty ())
					fields << label.arg (val);
			};
			addField (tr ("Country: %1"), address.country ());
			addField (tr ("Region: %1"), address.region ());
			addField (tr ("Locality: %1", "User's locality"), address.locality ());
			addField (tr ("Street: %1"), address.street ());
			addField (tr ("Postal code: %1"), address.postcode ());

			result.append ({ tr ("Address"), fields });
		}

#if QXMPP_VERSION >= 0x000800
		const auto& orgInfo = vcard.organization ();
		result.append ({ tr ("Organization"), orgInfo.organization () });
		result.append ({ tr ("Organization unit"), orgInfo.unit () });
		result.append ({ tr ("Job title"), orgInfo.title () });
		result.append ({ tr ("Job role"), orgInfo.role () });
#endif
		return result;
	}

	bool EntryBase::CanSendDirectedStatusNow (const QString& variant)
	{
		if (variant.isEmpty ())
			return true;

		if (GetStatus (variant).State_ != SOffline)
			return true;

		return false;
	}

	void EntryBase::SendDirectedStatus (const EntryStatus& state, const QString& variant)
	{
		if (!CanSendDirectedStatusNow (variant))
			return;

		auto conn = Account_->GetClientConnection ();

		auto pres = XooxUtil::StatusToPresence (state.State_,
				state.StatusString_, conn->GetLastState ().Priority_);

		QString to = GetJID ();
		if (!variant.isEmpty ())
			to += '/' + variant;
		pres.setTo (to);

		auto discoMgr = conn->GetClient ()->findExtension<QXmppDiscoveryManager> ();
		pres.setCapabilityHash ("sha-1");
		pres.setCapabilityNode (discoMgr->clientCapabilitiesNode ());
		pres.setCapabilityVer (discoMgr->capabilities ().verificationString ());
		conn->GetClient ()->sendPacket (pres);
	}

	void EntryBase::RequestLastPosts (int)
	{
	}

	namespace
	{
		QByteArray GetVCardPhotoHash (const QXmppVCardIq& vcard)
		{
			const auto& photo = vcard.photo ();
			return photo.isEmpty () ?
					QByteArray {} :
					QCryptographicHash::hash (photo, QCryptographicHash::Sha1);
		}
	}

	QFuture<QImage> EntryBase::RefreshAvatar (Size)
	{
		const auto maybeVCard = Account_->GetParentProtocol ()->
				GetVCardStorage ()->GetVCard (GetHumanReadableID ());
		if (maybeVCard && VCardPhotoHash_ == GetVCardPhotoHash (*maybeVCard))
			return Util::MakeReadyFuture (QImage::fromData (maybeVCard->photo ()));

		QFutureInterface<QImage> iface;
		iface.reportStarted ();
		Account_->GetClientConnection ()->FetchVCard (GetJID (),
				[iface] (const QXmppVCardIq& iq) mutable
				{
					const auto& photo = iq.photo ();
					const auto image = photo.isEmpty () ?
							QImage {} :
							QImage::fromData (photo);
					iface.reportFinished (&image);
				});

		return iface.future ();
	}

	bool EntryBase::HasAvatar () const
	{
		return !VCardPhotoHash_.isEmpty ();
	}

	bool EntryBase::SupportsSize (Size size) const
	{
		return size == Size::Full;
	}

	Media::AudioInfo EntryBase::GetUserTune (const QString& variant) const
	{
		return Variant2Audio_ [variant];
	}

	MoodInfo EntryBase::GetUserMood (const QString& variant) const
	{
		return Variant2Mood_ [variant];
	}

	ActivityInfo EntryBase::GetUserActivity (const QString& variant) const
	{
		return Variant2Activity_ [variant];
	}

	void EntryBase::UpdateEntityTime ()
	{
		const auto& now = QDateTime::currentDateTime ();
		if (LastEntityTimeRequest_.isValid () &&
				LastEntityTimeRequest_.secsTo (now) < 60)
			return;

		connect (Account_->GetClientConnection ()->GetEntityTimeManager (),
				SIGNAL (timeReceived (QXmppEntityTimeIq)),
				this,
				SLOT (handleTimeReceived (QXmppEntityTimeIq)),
				Qt::UniqueConnection);

		LastEntityTimeRequest_ = now;

		auto jid = GetJID ();

		auto timeMgr = Account_->GetClientConnection ()->GetEntityTimeManager ();
		if (jid.contains ('/'))
		{
			timeMgr->requestTime (jid);
			return;
		}

		for (const auto& variant : Variants ())
			if (!variant.isEmpty ())
				timeMgr->requestTime (jid + '/' + variant);
	}

	QObject* EntryBase::Ping (const QString& variant)
	{
		auto jid = GetJID ();
		if (!variant.isEmpty ())
			jid += '/' + variant;

		auto reply = new PingReplyObject { this };
		Account_->GetClientConnection ()->GetPingManager ()->Ping (jid,
				[reply] (int msecs) { reply->HandleReply (msecs); });
		return reply;
	}

	QObject* EntryBase::QueryVersion (const QString& variant)
	{
		auto jid = GetJID ();
		if (!variant.isEmpty ())
			jid += '/' + variant;

		const auto vm = Account_->GetClientConnection ()->GetVersionManager ();
		vm->requestVersion (jid);

		return new PendingVersionQuery { vm, jid, this };
	}

	void EntryBase::HandlePresence (const QXmppPresence& pres, const QString& resource)
	{
		SetClientInfo (resource, pres);
		SetStatus (XooxUtil::PresenceToStatus (pres), resource, pres);

		CheckVCardUpdate (pres);
	}

	void EntryBase::HandleMessage (GlooxMessage *msg)
	{
		if (msg->GetMessageType () == IMessage::Type::ChatMessage)
		{
			HasUnreadMsgs_ = true;
			UnreadMessages_ << msg;
		}

		const auto proxy = Account_->GetParentProtocol ()->GetProxyObject ();
		proxy->GetFormatterProxy ().PreprocessMessage (msg);

		AllMessages_ << msg;
		emit gotMessage (msg);
	}

	void EntryBase::HandlePEPEvent (QString variant, PEPEventBase *event)
	{
		const auto& vars = Variants ();
		if (!vars.isEmpty () &&
				(!vars.contains (variant) || variant.isEmpty ()))
			variant = vars.first ();

		if (const auto activity = dynamic_cast<UserActivity*> (event))
			return HandleUserActivity (activity, variant);
		if (const auto mood = dynamic_cast<UserMood*> (event))
			return HandleUserMood (mood, variant);
		if (const auto tune = dynamic_cast<UserTune*> (event))
			return HandleUserTune (tune, variant);

		if (const auto location = dynamic_cast<UserLocation*> (event))
		{
			if (Location_ [variant] == location->GetInfo ())
				return;

			Location_ [variant] = location->GetInfo ();
			emit locationChanged (variant, this);
			emit locationChanged (variant);
			return;
		}

		if (PEPMicroblog *microblog = dynamic_cast<PEPMicroblog*> (event))
		{
			emit gotNewPost (*microblog);
			return;
		}

		if (dynamic_cast<UserAvatarData*> (event) || dynamic_cast<UserAvatarMetadata*> (event))
			return;

		qWarning () << Q_FUNC_INFO
				<< "unhandled PEP event from"
				<< GetJID ();
	}

	void EntryBase::HandleAttentionMessage (const QXmppMessage& msg)
	{
		QString jid;
		QString resource;
		ClientConnection::Split (msg.from (), &jid, &resource);

		emit attentionDrawn (msg.body (), resource);
	}

	void EntryBase::UpdateChatState (QXmppMessage::State state, const QString& variant)
	{
		emit chatPartStateChanged (static_cast<ChatPartState> (state), variant);

		if (state == QXmppMessage::Gone)
		{
			GlooxMessage *msg = new GlooxMessage (IMessage::Type::EventMessage,
					IMessage::Direction::In,
					GetJID (),
					variant,
					Account_->GetClientConnection ().get ());
			msg->SetMessageSubType (IMessage::SubType::ParticipantEndedConversation);
			HandleMessage (msg);
		}
	}

	void EntryBase::SetStatus (const EntryStatus& status, const QString& variant, const QXmppPresence& presence)
	{
		const bool existed = CurrentStatus_.contains (variant);
		const bool wasOffline = existed ?
				CurrentStatus_ [variant].State_ == SOffline :
				false;

		if (existed &&
				status == CurrentStatus_ [variant] &&
				presence.priority () == Variant2ClientInfo_.value (variant).value ("priority"))
			return;

		CurrentStatus_ [variant] = status;

		const QStringList& vars = Variants ();
		if ((!existed || wasOffline) && !vars.isEmpty ())
		{
			const QString& highest = vars.first ();
			if (Location_.contains ({}))
				Location_ [highest] = Location_.take ({});
			if (Variant2Audio_.contains ({}))
				Variant2Audio_ [highest] = Variant2Audio_.take ({});
			if (Variant2Mood_.contains ({}))
				Variant2Mood_ [highest] = Variant2Mood_.take ({});
			if (Variant2Activity_.contains ({}))
				Variant2Activity_ [highest] = Variant2Activity_.take ({});
		}

		if ((!existed || wasOffline) &&
				status.State_ != SOffline)
		{
			auto conn = Account_->GetClientConnection ();
			if (conn->GetInfoReqPolicyManager ()->IsRequestAllowed (InfoRequest::Version, this))
			{
				if (!variant.isEmpty ())
					conn->FetchVersion (GetJID () + '/' + variant);
				else
					conn->FetchVersion (GetJID ());
			}
		}

		if (status.State_ != SOffline)
		{
			if (const int p = presence.priority ())
				Variant2ClientInfo_ [variant] ["priority"] = p;
		}
		else
		{
			Variant2Version_.remove (variant);
			Variant2ClientInfo_.remove (variant);
			Variant2VerString_.remove (variant);
			Variant2Identities_.remove (variant);

			Variant2Audio_.remove (variant);
			Variant2Mood_.remove (variant);
			Variant2Activity_.remove (variant);

			Variant2SecsDiff_.remove (variant);
		}

		emit statusChanged (status, variant);

		if (!existed ||
				(existed && status.State_ == SOffline) ||
				wasOffline)
			emit availableVariantsChanged (vars);

		GlooxMessage *message = 0;
		if (GetEntryType () == EntryType::PrivateChat)
			message = new GlooxMessage (IMessage::Type::StatusMessage,
					IMessage::Direction::In,
					qobject_cast<RoomCLEntry*> (GetParentCLEntryObject ())->
							GetRoomHandler ()->GetRoomJID (),
					GetEntryName (),
					Account_->GetClientConnection ().get ());
		else
			message = new GlooxMessage (IMessage::Type::StatusMessage,
				IMessage::Direction::In,
				GetJID (),
				variant,
				Account_->GetClientConnection ().get ());
		message->SetMessageSubType (IMessage::SubType::ParticipantStatusChange);

		const auto proxy = Account_->GetParentProtocol ()->GetProxyObject ();
		const auto& state = proxy->StateToString (status.State_);

		const auto& nick = GetEntryName () + '/' + variant;
		message->setProperty ("Azoth/Nick", nick);
		message->setProperty ("Azoth/TargetState", state);
		message->setProperty ("Azoth/StatusText", status.StatusString_);

		const auto& msg = tr ("%1 is now %2 (%3)")
				.arg (nick)
				.arg (state)
				.arg (status.StatusString_);
		message->SetBody (msg);
		HandleMessage (message);
	}

	QXmppVCardIq EntryBase::GetVCard () const
	{
		const auto storage = Account_->GetParentProtocol ()->GetVCardStorage ();
		return storage->GetVCard (GetHumanReadableID ()).get_value_or ({});
	}

	void EntryBase::SetVCard (const QXmppVCardIq& vcard)
	{
		if (VCardDialog_)
			VCardDialog_->UpdateInfo (vcard);

		Account_->GetParentProtocol ()->GetVCardStorage ()->SetVCard (GetHumanReadableID (), vcard);

		emit vcardUpdated ();

		const auto& newPhotoHash = GetVCardPhotoHash (vcard);
		if (newPhotoHash != VCardPhotoHash_)
		{
			VCardPhotoHash_ = newPhotoHash;
			WriteDownPhotoHash ();
			emit avatarChanged (this);
		}
	}

	bool EntryBase::HasUnreadMsgs () const
	{
		return HasUnreadMsgs_;
	}

	QList<GlooxMessage*> EntryBase::GetUnreadMessages () const
	{
		return UnreadMessages_;
	}

	void EntryBase::SetClientInfo (const QString& variant,
			const QString& node, const QByteArray& ver)
	{
		QString type = XooxUtil::GetClientIDName (node);
		if (type.isEmpty () && !node.isEmpty ())
			qWarning () << Q_FUNC_INFO
					<< "unknown client type for"
					<< node;
		Variant2ClientInfo_ [variant] ["client_type"] = type;

		QString name = XooxUtil::GetClientHRName (node);
		if (name.isEmpty () && !node.isEmpty ())
			qWarning () << Q_FUNC_INFO
					<< "unknown client name for"
					<< node;
		Variant2ClientInfo_ [variant] ["client_name"] = name;
		Variant2ClientInfo_ [variant] ["raw_client_name"] = name;

		Variant2VerString_ [variant] = ver;

		QString reqJid = GetJID ();
		QString reqVar = "";
		if (GetEntryType () == EntryType::Chat)
		{
			reqJid = variant.isEmpty () ?
					reqJid :
					reqJid + '/' + variant;
			reqVar = variant;
		}

		auto capsManager = Account_->GetClientConnection ()->GetCapsManager ();
		const auto& storedIds = capsManager->GetIdentities (ver);

		if (!storedIds.isEmpty ())
			SetDiscoIdentities (reqVar, storedIds);
		else
		{
			qDebug () << "requesting ids for" << reqJid << reqVar;
			QPointer<EntryBase> pThis (this);
			QPointer<CapsManager> pCM (capsManager);
			Account_->GetClientConnection ()->GetDiscoManagerWrapper ()->RequestInfo (reqJid,
				[ver, reqVar, pThis, pCM] (const QXmppDiscoveryIq& iq)
				{
					if (!ver.isEmpty () && pCM)
						pCM->SetIdentities (ver, iq.identities ());
					if (pThis)
						pThis->SetDiscoIdentities (reqVar, iq.identities ());
				});
		}
	}

	void EntryBase::SetClientInfo (const QString& variant, const QXmppPresence& pres)
	{
		if (pres.type () == QXmppPresence::Available)
			SetClientInfo (variant, pres.capabilityNode (), pres.capabilityVer ());
	}

	void EntryBase::SetClientVersion (const QString& variant, const QXmppVersionIq& version)
	{
		qDebug () << Q_FUNC_INFO << variant << version.os ();
		Variant2Version_ [variant] = version;

		emit entryGenerallyChanged ();
	}

	void EntryBase::SetDiscoIdentities (const QString& variant, const QList<QXmppDiscoveryIq::Identity>& ids)
	{
		Variant2Identities_ [variant] = ids;

		const QString& name = ids.value (0).name ();
		const QString& type = ids.value (0).type ();
		if (name.contains ("Kopete"))
		{
			Variant2ClientInfo_ [variant] ["client_type"] = "kopete";
			Variant2ClientInfo_ [variant] ["client_name"] = "Kopete";
			Variant2ClientInfo_ [variant] ["raw_client_name"] = "kopete";
			emit statusChanged (GetStatus (variant), variant);
		}
		else if (name.contains ("emacs", Qt::CaseInsensitive) ||
				name.contains ("jabber.el", Qt::CaseInsensitive))
		{
			Variant2ClientInfo_ [variant] ["client_type"] = "jabber.el";
			Variant2ClientInfo_ [variant] ["client_name"] = "Emacs Jabber.El";
			Variant2ClientInfo_ [variant] ["raw_client_name"] = "jabber.el";
			emit statusChanged (GetStatus (variant), variant);
		}
		else if (type == "mrim")
		{
			Variant2ClientInfo_ [variant] ["client_type"] = "mailruagent";
			Variant2ClientInfo_ [variant] ["client_name"] = "Mail.Ru Agent Gateway";
			Variant2ClientInfo_ [variant] ["raw_client_name"] = "mailruagent";
			emit statusChanged (GetStatus (variant), variant);
		}
	}

	GeolocationInfo_t EntryBase::GetGeolocationInfo (const QString& variant) const
	{
		return Location_ [variant];
	}

	QByteArray EntryBase::GetVariantVerString (const QString& var) const
	{
		return Variant2VerString_ [var];
	}

	QXmppVersionIq EntryBase::GetClientVersion (const QString& var) const
	{
		return Variant2Version_ [var];
	}

	void EntryBase::HandleUserActivity (const UserActivity *activity, const QString& variant)
	{
		if (activity->GetGeneral () == UserActivity::GeneralEmpty)
		{
			if (!Variant2Activity_.remove (variant))
				return;
		}
		else
		{
			const ActivityInfo info
			{
				activity->GetGeneralStr (),
				activity->GetSpecificStr (),
				activity->GetText ()
			};
			if (Variant2Activity_ [variant] == info)
				return;

			Variant2Activity_ [variant] = info;
		}

		emit activityChanged (variant);
	}

	void EntryBase::HandleUserMood (const UserMood *mood, const QString& variant)
	{
		if (mood->GetMood () == UserMood::MoodEmpty)
		{
			if (!Variant2Mood_.remove (variant))
				return;
		}
		else
		{
			const MoodInfo info
			{
				mood->GetMoodStr (),
				mood->GetText ()
			};
			if (Variant2Mood_ [variant] == info)
				return;

			Variant2Mood_ [variant] = info;
		}

		emit moodChanged (variant);
	}

	void EntryBase::HandleUserTune (const UserTune *tune, const QString& variant)
	{
		if (tune->IsNull ())
		{
			if (!Variant2Audio_.remove (variant))
				return;
		}
		else
		{
			const auto& audioInfo = tune->ToAudioInfo ();
			if (Variant2Audio_ [variant] == audioInfo)
				return;

			Variant2Audio_ [variant] = audioInfo;
		}

		emit tuneChanged (variant);
	}

	void EntryBase::CheckVCardUpdate (const QXmppPresence& pres)
	{
		auto conn = Account_->GetClientConnection ();
		if (!conn->GetInfoReqPolicyManager ()->IsRequestAllowed (InfoRequest::VCard, this))
			return;

		const auto& vcardUpdate = pres.vCardUpdateType ();
		if (vcardUpdate == QXmppPresence::VCardUpdateNoPhoto)
		{
			if (!VCardPhotoHash_.isEmpty ())
			{
				VCardPhotoHash_.clear ();
				WriteDownPhotoHash ();
				emit avatarChanged (this);
			}
		}
		else if (vcardUpdate == QXmppPresence::VCardUpdateValidPhoto)
		{
			if (pres.photoHash () != VCardPhotoHash_)
			{
				VCardPhotoHash_ = pres.photoHash ();
				WriteDownPhotoHash ();
				emit avatarChanged (this);
			}
		}
		else if (pres.type () == QXmppPresence::Available && !HasBlindlyRequestedVCard_)
		{
			// TODO check if this branch is needed
			QPointer<EntryBase> ptr (this);
			conn->FetchVCard (GetJID (),
					[ptr] (const QXmppVCardIq& iq) { if (ptr) ptr->SetVCard (iq); });
			HasBlindlyRequestedVCard_ = true;
		}
	}

	void EntryBase::SetNickFromVCard (const QXmppVCardIq& vcard)
	{
		if (!vcard.nickName ().isEmpty ())
		{
			SetEntryName (vcard.nickName ());
			return;
		}

		if (!vcard.fullName ().isEmpty ())
		{
			SetEntryName (vcard.fullName ());
			return;
		}

		const QString& fn = vcard.firstName ();
		const QString& mn = vcard.middleName ();
		const QString& ln = vcard.lastName ();
		QString name = fn;
		if (!fn.isEmpty ())
			name += " ";
		name += mn;
		if (!mn.isEmpty ())
			name += " ";
		name += ln;
		name = name.trimmed ();
		if (!name.isEmpty ())
			SetEntryName (name);
	}

	void EntryBase::WriteDownPhotoHash () const
	{
		const auto vcardStorage = Account_->GetParentProtocol ()->GetVCardStorage ();
		vcardStorage->SetVCardPhotoHash (GetHumanReadableID (), VCardPhotoHash_);
	}

	void EntryBase::handleTimeReceived (const QXmppEntityTimeIq& iq)
	{
		const auto& from = iq.from ();
		if (!from.startsWith (GetJID ()))
			return;

		const auto& thatTime = iq.utc ();
		if (!thatTime.isValid ())
			return;

		QString bare;
		QString variant;
		ClientConnection::Split (from, &bare, &variant);

		if (variant.isEmpty () || GetEntryType () == EntryType::PrivateChat)
			variant = "";

		const auto secsDiff = QDateTime::currentDateTimeUtc ().secsTo (thatTime);
		Variant2SecsDiff_ [variant] = { static_cast<int> (secsDiff), iq.tzo () };

		emit entryGenerallyChanged ();

		emit entityTimeUpdated ();
	}

	void EntryBase::handleCommands ()
	{
		auto jid = GetJID ();
		if (GetEntryType () != EntryType::PrivateChat)
		{
			QStringList commandable;
			const auto capsMgr = Account_->GetClientConnection ()->GetCapsManager ();
			for (const auto& pair : Util::Stlize (Variant2VerString_))
			{
				const auto& caps = capsMgr->GetRawCaps (pair.second);
				if (caps.isEmpty () ||
						caps.contains (AdHocCommandManager::GetAdHocFeature ()))
					commandable << pair.first;
			}

			if (commandable.isEmpty ())
				return;
			else if (commandable.size () == 1)
			{
				const auto& var = commandable.first ();
				if (!var.isEmpty ())
					jid += '/' + var;
			}
			else
			{
				bool ok = true;
				const auto& var = QInputDialog::getItem (0,
						tr ("Select resource"),
						tr ("Select resource for which to fetch the commands"),
						commandable,
						0,
						false,
						&ok);
				if (!ok || var.isEmpty ())
					return;

				jid += '/' + var;
			}
		}

		const auto dia = new ExecuteCommandDialog (jid, Account_);
		dia->show ();
	}

	void EntryBase::handleDetectNick ()
	{
		QPointer<EntryBase> ptr (this);
		Account_->GetClientConnection ()->FetchVCard (GetJID (),
				[ptr] (const QXmppVCardIq& iq) { if (ptr) ptr->SetNickFromVCard (iq); });
	}
}
}
}
