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

#include "avatarsstorageondisk.h"
#include <QDir>
#include <QSqlError>
#include <util/db/util.h>
#include <util/db/oral.h>
#include <util/sys/paths.h>
#include "interfaces/azoth/ihaveavatars.h"

namespace LeechCraft
{
namespace Azoth
{
	struct AvatarsStorageOnDisk::Record
	{
		Util::oral::PKey<int> ID_;

		QByteArray EntryID_;
		IHaveAvatars::Size Size_;
		QByteArray ImageData_;

		static QByteArray ClassName ()
		{
			return "Record";
		}

		static QString FieldNameMorpher (const QString& str)
		{
			return str.left (str.size () - 1);
		}
	};
}
}

using AvatarRecord = LeechCraft::Azoth::AvatarsStorageOnDisk::Record;

BOOST_FUSION_ADAPT_STRUCT (AvatarRecord,
		(decltype (AvatarRecord::ID_), ID_)
		(decltype (AvatarRecord::EntryID_), EntryID_)
		(decltype (AvatarRecord::Size_), Size_)
		(decltype (AvatarRecord::ImageData_), ImageData_))

namespace LeechCraft
{
namespace Util
{
namespace oral
{
	template<>
	struct Type2Name<Azoth::IHaveAvatars::Size>
	{
		QString operator() () const { return Type2Name<int> {} (); }
	};

	template<>
	struct ToVariant<Azoth::IHaveAvatars::Size>
	{
		QVariant operator() (Azoth::IHaveAvatars::Size size) const
		{
			return static_cast<int> (size);
		}
	};

	template<>
	struct FromVariant<Azoth::IHaveAvatars::Size>
	{
		Azoth::IHaveAvatars::Size operator() (const QVariant& var) const
		{
			return static_cast<Azoth::IHaveAvatars::Size> (var.toInt ());
		}
	};
}
}

namespace Azoth
{
	AvatarsStorageOnDisk::AvatarsStorageOnDisk (QObject *parent)
	: QObject { parent }
	, DB_ { QSqlDatabase::addDatabase ("QSQLITE",
				Util::GenConnectionName ("org.LeechCraft.Azoth.Xoox.VCards")) }
	{
		const auto& cacheDir = Util::GetUserDir (Util::UserDir::Cache, "azoth");
		DB_.setDatabaseName (cacheDir.filePath ("avatars.db"));
		if (!DB_.open ())
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot open the database";
			Util::DBLock::DumpError (DB_.lastError ());
			throw std::runtime_error { "Cannot create database" };
		}

		Util::RunTextQuery (DB_, "PRAGMA synchronous = NORMAL;");
		Util::RunTextQuery (DB_, "PRAGMA journal_mode = WAL;");

		AdaptedRecord_ = Util::oral::AdaptPtr<Record> (DB_);
	}
}
}
