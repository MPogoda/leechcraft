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

#include "clmodel.h"
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QMessageBox>
#include <util/xpc/defaulthookproxy.h>
#include "interfaces/azoth/iclentry.h"
#include "interfaces/azoth/iaccount.h"
#include "core.h"
#include "transferjobmanager.h"
#include "mucinvitedialog.h"
#include "dndutil.h"
#include "cltooltipmanager.h"

namespace LeechCraft
{
namespace Azoth
{
	CLModel::CLModel (CLTooltipManager *manager, QObject *parent)
	: QStandardItemModel { parent }
	, TooltipManager_ { manager }
	{
	}

	QStringList CLModel::mimeTypes () const
	{
		return { DndUtil::GetFormatId (), "text/uri-list", "text/plain" };
	}

	QMimeData* CLModel::mimeData (const QModelIndexList& indexes) const
	{
		QMimeData *result = new QMimeData;

		QStringList names;
		QList<QUrl> urls;
		QList<DndUtil::MimeContactInfo> entries;

		for (const auto& index : indexes)
		{
			if (index.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
				continue;

			auto entryObj = index.data (Core::CLREntryObject).value<QObject*> ();
			auto entry = qobject_cast<ICLEntry*> (entryObj);
			if (!entry)
				continue;

			const auto& thisGroup = index.parent ().data (Core::CLREntryCategory).toString ();

			entries.append ({ entry, thisGroup });
			names << entry->GetEntryName ();
			urls << QUrl (entry->GetHumanReadableID ());
		}

		DndUtil::Encode (entries, result);
		result->setText (names.join ("; "));
		result->setUrls (urls);

		return result;
	}

	bool CLModel::dropMimeData (const QMimeData *mime,
			Qt::DropAction action, int row, int, const QModelIndex& parent)
	{
		if (action == Qt::IgnoreAction)
			return true;

		if (PerformHooks (mime, row, parent))
			return true;

		if (TryInvite (mime, row, parent))
			return true;

		if (TryDropContact (mime, row, parent) ||
				TryDropContact (mime, parent.row (), parent.parent ()))
			return true;

		if (TryDropFile (mime, parent))
			return true;

		return false;
	}

	Qt::DropActions CLModel::supportedDropActions () const
	{
		return static_cast<Qt::DropActions> (Qt::CopyAction | Qt::MoveAction | Qt::LinkAction);
	}

	bool CLModel::PerformHooks (const QMimeData *mime, int row, const QModelIndex& parent)
	{
		if (CheckHookDnDEntry2Entry (mime, row, parent))
			return true;

		return false;
	}

	bool CLModel::CheckHookDnDEntry2Entry (const QMimeData *mime, int row, const QModelIndex& parent)
	{
		if (row != -1 ||
				!DndUtil::HasContacts (mime) ||
				parent.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
			return false;

		const auto source = DndUtil::DecodeEntryObj (mime);
		if (!source)
			return false;

		QObject *target = parent.data (Core::CLREntryObject).value<QObject*> ();

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookDnDEntry2Entry (proxy, source, target);
		return proxy->IsCancelled ();
	}

	bool CLModel::TryInvite (const QMimeData *mime, int, const QModelIndex& parent)
	{
		if (!DndUtil::HasContacts (mime))
			return false;

		if (parent.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
			return false;

		const auto targetObj = parent.data (Core::CLREntryObject).value<QObject*> ();
		const auto targetEntry = qobject_cast<ICLEntry*> (targetObj);
		const auto targetMuc = qobject_cast<IMUCEntry*> (targetObj);

		bool accepted = false;

		for (const auto& serializedObj : DndUtil::DecodeEntryObjs (mime))
		{
			const auto serializedEntry = qobject_cast<ICLEntry*> (serializedObj);
			if (!serializedEntry)
				continue;

			const auto serializedMuc = qobject_cast<IMUCEntry*> (serializedObj);
			if ((targetMuc && serializedMuc) || (!targetMuc && !serializedMuc))
				continue;

			auto muc = serializedMuc ? serializedMuc : targetMuc;
			auto entry = serializedMuc ? targetEntry : serializedEntry;

			MUCInviteDialog dia (qobject_cast<IAccount*> (entry->GetParentAccount ()));
			dia.SetID (entry->GetHumanReadableID ());
			if (dia.exec () == QDialog::Accepted)
				muc->InviteToMUC (dia.GetID (), dia.GetInviteMessage ());

			accepted = true;
		}

		return accepted;
	}

	bool CLModel::TryDropContact (const QMimeData *mime, int row, const QModelIndex& parent)
	{
		if (!DndUtil::HasContacts (mime))
			return false;

		if (parent.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETAccount)
			return false;

		QObject *accObj = parent.data (Core::CLRAccountObject).value<QObject*> ();
		IAccount *acc = qobject_cast<IAccount*> (accObj);
		if (!acc)
			return false;

		const auto& newGrp = parent.child (row, 0).data (Core::CLREntryCategory).toString ();

		for (const auto& info : DndUtil::DecodeMimeInfos (mime))
		{
			const auto entry = info.Entry_;
			const auto& oldGroup = info.Group_;

			if (oldGroup == newGrp)
				continue;

			auto groups = entry->Groups ();
			groups.removeAll (oldGroup);
			groups << newGrp;

			entry->SetGroups (groups);
		}

		return true;
	}

	bool CLModel::TryDropFile (const QMimeData* mime, const QModelIndex& parent)
	{
		// If MIME has CLEntryFormat, it's another serialized entry, we probably
		// don't want to send it.
		if (DndUtil::HasContacts (mime))
			return false;

		if (parent.data (Core::CLREntryType).value<Core::CLEntryType> () != Core::CLETContact)
			return false;

		QObject *entryObj = parent.data (Core::CLREntryObject).value<QObject*> ();
		ICLEntry *entry = qobject_cast<ICLEntry*> (entryObj);

		const auto& urls = mime->urls ();
		if (urls.isEmpty ())
			return false;

		return Core::Instance ().GetTransferJobManager ()->OfferURLs (entry, urls);
	}
}
}
