/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2009  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#ifndef PLUGINS_PYLC_WRAPPEROBJECT_H
#define PLUGINS_PYLC_WRAPPEROBJECT_H
#include <QObject>
#include <PythonQt/PythonQt.h>
#include <interfaces/iinfo.h>
#include <interfaces/idownload.h>

namespace LeechCraft
{
	namespace Plugins
	{
		namespace PyLC
		{
			class WrapperObject : public QObject
								, public IInfo
								, public IDownload
			{
				Q_OBJECT
				Q_INTERFACES (IInfo IDownload)

				QString Filename_;
				mutable PythonQtObjectPtr Module_;
			public:
				WrapperObject (const QString&, QObject* = 0);

				void Init (ICoreProxy_ptr);
				void SecondInit ();
				void Release ();
				QString GetName () const;
				QString GetInfo () const;
				QIcon GetIcon () const;
				QStringList Provides () const;
				QStringList Needs () const;
				QStringList Uses () const;
				void SetProvider (QObject*, const QString&);

				qint64 GetDownloadSpeed () const;
				qint64 GetUploadSpeed () const;
				void StartAll ();
				void StopAll ();
				bool CouldDownload (const LeechCraft::Entity&) const;
				int AddJob (LeechCraft::Entity);
				void KillTask (int);
			private:
				void* qt_metacast_dummy (const char*);
				QVariant Call (const QString&, QVariantList = QVariantList ()) const;
				bool Implements (const char*);
			};
		};
	};
};

#endif

