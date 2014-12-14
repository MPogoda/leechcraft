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

#include "panelsettingsdialog.h"
#include <QStandardItemModel>
#include <xmlsettingsdialog/xmlsettingsdialog.h>

namespace LeechCraft
{
namespace SB2
{
	PanelSettingsDialog::PanelSettingsDialog (const SettingsList_t& settingsList, QWidget *parent)
	: QDialog { parent }
	, ItemsModel_ { new QStandardItemModel { this } }
	, Items_ { settingsList }
	{
		Ui_.setupUi (this);

		for (const auto& item : Items_)
		{
			const auto rowItem = new QStandardItem { item.Name_ };
			rowItem->setEditable (false);
			ItemsModel_->appendRow (rowItem);

			Ui_.SettingsStack_->addWidget (item.XSD_);
		}

		Ui_.ItemsView_->setModel (ItemsModel_);

		connect (Ui_.ItemsView_->selectionModel (),
				SIGNAL (currentChanged (QModelIndex, QModelIndex)),
				this,
				SLOT (handleItemSelected (QModelIndex)));
	}

	void PanelSettingsDialog::on_ButtonBox__accepted ()
	{
		for (const auto& item : Items_)
			item.XSD_->setParent (0);
	}

	void PanelSettingsDialog::on_ButtonBox__rejected ()
	{
		for (const auto& item : Items_)
			item.XSD_->setParent (0);
	}

	void PanelSettingsDialog::on_ButtonBox__clicked (QAbstractButton *button)
	{
		switch (Ui_.ButtonBox_->buttonRole (button))
		{
		case QDialogButtonBox::AcceptRole:
		case QDialogButtonBox::ApplyRole:
			for (const auto& item : Items_)
				item.XSD_->accept ();
			break;
		case QDialogButtonBox::RejectRole:
		case QDialogButtonBox::ResetRole:
			for (const auto& item : Items_)
				item.XSD_->reject ();
			break;
		default:
			break;
		}
	}

	void PanelSettingsDialog::handleItemSelected (const QModelIndex& index)
	{
		if (!index.isValid ())
			return;

		Ui_.SettingsStack_->setCurrentIndex (index.row ());
	}
}
}