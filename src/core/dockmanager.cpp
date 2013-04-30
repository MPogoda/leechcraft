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

#include "dockmanager.h"
#include <QDockWidget>
#include <QToolButton>
#include <QToolBar>
#include <util/defaulthookproxy.h>
#include <interfaces/ihavetabs.h>
#include "tabmanager.h"
#include "core.h"
#include "rootwindowsmanager.h"
#include "mainwindow.h"
#include "docktoolbarmanager.h"

namespace LeechCraft
{
	DockManager::DockManager (RootWindowsManager *rootWM, QObject *parent)
	: QObject (parent)
	, RootWM_ (rootWM)
	{
		for (int i = 0; i < RootWM_->GetWindowsCount (); ++i)
			handleWindow (i);

		connect (RootWM_,
				SIGNAL (windowAdded (int)),
				this,
				SLOT (handleWindow (int)));
	}

	void DockManager::AddDockWidget (QDockWidget *dw, Qt::DockWidgetArea area)
	{
		auto win = static_cast<MainWindow*> (RootWM_->GetPreferredWindow ());
		win->addDockWidget (area, dw);
		Dock2Window_ [dw] = win;

		connect (dw,
				SIGNAL (destroyed (QObject*)),
				this,
				SLOT (handleDockDestroyed ()));

		Window2DockToolbarMgr_ [win]->AddDock (dw, area);

		auto toggleAct = dw->toggleViewAction ();
		ToggleAct2Dock_ [toggleAct] = dw;
		connect (toggleAct,
				SIGNAL (triggered (bool)),
				this,
				SLOT (handleDockToggled (bool)));
	}

	void DockManager::AssociateDockWidget (QDockWidget *dock, QWidget *tab)
	{
		dock->installEventFilter (this);

		TabAssociations_ [dock] = tab;

		auto rootWM = Core::Instance ().GetRootWindowsManager ();
		const auto winIdx = rootWM->GetWindowForTab (qobject_cast<ITabWidget*> (tab));
		if (winIdx >= 0)
			handleTabChanged (rootWM->GetTabManager (winIdx)->GetCurrentWidget ());
		else
			dock->setVisible (false);
	}

	void DockManager::ToggleViewActionVisiblity (QDockWidget *widget, bool visible)
	{
		auto win = Dock2Window_ [widget];

		Util::DefaultHookProxy_ptr proxy (new Util::DefaultHookProxy);
		emit hookDockWidgetActionVisToggled (proxy, win, widget, visible);
		if (proxy->IsCancelled ())
			return;

		/*
		// TODO
		QAction *act = widget->toggleViewAction ();
		if (!visible)
			MenuView_->removeAction (act);
		else
			MenuView_->insertAction (MenuView_->actions ().first (), act);
		*/
	}

	bool DockManager::eventFilter (QObject *obj, QEvent *event)
	{
		if (event->type () != QEvent::Close)
			return false;

		auto dock = qobject_cast<QDockWidget*> (obj);
		if (!dock)
			return false;

		ForcefullyClosed_ << dock;

		return false;
	}

	void DockManager::handleTabMove (int from, int to, int tab)
	{
		auto rootWM = Core::Instance ().GetRootWindowsManager ();

		auto fromWin = static_cast<MainWindow*> (rootWM->GetMainWindow (from));
		auto toWin = static_cast<MainWindow*> (rootWM->GetMainWindow (to));
		auto widget = fromWin->GetTabWidget ()->Widget (tab);

		for (auto i = TabAssociations_.begin (), end = TabAssociations_.end (); i != end; ++i)
			if (*i == widget)
			{
				auto dw = i.key ();
				Dock2Window_ [dw] = toWin;

				const auto area = fromWin->dockWidgetArea (dw);

				fromWin->removeDockWidget (dw);
				Window2DockToolbarMgr_ [fromWin]->RemoveDock (dw);
				toWin->addDockWidget (area, dw);
				Window2DockToolbarMgr_ [toWin]->AddDock (dw, area);
			}
	}

	void DockManager::handleDockDestroyed ()
	{
		auto dock = static_cast<QDockWidget*> (sender ());

		auto toggleAct = ToggleAct2Dock_.key (dock);
		Window2DockToolbarMgr_ [Dock2Window_ [dock]]->HandleDockDestroyed (dock, toggleAct);

		TabAssociations_.remove (dock);
		ToggleAct2Dock_.remove (toggleAct);
		ForcefullyClosed_.remove (dock);
		Dock2Window_.remove (dock);
	}

	void DockManager::handleDockToggled (bool isVisible)
	{
		auto dock = ToggleAct2Dock_ [static_cast<QAction*> (sender ())];
		if (!dock)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown toggler"
					<< sender ();
			return;
		}

		if (isVisible)
		{
			if (ForcefullyClosed_.remove (dock))
			{
				auto win = Dock2Window_ [dock];
				Window2DockToolbarMgr_ [win]->AddDock (dock, win->dockWidgetArea (dock));
			}
		}
		else
			ForcefullyClosed_ << dock;
	}

	void DockManager::handleTabChanged (QWidget *tabWidget)
	{
		auto thisWindowIdx = RootWM_->GetWindowForTab (qobject_cast<ITabWidget*> (tabWidget));
		auto thisWindow = static_cast<MainWindow*> (RootWM_->GetMainWindow (thisWindowIdx));
		auto toolbarMgr = Window2DockToolbarMgr_ [thisWindow];

		QList<QDockWidget*> toShow;
		for (auto dock : TabAssociations_.keys ())
		{
			auto otherWidget = TabAssociations_ [dock];
			auto otherWindow = RootWM_->GetWindowIndex (Dock2Window_ [dock]);
			if (otherWindow != thisWindowIdx)
				continue;

			if (otherWidget != tabWidget)
			{
				dock->setVisible (false);
				toolbarMgr->RemoveDock (dock);
			}
			else if (!ForcefullyClosed_.contains (dock))
				toShow << dock;
		}

		for (auto dock : toShow)
		{
			dock->setVisible (true);
			toolbarMgr->AddDock (dock, thisWindow->dockWidgetArea (dock));
		}
	}

	void DockManager::handleWindow (int index)
	{
		auto win = static_cast<MainWindow*> (RootWM_->GetMainWindow (index));
		Window2DockToolbarMgr_ [win] = new DockToolbarManager (win);

		connect (win,
				SIGNAL (destroyed (QObject*)),
				this,
				SLOT (handleWindowDestroyed ()));
	}

	void DockManager::handleWindowDestroyed ()
	{
		auto win = static_cast<MainWindow*> (sender ());
		Window2DockToolbarMgr_.remove (win);
	}
}
