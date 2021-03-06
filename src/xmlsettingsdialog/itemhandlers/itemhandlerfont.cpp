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

#include "itemhandlerfont.h"
#include <QLabel>
#include <QGridLayout>
#include <QApplication>
#include <QtDebug>
#include "../fontpicker.h"

namespace LeechCraft
{
	ItemHandlerFont::ItemHandlerFont ()
	{
	}

	ItemHandlerFont::~ItemHandlerFont ()
	{
	}

	bool ItemHandlerFont::CanHandle (const QDomElement& element) const
	{
		return element.attribute ("type") == "font";
	}

	void ItemHandlerFont::Handle (const QDomElement& item, QWidget *pwidget)
	{
		QGridLayout *lay = qobject_cast<QGridLayout*> (pwidget->layout ());
		const QString& labelString = XSD_->GetLabel (item);
		QLabel *label = new QLabel (labelString);
		label->setWordWrap (false);

		FontPicker *picker = new FontPicker (labelString, XSD_->GetWidget ());
		picker->setObjectName (item.attribute ("property"));
		picker->SetCurrentFont (XSD_->GetValue (item).value<QFont> ());

		connect (picker,
				SIGNAL (currentFontChanged (const QFont&)),
				this,
				SLOT (updatePreferences ()));

		picker->setProperty ("ItemHandler", QVariant::fromValue<QObject*> (this));
		picker->setProperty ("SearchTerms", labelString);

		int row = lay->rowCount ();
		lay->addWidget (label, row, 0);
		lay->addWidget (picker, row, 1);
	}

	QVariant ItemHandlerFont::GetValue (const QDomElement& element,
			QVariant value) const
	{
		if (value.isNull () ||
				!value.canConvert<QFont> ())
		{
			if (element.hasAttribute ("default"))
			{
				QFont font;
				const QString& defStr = element.attribute ("default");
				if (font.fromString (defStr))
					value = font;
				else
					value = QFont (defStr);
			}
			else
				value = QApplication::font ();
		}
		return value;
	}

	void ItemHandlerFont::SetValue (QWidget *widget,
			const QVariant& value) const
	{
		FontPicker *fontPicker = qobject_cast<FontPicker*> (widget);
		if (!fontPicker)
		{
			qWarning () << Q_FUNC_INFO
				<< "not a FontPicker"
				<< widget;
			return;
		}
		fontPicker->SetCurrentFont (value.value<QFont> ());
	}

	void ItemHandlerFont::UpdateValue (QDomElement& element,
			const QVariant& value) const
	{
		element.setAttribute ("default", value.value<QFont> ().toString ());
	}

	QVariant ItemHandlerFont::GetObjectValue (QObject *object) const
	{
		FontPicker *fontPicker = qobject_cast<FontPicker*> (object);
		if (!fontPicker)
		{
			qWarning () << Q_FUNC_INFO
				<< "not a FontPicker"
				<< object;
			return QVariant ();
		}
		return fontPicker->GetCurrentFont ();
	}
};
