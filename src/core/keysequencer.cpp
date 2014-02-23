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

#include "keysequencer.h"
#include <QKeyEvent>
#include <QtDebug>

KeySequencer::KeySequencer (const QString& labelStr, QWidget *parent)
: QDialog (parent)
, LastCode_ (0)
{
	Ui_.setupUi (this);
	Ui_.DescLabel_->setText (labelStr);
	Ui_.DescLabel_->setFocus ();
}

QKeySequence KeySequencer::GetResult () const
{
	return Result_;
}

namespace
{
	void FixCodes (int& code, QKeyEvent *event)
	{
		auto fixSingle = [&code, event] (Qt::KeyboardModifier keyM, Qt::Modifier codeM)
		{
			if (event->modifiers () & keyM)
				code += codeM;
		};

		fixSingle (Qt::ControlModifier, Qt::CTRL);
		fixSingle (Qt::AltModifier, Qt::ALT);
		fixSingle (Qt::ShiftModifier, Qt::SHIFT);
		fixSingle (Qt::MetaModifier, Qt::META);
	}
}

void KeySequencer::keyPressEvent (QKeyEvent *event)
{
	event->accept ();

	LastCode_ = 0;
	FixCodes (LastCode_, event);

	const int key = event->key ();
	if (key != Qt::Key_Control &&
			key != Qt::Key_Alt &&
			key != Qt::Key_Meta &&
			key != Qt::Key_Shift)
		LastCode_ += key;

	const QKeySequence ts (LastCode_);

	Ui_.Shortcut_->setText (ts.toString (QKeySequence::NativeText));
}

void KeySequencer::keyReleaseEvent (QKeyEvent *event)
{
	event->accept ();

	auto testCode = LastCode_;
	for (auto code : { Qt::CTRL, Qt::ALT, Qt::SHIFT, Qt::META })
		testCode &= ~code;
	if (!testCode)
	{
		reject ();
		return;
	}

	Result_ = QKeySequence (LastCode_);

	accept ();
}

