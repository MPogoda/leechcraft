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

#include "proxiesconfigwidget.h"
#include <QStandardItemModel>
#include <QSettings>
#include <util/sll/slotclosure.h>
#include "proxiesstorage.h"
#include "editurlsdialog.h"
#include "editlistsdialog.h"
#include "scriptsmanager.h"
#include "proxiesstorage.h"

namespace LeechCraft
{
namespace XProxy
{
	namespace
	{
		QString ProxyType2Str (QNetworkProxy::ProxyType type)
		{
			switch (type)
			{
			case QNetworkProxy::ProxyType::Socks5Proxy:
				return "SOCKS5";
				break;
			case QNetworkProxy::ProxyType::HttpProxy:
				return "HTTP";
				break;
			case QNetworkProxy::ProxyType::HttpCachingProxy:
				return ProxiesConfigWidget::tr ("caching HTTP");
				break;
			case QNetworkProxy::ProxyType::FtpCachingProxy:
				return ProxiesConfigWidget::tr ("caching FTP");
				break;
			case QNetworkProxy::ProxyType::NoProxy:
				return ProxiesConfigWidget::tr ("no proxy");
				break;
			default:
				return ProxiesConfigWidget::tr ("other type");
				break;
			}
		}

		QList<QStandardItem*> Proxy2Row (const Proxy& proxy)
		{
			QList<QStandardItem*> row;
			row << new QStandardItem (ProxyType2Str (proxy.Type_));
			row << new QStandardItem (proxy.Host_ + ":" + QString::number (proxy.Port_));
			row << new QStandardItem (proxy.User_);
			return row;
		}
	}

	ProxiesConfigWidget::ProxiesConfigWidget (ProxiesStorage *storage,
			ScriptsManager *manager, QWidget *parent)
	: QWidget (parent)
	, Storage_ (storage)
	, ScriptsMgr_ (manager)
	, Model_ (new QStandardItemModel (this))
	{
		Ui_.setupUi (this);
		Ui_.ProxiesList_->setModel (Model_);

		connect (Ui_.ProxiesList_->selectionModel (),
				SIGNAL (currentRowChanged (QModelIndex, QModelIndex)),
				this,
				SLOT (handleItemSelected (QModelIndex)));

		const QStringList labels
		{
			tr ("Proxy type"),
			tr ("Proxy target"),
			tr ("User")
		};
		Model_->setHorizontalHeaderLabels (labels);

		reject ();
	}

	Proxy ProxiesConfigWidget::EntryFromUI () const
	{
		auto type = QNetworkProxy::ProxyType::NoProxy;
		switch (Ui_.ProxyType_->currentIndex ())
		{
		case 0:
			type = QNetworkProxy::ProxyType::Socks5Proxy;
			break;
		case 1:
			type = QNetworkProxy::ProxyType::HttpProxy;
			break;
		case 2:
			type = QNetworkProxy::ProxyType::HttpCachingProxy;
			break;
		case 3:
			type = QNetworkProxy::ProxyType::FtpCachingProxy;
			break;
		case 4:
			type = QNetworkProxy::ProxyType::NoProxy;
			break;
		}
		Proxy proxy =
		{
			type,
			Ui_.ProxyHost_->text (),
			Ui_.ProxyPort_->value (),
			Ui_.ProxyUser_->text (),
			Ui_.ProxyPassword_->text ()
		};

		return proxy;
	}

	void ProxiesConfigWidget::accept ()
	{
		Storage_->SaveSettings ();
	}

	void ProxiesConfigWidget::reject ()
	{
		if (const auto rc = Model_->rowCount ())
			Model_->removeRows (0, rc);

		Storage_->LoadSettings ();

		Proxies_ = Storage_->GetKnownProxies ();
		for (const auto& proxy : Proxies_)
			Model_->appendRow (Proxy2Row (proxy));
	}

	void ProxiesConfigWidget::handleItemSelected (const QModelIndex& idx)
	{
		Ui_.UpdateProxyButton_->setEnabled (idx.isValid ());
		Ui_.RemoveProxyButton_->setEnabled (idx.isValid ());
		Ui_.EditUrlsButton_->setEnabled (idx.isValid ());
		Ui_.EditListsButton_->setEnabled (idx.isValid ());

		const auto& proxy = Proxies_.value (idx.row ());
		Ui_.ProxyHost_->setText (proxy.Host_);
		Ui_.ProxyPort_->setValue (proxy.Port_);
		Ui_.ProxyUser_->setText (proxy.User_);
		Ui_.ProxyPassword_->setText (proxy.Pass_);
		switch (proxy.Type_)
		{
		case QNetworkProxy::ProxyType::Socks5Proxy:
			Ui_.ProxyType_->setCurrentIndex (0);
			break;
		case QNetworkProxy::ProxyType::HttpProxy:
			Ui_.ProxyType_->setCurrentIndex (1);
			break;
		case QNetworkProxy::ProxyType::HttpCachingProxy:
			Ui_.ProxyType_->setCurrentIndex (2);
			break;
		case QNetworkProxy::ProxyType::FtpCachingProxy:
			Ui_.ProxyType_->setCurrentIndex (3);
			break;
		case QNetworkProxy::ProxyType::NoProxy:
			Ui_.ProxyType_->setCurrentIndex (4);
			break;
		default:
			qWarning () << Q_FUNC_INFO
					<< "unknown proxy type"
					<< proxy.Type_;
			break;
		}
	}

	void ProxiesConfigWidget::on_AddProxyButton__released ()
	{
		const auto& proxy = EntryFromUI ();
		Proxies_ << proxy;
		Model_->appendRow (Proxy2Row (proxy));

		Storage_->AddProxy (proxy);
	}

	void ProxiesConfigWidget::on_UpdateProxyButton__released ()
	{
		const int row = Ui_.ProxiesList_->currentIndex ().row ();
		if (row < 0 || row >= Proxies_.size ())
			return;

		const auto& oldProxy = Proxies_.at (row);

		const auto& proxy = EntryFromUI ();
		Proxies_ [row] = proxy;
		Model_->removeRow (row);
		Model_->insertRow (row, Proxy2Row (proxy));

		Storage_->UpdateProxy (oldProxy, proxy);
	}

	void ProxiesConfigWidget::on_RemoveProxyButton__released ()
	{
		const int row = Ui_.ProxiesList_->currentIndex ().row ();
		if (row < 0 || row >= Proxies_.size ())
			return;

		Storage_->RemoveProxy (Proxies_.takeAt (row));
		Model_->removeRow (row);
	}

	void ProxiesConfigWidget::on_EditUrlsButton__released ()
	{
		const int row = Ui_.ProxiesList_->currentIndex ().row ();
		if (row < 0 || row >= Proxies_.size ())
			return;

		const auto& proxy = Proxies_.value (row);
		auto dialog = new EditUrlsDialog { Storage_->GetTargets (proxy), this };
		dialog->setAttribute (Qt::WA_DeleteOnClose);

		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[this, proxy, dialog] { Storage_->SetTargets (proxy, dialog->GetTargets ()); },
			dialog,
			SIGNAL (accepted ()),
			dialog
		};

		dialog->show ();
	}

	void ProxiesConfigWidget::on_EditListsButton__released ()
	{
		const int row = Ui_.ProxiesList_->currentIndex ().row ();
		if (row < 0 || row >= Proxies_.size ())
			return;

		const auto& proxy = Proxies_.value (row);
		auto dialog = new EditListsDialog { Storage_->GetScripts (proxy), ScriptsMgr_, this };
		dialog->setAttribute (Qt::WA_DeleteOnClose);

		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[this, proxy, dialog] { Storage_->SetScripts (proxy, dialog->GetScripts ()); },
			dialog,
			SIGNAL (accepted ()),
			dialog
		};

		dialog->show ();
	}
}
}
