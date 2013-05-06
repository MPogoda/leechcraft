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

#include "accountpropsdialog.h"
#include <QtDebug>
#include "structures.h"

namespace LeechCraft
{
namespace Poleemery
{
	AccountPropsDialog::AccountPropsDialog (QWidget *parent)
	: QDialog (parent)
	, CurrentAccID_ (-1)
	{
		Ui_.setupUi (this);

		Ui_.AccType_->addItem (ToHumanReadable (AccType::Cash));
		Ui_.AccType_->addItem (ToHumanReadable (AccType::BankAccount));

		QStringList currencies;
		for (auto language = 2; language < 214; ++language)
			for (auto country = 1; country < 247; ++country)
			{
				const QLocale locale (static_cast<QLocale::Language> (language), static_cast<QLocale::Country> (country));
				currencies << locale.currencySymbol (QLocale::CurrencyIsoCode);
			}

		std::sort (currencies.begin (), currencies.end ());
		currencies.erase (std::unique (currencies.begin (), currencies.end ()), currencies.end ());
		currencies.removeAll (QString ());

		Ui_.Currency_->addItems (currencies);
	}

	void AccountPropsDialog::SetAccount (const Account& account)
	{
		CurrentAccID_ = account.ID_;
		Ui_.AccType_->setCurrentIndex (static_cast<int> (account.Type_));
		Ui_.AccName_->setText (account.Name_);

		const auto pos = Ui_.Currency_->findText (account.Currency_);
		if (pos >= 0)
			Ui_.Currency_->setCurrentIndex (pos);
	}

	Account AccountPropsDialog::GetAccount () const
	{
		return
		{
			CurrentAccID_,
			static_cast<AccType> (Ui_.AccType_->currentIndex ()),
			Ui_.AccName_->text (),
			Ui_.Currency_->currentText ()
		};
	}
}
}
