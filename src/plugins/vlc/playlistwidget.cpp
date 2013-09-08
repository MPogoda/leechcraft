/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2013  Vladislav Tyulbashev
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANusers/vtyulb/TY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include <vlc/vlc.h>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QVBoxLayout>
#include <QTimer>
#include <QString>
#include <QRect>
#include <QModelIndex>
#include <QAction>
#include <QMenu>
#include "playlistwidget.h"
#include "playlistmodel.h"

namespace LeechCraft
{
namespace vlc
{
	PlaylistWidget::PlaylistWidget (QWidget *parent)
	: QTreeView (parent)
	{
		setAcceptDrops (true);
		setFixedWidth (200);
		show ();
	}
	
	void PlaylistWidget::Init (libvlc_instance_t *instance, libvlc_media_player_t *player)
	{
		Player_ = libvlc_media_list_player_new (instance);
		Instance_ = instance;
		libvlc_media_list_player_set_media_player (Player_, player);
		Playlist_ = libvlc_media_list_new (Instance_);
		libvlc_media_list_player_set_media_list (Player_, Playlist_);
		NativePlayer_ = player;
		
		Model_ = new PlaylistModel (Playlist_, this);
		setModel (Model_);
		
		
		QTimer *timer = new QTimer (this);
		timer->setInterval (1000);
		connect (timer,
				SIGNAL (timeout ()),
				this,
				SLOT (updateInterface ()));
		
		timer->start ();
		
		connect (selectionModel (),
				SIGNAL (currentRowChanged (QModelIndex, QModelIndex)),
				this,
				SLOT (selectionChanged (QModelIndex, QModelIndex)));
		
		DeleteAction_ = new QAction (this);
		DeleteAction_->setShortcut (QKeySequence (Qt::Key_Delete));
		
		connect (this,
				SIGNAL (customContextMenuRequested (QPoint)),
				this,
				SLOT (createMenu (QPoint)));
	}
	
	void PlaylistWidget::AddUrl (const QUrl& url)
	{
		libvlc_media_t *m = libvlc_media_new_path (Instance_, url.toString ().toUtf8 ());
		libvlc_media_list_add_media (Playlist_, m);
		libvlc_media_list_player_play (Player_);
		
		updateInterface ();
	}
	
	bool PlaylistWidget::NowPlaying ()
	{
		return libvlc_media_list_player_is_playing (Player_);
	}
	
	void PlaylistWidget::togglePlay ()
	{
		if (NowPlaying ())
			libvlc_media_list_player_pause (Player_);
		else
			libvlc_media_list_player_play (Player_);
	}
	
	void PlaylistWidget::dragEnterEvent (QDragEnterEvent *event)
	{
		fprintf (stderr, "drag\n");
		event->accept ();
		fprintf (stderr, "%d\n", event->isAccepted ());
	}

	void PlaylistWidget::dropEvent (QDropEvent *event)
	{
		QList <QUrl> urls = event->mimeData ()->urls ();
		for (int i = 0; i < urls.size (); i++)
			AddUrl (urls [i]);
		
		fprintf (stderr, "drop\n");
		event->accept ();
	}
	
	void PlaylistWidget::Clear ()
	{
		libvlc_media_list_player_stop (Player_);
		while (libvlc_media_list_count (Playlist_))
			libvlc_media_list_remove_index (Playlist_, 0);
	}
	
	void PlaylistWidget::updateInterface ()
	{
		Model_->updateTable ();
		
		int currentRow = libvlc_media_list_index_of_item (Playlist_, libvlc_media_player_get_media (NativePlayer_));
		for (int i = 0; i < Model_->rowCount (); i++)
			if (i != currentRow)
				selectionModel ()->select (Model_->indexFromItem (Model_->item (i)),
													QItemSelectionModel::Deselect);
				
		selectionModel ()->select (Model_->indexFromItem (Model_->item (currentRow)),
									QItemSelectionModel::Select);
		
		update ();
	}
	
	void PlaylistWidget::mousePressEvent (QMouseEvent *event)
	{
		if (event->button () != Qt::LeftButton)
			return;
		
		const QModelIndex& index = indexAt (event->pos ());
		int item = index.row ();
		libvlc_media_list_player_play_item_at_index (Player_, item);
		fprintf (stderr, "%d\n", item);
	}
	
	void PlaylistWidget::selectionChanged (const QModelIndex& current, const QModelIndex& previous)
	{
		libvlc_media_list_player_play_item_at_index (Player_, current.row ());
	}
	
	void PlaylistWidget::createMenu (QPoint p)
	{
		QMenu *menu = new QMenu (this);
		QAction *action = new QAction (menu);
		action->setText ("Delete");
		action->setData (QVariant (indexAt (p).row ()));
		menu->exec (p);
		
		connect (menu,
				SIGNAL (triggered (QAction*)),
				this,
				SLOT (deleteRequested (QAction*)));
	}
	
	void PlaylistWidget::deleteRequested (QAction *object)
	{
		libvlc_media_list_remove_index (Playlist_, object->data ().toInt ());
	}
}
}
