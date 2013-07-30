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

#include "mobiparser.h"
#include <QFile>
#include <QtDebug>
#include <QtEndian>

namespace LeechCraft
{
namespace Monocle
{
namespace Dik
{
	MobiParser::MobiParser (const QString& filename)
	: File_ (new QFile (filename))
	{
		if (!File_->open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot open file"
					<< File_->errorString ();
			return;
		}

		if (!InitRecords () ||
				!InitHeader ())
			return;

		IsValid_ = true;
	}

	QByteArray MobiParser::GetRecord (int idx) const
	{
		if (idx >= NumRecords_)
			return {};

		const auto offset = RecordOffsets_.at (idx);
		if (!File_->seek (offset))
			return {};

		return idx == NumRecords_ - 1 ?
				File_->readAll () :
				File_->read (RecordOffsets_.at (idx + 1) - offset);
	}

	bool MobiParser::InitRecords ()
	{
		if (!File_->seek (0x3c))
			return false;

		const QString filetype (File_->read (8));

		if (!File_->seek (0x4c))
			return false;

		Read (NumRecords_);
		NumRecords_ = qFromBigEndian (NumRecords_);

		for (quint16 i = 0; i < NumRecords_; ++i)
		{
			quint32 offset = 0;
			Read (offset);
			RecordOffsets_ << qFromBigEndian (offset);
			Read (offset);
		}

		return true;
	}

	bool MobiParser::InitHeader ()
	{
		const auto& headrec = GetRecord (0);
		if (headrec.size () < 14)
			return false;

		return true;
	}
}
}
}
