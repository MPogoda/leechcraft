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

#include "capsdatabase.h"
#include <QDataStream>
#include <QFile>
#include <QTimer>
#include <util/util.h>
#include <util/sys/paths.h>
#include "util.h"

Q_DECLARE_METATYPE (QXmppDiscoveryIq::Identity);

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	CapsDatabase::CapsDatabase (QObject *parent)
	: QObject (parent)
	, SaveScheduled_ (false)
	{
		qRegisterMetaType<QXmppDiscoveryIq::Identity> ("QXmppDiscoveryIq::Identity");
		qRegisterMetaTypeStreamOperators<QXmppDiscoveryIq::Identity> ("QXmppDiscoveryIq::Identity");
		Load ();
	}

	bool CapsDatabase::Contains (const QByteArray& hash) const
	{
		return Ver2Features_.contains (hash) &&
				Ver2Identities_.contains (hash);
	}

	QStringList CapsDatabase::Get (const QByteArray& hash) const
	{
		return Ver2Features_ [hash];
	}

	void CapsDatabase::Set (const QByteArray& hash, const QStringList& features)
	{
		Ver2Features_ [hash] = features;
		ScheduleSave ();
	}

	QList<QXmppDiscoveryIq::Identity> CapsDatabase::GetIdentities (const QByteArray& hash) const
	{
		return Ver2Identities_ [hash];
	}

	void CapsDatabase::SetIdentities (const QByteArray& hash,
			const QList<QXmppDiscoveryIq::Identity>& ids)
	{
		Ver2Identities_ [hash] = ids;
		ScheduleSave ();
	}

	void CapsDatabase::save () const
	{
		QDir dir = Util::CreateIfNotExists ("azoth/xoox");
		QFile file (dir.filePath ("caps_s.db"));
		if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open file"
					<< file.fileName ()
					<< "for writing:"
					<< file.errorString ();
			return;
		}

		QDataStream stream (&file);
		stream << static_cast<quint8> (2)
				<< Ver2Features_
				<< Ver2Identities_;

		SaveScheduled_ = false;
	}

	void CapsDatabase::ScheduleSave ()
	{
		if (SaveScheduled_)
			return;

		SaveScheduled_ = true;
		QTimer::singleShot (10000,
				this,
				SLOT (save ()));
	}

	void CapsDatabase::Load ()
	{
		QDir dir = Util::CreateIfNotExists ("azoth/xoox");
		QFile file (dir.filePath ("caps_s.db"));
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open file"
					<< file.fileName ()
					<< "for reading:"
					<< file.errorString ();
			return;
		}

		QDataStream stream (&file);
		quint8 ver = 0;
		stream >> ver;
		if (ver < 1 || ver > 2)
			qWarning () << Q_FUNC_INFO
					<< "unknown storage version"
					<< ver;
		if (ver >= 1)
			stream >> Ver2Features_;
		if (ver >= 2)
			stream >> Ver2Identities_;
	}
}
}
}
