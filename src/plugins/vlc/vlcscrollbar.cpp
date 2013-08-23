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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "vlcscrollbar.h"
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace LeechCraft
{
namespace vlc
{
	VlcScrollBar::VlcScrollBar (QWidget* parent)
	{
		QVBoxLayout *layout = new QVBoxLayout;
		layout->addWidget (this);
		parent->setLayout (layout);
		CurrentPosition_ = 0;
	}

	void VlcScrollBar::paintEvent (QPaintEvent *event)
	{
		QPainter p (this);
		
		p.setBrush (palette ().dark ());
		p.drawRect (0, 0, width () - 1, height () - 1);
		
		p.setBrush (palette ().button ());
		p.drawRect (0, 0, std::min (int (width () * CurrentPosition_), width () - 1), height () - 1);
		
		p.end ();
		event->accept ();
	}
	
	void VlcScrollBar::mousePressEvent (QMouseEvent *event)
	{
		emit changePosition (event->x () / double (width ()));
		event->accept ();
	}
	
	void VlcScrollBar::setPosition (double pos)
	{
		CurrentPosition_ = pos;
	}
}
}
