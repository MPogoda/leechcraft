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

#include "pendingupload.h"
#include <QFileInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <util/sll/parsejson.h>
#include <util/sll/slotclosure.h>
#include <util/sll/urloperator.h>
#include "vkaccount.h"
#include "vkentry.h"
#include "logger.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Murm
{
	PendingUpload::PendingUpload (VkEntry *entry,
			const QString& path, const QString& comment, VkAccount *acc)
	: QObject { acc }
	, Acc_ { acc }
	, Conn_ { acc->GetConnection () }
	, Path_ { path }
	, Comment_ { comment }
	, Entry_ { entry }
	{
		const auto nam = acc->GetCoreProxy ()->GetNetworkAccessManager ();
		Conn_->QueueRequest ([=] (const QString& key, const VkConnection::UrlParams_t& params)
			{
				QUrl url ("https://api.vk.com/method/docs.getUploadServer");
				Util::UrlOperator { url } ("access_token", key);

				VkConnection::AddParams (url, params);

				auto reply = nam->get (QNetworkRequest (url));
				new Util::SlotClosure<Util::DeleteLaterPolicy>
				{
					[this, reply]
					{
						HandleGotServer (reply);
						reply->deleteLater ();
					},
					reply,
					SIGNAL (finished ()),
					this
				};
				return reply;
			});
	}

	QString PendingUpload::GetSourceID () const
	{
		return Entry_ ? Entry_->GetEntryID () : QString {};
	}

	QString PendingUpload::GetName () const
	{
		return Path_;
	}

	qint64 PendingUpload::GetSize () const
	{
		return QFileInfo { Path_ }.size ();
	}

	QString PendingUpload::GetComment () const
	{
		return {};
	}

	TransferDirection PendingUpload::GetDirection () const
	{
		return TDOut;
	}

	void PendingUpload::Accept (const QString&)
	{
	}

	void PendingUpload::Abort ()
	{
	}

	void PendingUpload::HandleGotServer (QNetworkReply *reply)
	{
		if (!Conn_->CheckFinishedReply (reply))
			return;

		const auto& json = Util::ParseJson (reply, Q_FUNC_INFO);
		try
		{
			Conn_->CheckReplyData (json, reply);
		}
		catch (const VkConnection::RecoverableException&)
		{
			return;
		}
		catch (const VkConnection::UnrecoverableException& ex)
		{
			emit errorAppeared (TEProtocolError,
					tr ("Unable to get upload server from VKontakte, protocol error %1: %2.")
						.arg (ex.GetCode ())
						.arg (ex.GetMessage ()));
			emit stateChanged (TSFinished);
			return;
		}

		Acc_->GetLogger () << "got upload server:" << json;

		const QUrl uploadUrl { json.toMap () ["response"].toMap () ["upload_url"].toByteArray () };
		if (!uploadUrl.isValid ())
		{
			emit errorAppeared (TEProtocolError,
					tr ("Unable to get upload server from VKontakte"));
			emit stateChanged (TSFinished);
			return;
		}

		emit stateChanged (TSTransfer);

		const auto multipart = new QHttpMultiPart { QHttpMultiPart::FormDataType };
		QHttpPart filePart;
		filePart.setHeader (QNetworkRequest::ContentDispositionHeader,
				QString ("form-data; name=\"file\"; filename=\"%1\"")
					.arg (QFileInfo { Path_ }.fileName ()));
		const auto file = new QFile { Path_ };
		if (!file->open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot open file"
					<< file->fileName ()
					<< file->errorString ();
			emit errorAppeared (TEFileAccessError, file->errorString ());
			emit stateChanged (TSFinished);
			return;
		}

		filePart.setBodyDevice (file);
		file->setParent (multipart);
		multipart->append (filePart);

		const auto nam = Acc_->GetCoreProxy ()->GetNetworkAccessManager ();
		const auto upReply = nam->post (QNetworkRequest { uploadUrl }, multipart);
		connect (upReply,
				SIGNAL (uploadProgress (qint64, qint64)),
				this,
				SIGNAL (transferProgress (qint64, qint64)));
		connect (upReply,
				SIGNAL (finished ()),
				this,
				SLOT (handleUploadFinished ()));

		multipart->setParent (upReply);
	}

	void PendingUpload::HandleSaved (QNetworkReply *reply)
	{
		if (!Entry_)
			return;

		if (!Conn_->CheckFinishedReply (reply))
			return;

		const auto& json = Util::ParseJson (reply, Q_FUNC_INFO);
		try
		{
			Conn_->CheckReplyData (json, reply);
		}
		catch (const VkConnection::RecoverableException&)
		{
			return;
		}
		catch (const VkConnection::UnrecoverableException& ex)
		{
			emit errorAppeared (TEProtocolError,
					tr ("Unable to save document, error %1: %2.")
						.arg (ex.GetCode ())
						.arg (ex.GetMessage ()));
			emit stateChanged (TSFinished);
			return;
		}

		Acc_->GetLogger () << "got document save result server:" << json;

		const auto& docMap = json.toMap () ["response"].toList ().value (0).toMap ();

		const auto& ownerId = docMap ["owner_id"].toString ();
		const auto& docId = docMap ["id"].toString ();
		const auto& attId = "doc" + ownerId + "_" + docId;

		Conn_->SendMessage (Entry_->GetInfo ().ID_,
				Comment_,
				[this] (qulonglong) { emit stateChanged (TSFinished); },
				VkConnection::Type::Dialog,
				{ attId });
	}

	void PendingUpload::handleUploadFinished ()
	{
		const auto reply = qobject_cast<QNetworkReply*> (sender ());
		if (reply->error () != QNetworkReply::NoError)
		{
			qWarning () << Q_FUNC_INFO
					<< reply->error ()
					<< reply->errorString ();

			emit errorAppeared (TEProtocolError,
					tr ("Unable to upload document, network error %1.")
						.arg (reply->errorString ()));
			emit stateChanged (TSFinished);
			return;
		}

		const auto& json = Util::ParseJson (reply, Q_FUNC_INFO);
		const auto& replyMap = json.toMap ();

		Acc_->GetLogger () << "got upload result:" << json;

		if (replyMap.contains ("error"))
		{
			emit errorAppeared (TEProtocolError,
					tr ("Unable to upload document, server error: %1.")
						.arg (replyMap ["error"].toString ()));
			emit stateChanged (TSFinished);
			return;
		}

		const auto& str = replyMap ["file"].toString ();
		const auto nam = Acc_->GetCoreProxy ()->GetNetworkAccessManager ();
		Conn_->QueueRequest ([this, nam, str] (const QString& key, const VkConnection::UrlParams_t& params)
			{
				QUrl url ("https://api.vk.com/method/docs.save");
				Util::UrlOperator { url }
						("access_token", key)
						("file", str);

				VkConnection::AddParams (url, params);

				auto reply = nam->get (QNetworkRequest (url));
				new Util::SlotClosure<Util::DeleteLaterPolicy>
				{
					[this, reply]
					{
						HandleSaved (reply);
						reply->deleteLater ();
					},
					reply,
					SIGNAL (finished ()),
					this
				};
				return reply;
			});
	}
}
}
}
