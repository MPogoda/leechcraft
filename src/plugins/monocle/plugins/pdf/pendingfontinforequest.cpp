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

#include "pendingfontinforequest.h"
#include <QtConcurrentRun>
#include <QFutureWatcher>

#if QT_VERSION < 0x050000
#include <poppler-qt4.h>
#else
#include <poppler-qt5.h>
#endif

#include <util/sll/slotclosure.h>

namespace LeechCraft
{
namespace Monocle
{
namespace PDF
{
	PendingFontInfoRequest::PendingFontInfoRequest (const std::shared_ptr<Poppler::Document>& doc)
	{
		const auto watcher = new QFutureWatcher<QList<Poppler::FontInfo>> { this };
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[this, watcher] () -> void
			{
				watcher->deleteLater ();
				for (const auto& item : watcher->result ())
					Result_.append ({
							item.name (),
							item.file (),
							item.isEmbedded ()
						});
				emit ready ();
				deleteLater ();
			},
			watcher,
			SIGNAL (finished ()),
			this
		};
		watcher->setFuture (QtConcurrent::run ([doc] { return doc->fonts (); }));
	}

	QObject* PendingFontInfoRequest::GetQObject ()
	{
		return this;
	}

	QList<FontInfo> PendingFontInfoRequest::GetFontInfos () const
	{
		return Result_;
	}
}
}
}