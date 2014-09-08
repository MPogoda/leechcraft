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

#include "proxyobject.h"
#include <algorithm>
#ifdef USE_BOOST_LOCALE
#include <boost/locale.hpp>
#endif
#include <QInputDialog>
#include <QtDebug>
#include <util/xpc/util.h>
#include <util/xpc/passutils.h>
#include <util/sys/sysinfo.h>
#include "interfaces/azoth/iaccount.h"
#include "core.h"
#include "xmlsettingsmanager.h"
#include "chattabsmanager.h"
#include "coremessage.h"
#include "unreadqueuemanager.h"
#include "resourcesmanager.h"
#include "util.h"
#include "customstatusesmanager.h"

namespace LeechCraft
{
namespace Azoth
{
	ProxyObject::ProxyObject (QObject* parent)
	: QObject (parent)
	, LinkRegexp_ ("((?:(?:\\w+://)|(?:xmpp:|mailto:|www\\.|magnet:|irc:))[^\\s<]+)",
			Qt::CaseInsensitive)
	{
		SerializedStr2AuthStatus_ ["None"] = ASNone;
		SerializedStr2AuthStatus_ ["To"] = ASTo;
		SerializedStr2AuthStatus_ ["From"] = ASFrom;
		SerializedStr2AuthStatus_ ["Both"] = ASBoth;
	}

	QObject* ProxyObject::GetSettingsManager ()
	{
		return &XmlSettingsManager::Instance ();
	}

	void ProxyObject::SetPassword (const QString& password, QObject *accObj)
	{
		const auto acc = qobject_cast<IAccount*> (accObj);
		const auto& key = "org.LeechCraft.Azoth.PassForAccount/" + acc->GetAccountID ();
		Util::SavePassword (password, key, Core::Instance ().GetProxy ());
	}

	QString ProxyObject::GetAccountPassword (QObject *accObj, bool useStored)
	{
		const auto acc = qobject_cast<IAccount*> (accObj);
		const auto& key = "org.LeechCraft.Azoth.PassForAccount/" + acc->GetAccountID ();
		return Util::GetPassword (key,
				tr ("Enter password for %1:")
						.arg (acc->GetAccountName ()),
				Core::Instance ().GetProxy (),
				useStored);
	}

	bool ProxyObject::IsAutojoinAllowed ()
	{
		return XmlSettingsManager::Instance ()
				.property ("IsAutojoinAllowed").toBool ();
	}

	QString ProxyObject::StateToString (State st) const
	{
		switch (st)
		{
		case SOnline:
			return Core::tr ("Online");
		case SChat:
			return Core::tr ("Free to chat");
		case SAway:
			return Core::tr ("Away");
		case SDND:
			return Core::tr ("Do not disturb");
		case SXA:
			return Core::tr ("Not available");
		case SOffline:
			return Core::tr ("Offline");
		default:
			return Core::tr ("Error");
		}
	}

	QString ProxyObject::AuthStatusToString (AuthStatus status) const
	{
		switch (status)
		{
		case ASNone:
			return "None";
		case ASTo:
			return "To";
		case ASFrom:
			return "From";
		case ASBoth:
			return "Both";
		case ASContactRequested:
			return "Requested";
		default:
			qWarning () << Q_FUNC_INFO
					<< "unknown status"
					<< status;
			return "Unknown";
		}
	}

	AuthStatus ProxyObject::AuthStatusFromString (const QString& str) const
	{
		return SerializedStr2AuthStatus_.value (str, ASNone);;
	}

	QObject* ProxyObject::GetAccount (const QString& accID) const
	{
		Q_FOREACH (IAccount *acc, Core::Instance ().GetAccounts ())
			if (acc->GetAccountID () == accID)
				return acc->GetQObject ();

		return 0;
	}

	QList<QObject*> ProxyObject::GetAllAccounts () const
	{
		QList<QObject*> result;
		Q_FOREACH (IAccount *acc, Core::Instance ().GetAccounts ())
			result << acc->GetQObject ();
		return result;
	}

	QObject* ProxyObject::GetEntry (const QString& entryID, const QString&) const
	{
		return Core::Instance ().GetEntry (entryID);
	}

	void ProxyObject::OpenChat (const QString& entryID, const QString& accID,
			const QString& message, const QString& variant) const
	{
		ChatTabsManager *mgr = Core::Instance ().GetChatTabsManager ();

		ICLEntry *entry = qobject_cast<ICLEntry*> (GetEntry (entryID, accID));
		QWidget *chat = mgr->OpenChat (entry, true);

		QMetaObject::invokeMethod (chat,
				"prepareMessageText",
				Qt::QueuedConnection,
				Q_ARG (QString, message));
		QMetaObject::invokeMethod (chat,
				"selectVariant",
				Qt::QueuedConnection,
				Q_ARG (QString, variant));
	}

	QString ProxyObject::GetSelectedChatTemplate (QObject *entry, QWebFrame *frame) const
	{
		return Core::Instance ().GetSelectedChatTemplate (entry, frame);
	}

	QList<QColor> ProxyObject::GenerateColors (const QString& scheme, QColor bg) const
	{
		return Azoth::GenerateColors (scheme, bg);
	}

	QString ProxyObject::GetNickColor (const QString& nick, const QList<QColor>& colors) const
	{
		return Azoth::GetNickColor (nick, colors);
	}

	QString ProxyObject::FormatDate (QDateTime dt, QObject *obj) const
	{
		return Core::Instance ().FormatDate (dt, qobject_cast<IMessage*> (obj));
	}

	QString ProxyObject::FormatNickname (QString nick, QObject *obj, const QString& color) const
	{
		return Core::Instance ().FormatNickname (nick, qobject_cast<IMessage*> (obj), color);
	}

	QString ProxyObject::FormatBody (QString body, QObject *obj) const
	{
		return Core::Instance ().FormatBody (body, qobject_cast<IMessage*> (obj));
	}

	void ProxyObject::PreprocessMessage (QObject *msgObj)
	{
		if (msgObj->property ("Azoth/DoNotPreprocess").toBool ())
			return;

		IMessage *msg = qobject_cast<IMessage*> (msgObj);
		if (!msg)
		{
			qWarning () << Q_FUNC_INFO
					<< "message"
					<< msgObj
					<< "is not an IMessage";
			return;
		}

		switch (msg->GetMessageSubType ())
		{
		case IMessage::SubType::ParticipantStatusChange:
		{
			const QString& nick = msgObj->property ("Azoth/Nick").toString ();
			const QString& state = msgObj->property ("Azoth/TargetState").toString ();
			const QString& text = msgObj->property ("Azoth/StatusText").toString ();
			if (!nick.isEmpty () && !state.isEmpty ())
			{
				const QString& newBody = text.isEmpty () ?
						tr ("%1 changed status to %2")
							.arg (nick)
							.arg (state) :
						tr ("%1 changed status to %2 (%3)")
							.arg (nick)
							.arg (state)
							.arg (text);
				msg->SetBody (newBody);
			}
			break;
		}
		default:
			break;
		}
	}

	Util::ResourceLoader* ProxyObject::GetResourceLoader (IProxyObject::PublicResourceLoader loader) const
	{
		switch (loader)
		{
		case PRLClientIcons:
			return ResourcesManager::Instance ().GetResourceLoader (ResourcesManager::RLTClientIconLoader);
		case PRLStatusIcons:
			return ResourcesManager::Instance ().GetResourceLoader (ResourcesManager::RLTStatusIconLoader);
		case PRLSystemIcons:
			return ResourcesManager::Instance ().GetResourceLoader (ResourcesManager::RLTSystemIconLoader);
		default:
			qWarning () << Q_FUNC_INFO
					<< "unknown type"
					<< loader;
			return 0;
		}
	}

	QIcon ProxyObject::GetIconForState (State state) const
	{
		return ResourcesManager::Instance ().GetIconForState (state);
	}

	const auto MaxBodySize4Links = 10 * 1024;

	void ProxyObject::FormatLinks (QString& body)
	{
		if (body.size () > MaxBodySize4Links)
			return;

		int pos = 0;
		while ((pos = LinkRegexp_.indexIn (body, pos)) != -1)
		{
			const auto& link = LinkRegexp_.cap (1);
			if (pos > 0 &&
					(body.at (pos - 1) == '"' ||
						body.at (pos - 1) == '=' ||
						body.at (pos - 1) == '\''))
			{
				pos += link.size ();
				continue;
			}

			auto trimmed = link.trimmed ();
			if (trimmed.startsWith ("www."))
				trimmed.prepend ("http://");

			auto shortened = trimmed;
			const auto length = XmlSettingsManager::Instance ()
					.property ("ShortenURLLength").toInt ();
			if (shortened.size () > length)
				shortened = trimmed.left (length / 2) + "..." + trimmed.right (length / 2);

			const auto& str = "<a href=\"" + trimmed + "\" title=\"" + trimmed + "\">" + shortened + "</a>";
			body.replace (pos, link.length (), str);

			pos += str.length ();
		}
	}

	QStringList ProxyObject::FindLinks (const QString& body)
	{
		QStringList result;

		if (body.size () > MaxBodySize4Links)
			return result;

		int pos = 0;
		while ((pos = LinkRegexp_.indexIn (body, pos)) != -1)
		{
			const auto& link = LinkRegexp_.cap (1);
			if (pos > 0 &&
					(body.at (pos - 1) == '"' || body.at (pos - 1) == '='))
			{
				pos += link.size ();
				continue;
			}

			result << link.trimmed ();
			pos += link.size ();
		}

		return result;
	}

	QObject* ProxyObject::CreateCoreMessage (const QString& body,
			const QDateTime& date,
			IMessage::Type type, IMessage::Direction dir,
			QObject *other, QObject *parent)
	{
		return new CoreMessage (body, date, type, dir, other, parent);
	}

	QString ProxyObject::ToPlainBody (QString body)
	{
		body.replace ("<li>", "\n * ");
		auto pos = 0;
		while ((pos = body.indexOf ('<', pos)) != -1)
		{
			const auto endPos = body.indexOf ('>', pos + 1);
			body.remove (pos, endPos - pos + 1);
		}

		return body;
	}

	bool ProxyObject::IsMessageRead (QObject *msgObj)
	{
		return Core::Instance ().GetUnreadQueueManager ()->IsMessageRead (msgObj);
	}

	void ProxyObject::MarkMessagesAsRead (QObject *entryObj)
	{
		Core::Instance ().GetUnreadQueueManager ()->clearMessagesForEntry (entryObj);
	}

	QString ProxyObject::PrettyPrintDateTime (const QDateTime& dt)
	{
#ifdef USE_BOOST_LOCALE
		static class LocaleInitializer
		{
		public:
			LocaleInitializer ()
			{
				boost::locale::generator gen;
				std::locale::global (gen (""));
			}
		} loc;

		const auto& cal = dt.timeSpec () == Qt::LocalTime ?
				boost::locale::calendar {} :
				boost::locale::calendar { "GMT" };
		boost::locale::date_time bdt { static_cast<double> (dt.toTime_t ()),  cal };

		std::ostringstream ostr;
		ostr << bdt;

		return QString::fromUtf8 (ostr.str ().c_str ());
#else
		return QLocale {}.toString (dt);
#endif
	}

	boost::optional<CustomStatus> ProxyObject::FindCustomStatus (const QString& name) const
	{
		const auto mgr = Core::Instance ().GetCustomStatusesManager ();
		const auto& statuses = mgr->GetStates ();

		const auto pos = std::find_if (statuses.begin (), statuses.end (),
				[&name] (const CustomStatus& status)
					{ return !QString::compare (status.Name_, name, Qt::CaseInsensitive); });
		if (pos == statuses.end ())
			return {};
		return *pos;
	}

	QStringList ProxyObject::GetCustomStatusNames () const
	{
		const auto mgr = Core::Instance ().GetCustomStatusesManager ();
	
		QStringList result;
		for (const auto& status : mgr->GetStates ())
			result << status.Name_;
		return result;
	}
}
}
