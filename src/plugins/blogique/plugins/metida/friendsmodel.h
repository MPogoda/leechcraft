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

#pragma once

#include <QStandardItemModel>

namespace LeechCraft
{
namespace Blogique
{
namespace Metida
{
	class FriendsModel : public QStandardItemModel
	{
		Q_OBJECT
	public:
		enum Columns
		{
			Nickname_ = 0,
			FriendStatus_,
			Name_,
			Birthday_
		};
		
		enum FriendsRoles
		{
			FRFriendStatus = Qt::UserRole + 1
		};
		
		enum FriendStatus
		{
			FSFriendOf = 0,
			FSMyFriend,
			FSBothFriends
		};

		FriendsModel (QObject *parent = 0);

		Qt::ItemFlags flags (const QModelIndex& index) const;

		Qt::DropActions supportedDropActions () const;

		QStringList mimeTypes () const;
		QMimeData* mimeData (const QModelIndexList& indexes) const;
		bool dropMimeData (const QMimeData *data, Qt::DropAction action,
				int row, int column, const QModelIndex& parent);

	signals:
		void userGroupChanged (const QString& name,
				const QString& bgColor, const QString& fgColor, int realGroupId);
	};
}
}
}
