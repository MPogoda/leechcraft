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

#include "presencecommand.h"
#include <functional>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/fusion/adapted.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <QList>
#include <interfaces/azoth/iprovidecommands.h>
#include <interfaces/azoth/iaccount.h>
#include <interfaces/azoth/iproxyobject.h>
#include <interfaces/azoth/ihavedirectedstatus.h>

namespace LeechCraft
{
namespace Azoth
{
namespace MuCommands
{
	namespace
	{
		QList<IAccount*> FindAccounts (const QString& name, IProxyObject *proxy, ICLEntry *entry)
		{
			if (name == "$")
				return { qobject_cast<IAccount*> (entry->GetParentAccount ()) };

			QList<IAccount*> allAccs;
			for (const auto accObj : proxy->GetAllAccounts ())
				allAccs << qobject_cast<IAccount*> (accObj);

			if (name == "*")
				return allAccs;

			for (const auto acc : allAccs)
				if (acc->GetAccountName () == name)
					return { acc };

			return {};
		}

		State String2State (const QString& str)
		{
			if (str.startsWith ("avail"))
				return SOnline;
			else if (str == "away")
				return SAway;
			else if (str == "xa")
				return SXA;
			else if (str == "dnd")
				return SDND;
			else if (str == "chat")
				return SChat;

			throw CommandException { QObject::tr ("Unknown status %1.")
						.arg ("<em>" + str + "</em>") };
		}

		std::function<EntryStatus (EntryStatus)> GetStatusMangler (IProxyObject *proxy, const QString& text)
		{
			const auto& action = text.section ('\n', 1, 1).trimmed ();
			const auto& message = text.section ('\n', 2).trimmed ();
			if (action == "clear")
				return [] (const EntryStatus& status)
						{ return EntryStatus { status.State_, {} }; };
			else if (action == "message")
				return [message] (const EntryStatus& status)
						{ return EntryStatus { status.State_, message }; };
			else if (const auto status = proxy->FindCustomStatus (action))
				return [status, message] (const EntryStatus&)
						{ return EntryStatus { status->State_, status->Text_}; };
			else
			{
				const auto state = String2State (action);
				return [state, message] (const EntryStatus&)
						{ return EntryStatus { state, message }; };
			}
		}

		struct AllAccounts {};
		bool operator== (const AllAccounts&, const AllAccounts&) { return true; }

		struct CurrentAccount {};
		bool operator== (const CurrentAccount&, const CurrentAccount&) { return true; }

		typedef boost::variant<AllAccounts, std::string, CurrentAccount> AccName_t;

		typedef boost::variant<State, std::string> State_t;
		typedef std::pair<State, std::string> FullState_t;
		typedef boost::variant<FullState_t, State_t> Status_t;

		struct PresenceParams
		{
			AccName_t AccName_;
			Status_t Status_;
		};
	}
}
}
}

BOOST_FUSION_ADAPT_STRUCT (LeechCraft::Azoth::MuCommands::PresenceParams,
		(LeechCraft::Azoth::MuCommands::AccName_t, AccName_)
		(LeechCraft::Azoth::MuCommands::Status_t, Status_))

namespace LeechCraft
{
namespace Azoth
{
namespace MuCommands
{
	namespace
	{
		namespace ascii = boost::spirit::ascii;
		namespace qi = boost::spirit::qi;
		namespace phoenix = boost::phoenix;

		template<typename Iter>
		struct Parser : qi::grammar<Iter, PresenceParams ()>
		{
			qi::rule<Iter, PresenceParams ()> Start_;

			qi::rule<Iter, AllAccounts ()> AllAccounts_;
			qi::rule<Iter, CurrentAccount ()> CurAcc_;
			qi::rule<Iter, AccName_t ()> AccName_;

			qi::rule<Iter, State_t ()> State_;
			qi::rule<Iter, FullState_t ()> FullState_;
			qi::rule<Iter, Status_t ()> Status_;

			struct StateSymbs : qi::symbols<char, State>
			{
				StateSymbs ()
				{
					add ("avail", SOnline)
						("away", SAway)
						("xa", SXA)
						("dnd", SDND)
						("chat", SChat);
				}
			} PredefinedState_;

			Parser ()
			: Parser::base_type (Start_)
			{
				AllAccounts_ = qi::lit ('*') > qi::attr (AllAccounts ());
				CurAcc_ = qi::eps > qi::attr (CurrentAccount ());
				AccName_ = AllAccounts_ | +(qi::char_ - '\n') | CurAcc_;

				State_ = PredefinedState_ | +(qi::char_ - '\n');
				FullState_ = PredefinedState_ >> '\n' >> +qi::char_;
				Status_ = FullState_ | State_;

				Start_ = AccName_ >> '\n' >> Status_;
			}
		};

		template<typename Iter>
		PresenceParams ParsePresenceCommand (Iter begin, Iter end)
		{
			PresenceParams res;
			qi::parse (begin, end, Parser<Iter> (), res);
			return res;
		}

		PresenceParams ParsePresenceCommand (QString cmd)
		{
			cmd = cmd.trimmed ();
			cmd = cmd.mid (QString { "/presence" }.size ());
			if (cmd.startsWith (' '))
				cmd = cmd.mid (1);
			const auto& unicode = cmd.toUtf8 ();
			return ParsePresenceCommand (unicode.begin (), unicode.end ());
		}
	}

	bool SetPresence (IProxyObject *proxy, ICLEntry *entry, const QString& text)
	{
		const auto& accName = text.section ('\n', 0, 0).section (' ', 1).trimmed ();
		const auto& accs = FindAccounts (accName, proxy, entry);
		if (accs.isEmpty ())
			throw CommandException { QObject::tr ("Unable to find account %1.")
						.arg ("<em>" + accName + "</em>") };

		const auto& statusSetter = GetStatusMangler (proxy, text);
		for (const auto acc : accs)
			acc->ChangeState (statusSetter (acc->GetState ()));

		return true;
	}

	bool SetDirectedPresence (IProxyObject *proxy, ICLEntry *entry, const QString& text)
	{
		const auto ihds = qobject_cast<IHaveDirectedStatus*> (entry->GetQObject ());
		if (!ihds)
			throw CommandException { QObject::tr ("%1 doesn't support directed presence.")
						.arg (entry->GetEntryName ()) };

		const auto& variant = text.section ('\n', 0, 0).section (' ', 1).trimmed ();

		const auto acc = qobject_cast<IAccount*> (entry->GetParentAccount ());
		const auto& newStatus = GetStatusMangler (proxy, text) (acc->GetState ());

		if (!variant.isEmpty ())
			ihds->SendDirectedStatus (newStatus, variant);
		else
			for (const auto& var : entry->Variants ())
				ihds->SendDirectedStatus (newStatus, var);

		return true;
	}
}
}
}
