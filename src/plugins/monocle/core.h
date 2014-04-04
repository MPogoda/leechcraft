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

#pragma once

#include <QObject>
#include <interfaces/monocle/ibackendplugin.h>
#include <interfaces/core/icoreproxy.h>

namespace LeechCraft
{
namespace Monocle
{
	class RecentlyOpenedManager;
	class PixmapCacheManager;
	class DefaultBackendManager;
	class DocStateManager;
	class BookmarksManager;

	class Core : public QObject
	{
		Q_OBJECT

		ICoreProxy_ptr Proxy_;
		QList<QObject*> Backends_;

		PixmapCacheManager *CacheManager_;
		RecentlyOpenedManager *ROManager_;
		DefaultBackendManager *DefaultBackendManager_;
		DocStateManager *DocStateManager_;
		BookmarksManager *BookmarksManager_;

		Core ();
	public:
		static Core& Instance ();

		void SetProxy (ICoreProxy_ptr);
		ICoreProxy_ptr GetProxy () const;

		void AddPlugin (QObject*);

		bool CanHandleMime (const QString&);
		bool CanLoadDocument (const QString&);
		IDocument_ptr LoadDocument (const QString&);

		PixmapCacheManager* GetPixmapCacheManager () const;
		RecentlyOpenedManager* GetROManager () const;
		DefaultBackendManager* GetDefaultBackendManager () const;
		DocStateManager* GetDocStateManager () const;
		BookmarksManager* GetBookmarksManager () const;
	};
}
}
