/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2011  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#ifndef PLUGINS_AZOTH_PLUGINS_XOOX_PGPMANAGER_H
#define PLUGINS_AZOTH_PLUGINS_XOOX_PGPMANAGER_H

#include <QMap>
#include <QByteArray>
#include <QtCrypto>
#include <QXmppClientExtension.h>
#include <QXmppMessage.h>
#include <QXmppPresence.h>

class QXmppAnnotationsManager;

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	class QXmppPgpManager : public QXmppClientExtension
	{
		Q_OBJECT

	public:
		QCA::PGPKey PublicKey (const QString&) const;
		void SetPublicKey (const QString&, const QCA::PGPKey&);

		QCA::PGPKey PrivateKey () const;
		void SetPrivateKey (const QCA::PGPKey&);

		bool EncryptBody (const QCA::PGPKey&, const QByteArray&, QByteArray&);
		bool SignMessage (const QByteArray&, QByteArray&);
		bool SignPresence (const QByteArray&, QByteArray&);

		bool DecryptBody (const QByteArray&, QByteArray&);
		bool IsValidSignature (const QCA::PGPKey&, const QByteArray&, const QByteArray&);

		bool handleStanza (const QDomElement &element);

	signals:
		void encryptedMessageReceived (const QXmppMessage&);
		void signedMessageReceived (const QXmppMessage&);
		void signedPresenceReceived (const QXmppPresence&);
		void invalidSignatureReceived (const QDomElement &);

	private:
		// set presence type enum value from its string representation
		QXmppPresence::Type SetPresenceTypeFromStr (const QString&);
		// private key, used for decrypting messages
		QCA::PGPKey PrivateKey_;
		// map of userIDs and corresponding public keys
		// each user ID is a completely arbitrary value, one can use JIDs for this purpose
		QMap<QString, QCA::PGPKey> PublicKeys_;
	};
}
}
}

#endif
