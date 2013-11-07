/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2012  Oleg Linkin
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

#include "localblogaccount.h"
#include <stdexcept>
#include <QtDebug>
#include "accountconfigurationdialog.h"
#include "accountconfigurationwidget.h"
#include "accountstorage.h"
#include "localbloggingplatform.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Blogique
{
namespace Hestia
{
	LocalBlogAccount::LocalBlogAccount (const QString& name, QObject *parent)
	: QObject (parent)
	, ParentBloggingPlatform_ (qobject_cast<LocalBloggingPlatform*> (parent))
	, Name_ (name)
	, IsValid_ (false)
	, AccountStorage_ (new AccountStorage (this))
	, LoadAllEvents_ (new QAction (tr ("All entries"), this))
	, DefaultPostsNumber_ (20)
	{
		connect (LoadAllEvents_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleLoadAllEvents ()));
	}

	QObject* LocalBlogAccount::GetQObject ()
	{
		return this;
	}

	QObject* LocalBlogAccount::GetParentBloggingPlatform () const
	{
		return ParentBloggingPlatform_;
	}

	QString LocalBlogAccount::GetAccountName () const
	{
		return Name_;
	}

	QString LocalBlogAccount::GetOurLogin () const
	{
		return QString ();
	}

	void LocalBlogAccount::RenameAccount (const QString&)
	{
	}

	QByteArray LocalBlogAccount::GetAccountID () const
	{
		return ParentBloggingPlatform_->GetBloggingPlatformID () + "_" +
				Name_.toUtf8 ();
	}

	void LocalBlogAccount::OpenConfigurationDialog ()
	{
		AccountConfigurationDialog dia;

		if (!DatabasePath_.isEmpty ())
			dia.ConfWidget ()->SetAccountBasePath (DatabasePath_);

		if (dia.exec () == QDialog::Rejected)
			return;

		FillSettings (dia.ConfWidget ());
	}

	bool LocalBlogAccount::IsValid () const
	{
		return IsValid_;
	}

	QObject* LocalBlogAccount::GetProfile ()
	{
		return 0;
	}

	void LocalBlogAccount::RemoveEntry (const Entry& entry)
	{
		try
		{
			AccountStorage_->RemoveEntry (entry.EntryId_);
			emit entryRemoved (entry.EntryId_);
			emit requestEntriesBegin ();
			handleLoadAllEvents ();
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

	void LocalBlogAccount::UpdateEntry (const Entry& entry)
	{
		try
		{
			AccountStorage_->UpdateEntry (entry, entry.EntryId_);
			emit entryUpdated ({ entry });
			emit requestEntriesBegin ();
			handleLoadAllEvents ();
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

	QList<QAction*> LocalBlogAccount::GetUpdateActions () const
	{
		return { LoadAllEvents_ };
	}

	void LocalBlogAccount::RequestLastEntries (int count)
	{
		try
		{
			emit requestEntriesBegin ();
			emit gotEntries (AccountStorage_->GetLastEntries (AccountStorage::Mode::FullMode,
					count ? count : DefaultPostsNumber_));
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

	void LocalBlogAccount::RequestStatistics ()
	{
		try
		{
			emit gotBlogStatistics (AccountStorage_->GetEntriesCountByDate ());
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

	void LocalBlogAccount::RequestTags ()
	{
		emit tagsUpdated (GetTags ());
	}

	void LocalBlogAccount::GetEntriesByDate (const QDate& date)
	{
		try
		{
			emit gotEntries (AccountStorage_->GetEntriesByDate (date));
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

	void LocalBlogAccount::GetEntriesWithFilter (const Filter& filter)
	{
		try
		{
			emit gotFilteredEntries (AccountStorage_->GetEntriesWithFilter (filter));
			emit gettingFilteredEntriesFinished ();
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

	void LocalBlogAccount::RequestRecentComments ()
	{
	}

	QHash<QString, int> LocalBlogAccount::GetTags () const
	{
		try
		{
			return AccountStorage_->GetAllTags ();
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
			return QHash<QString, int> ();
		}
	}

	void LocalBlogAccount::FillSettings (AccountConfigurationWidget *widget)
	{
		DatabasePath_ = widget->GetAccountBasePath ();
		if (DatabasePath_.isEmpty ())
			return;

		if (widget->GetOption () & IBloggingPlatform::AAORegisterNewAccount)
		{
			IsValid_ = true;
			AccountStorage_->Init (DatabasePath_);
			emit accountValidated (IsValid_);
		}
		else
			Validate ();
	}

	void LocalBlogAccount::Init ()
	{
		connect (this,
				SIGNAL (accountValidated (bool)),
				ParentBloggingPlatform_,
				SLOT (handleAccountValidated (bool)));

		connect (this,
				SIGNAL (accountSettingsChanged ()),
				ParentBloggingPlatform_,
				SLOT (saveAccounts ()));

		if (IsValid_)
			AccountStorage_->Init (DatabasePath_);
	}

	QByteArray LocalBlogAccount::Serialize () const
	{
		quint16 ver = 1;
		QByteArray result;
		{
			QDataStream ostr (&result, QIODevice::WriteOnly);
			ostr << ver
					<< Name_
					<< DatabasePath_
					<< IsValid_;
		}

		return result;
	}

	LocalBlogAccount* LocalBlogAccount::Deserialize (const QByteArray& data, QObject *parent)
	{
		quint16 ver = 0;
		QDataStream in (data);
		in >> ver;

		if (ver != 1)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown version"
					<< ver;
			return 0;
		}

		QString name;
		in >> name;
		LocalBlogAccount *result = new LocalBlogAccount (name, parent);
		in >> result->DatabasePath_
				>> result->IsValid_;

		return result;
	}

	void LocalBlogAccount::Validate ()
	{
		IsValid_ = AccountStorage_->CheckDatabase (DatabasePath_);
		emit accountValidated (IsValid_);
	}

	void LocalBlogAccount::updateProfile ()
	{
	}

	void LocalBlogAccount::submit (const Entry& e)
	{
		try
		{
			AccountStorage_->SaveNewEntry (e);
			emit entryPosted ({ e });
			emit requestEntriesBegin ();
			handleLoadAllEvents ();
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

	void LocalBlogAccount::preview (const Entry&)
	{
	}

	void LocalBlogAccount::handleLoadAllEvents ()
	{
		try
		{
			emit gotEntries (AccountStorage_->GetEntries (AccountStorage::Mode::FullMode));
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

}
}
}
