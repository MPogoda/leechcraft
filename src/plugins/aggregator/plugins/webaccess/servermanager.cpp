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

#include "servermanager.h"
#include <cstring>
#include <QStringList>
#include <Wt/WServer>
#include "aggregatorapp.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
namespace WebAccess
{
	namespace
	{
		class ArgcGenerator
		{
			QStringList Parms_;
		public:
			inline ArgcGenerator ()
			: Parms_ ("/usr/local/lib/leechcraft")
			{
			}

			inline void AddParm (const QString& name, const QString& parm)
			{
				Parms_ << name << parm;
			}

			inline int GetArgc () const
			{
				return Parms_.size ();
			}

			inline char** GetArgv () const
			{
				char **result = new char* [GetArgc () + 1];
				int i = 0;
				for (const auto& parm : Parms_)
				{
					result [i] = new char [parm.size () + 1];
					std::strcpy (result [i], parm.toLatin1 ());
					++i;
				}
				result [i] = 0;

				return result;
			}
		};
	}

	ServerManager::ServerManager (IProxyObject *proxy, ICoreProxy_ptr coreProxy)
	: CoreProxy_ (coreProxy)
	, Server_ (new Wt::WServer ())
	{
		Server_->addEntryPoint (Wt::Application,
				[proxy, coreProxy] (const Wt::WEnvironment& we)
					{ return new AggregatorApp (proxy, coreProxy, we); });

		reconfigureServer ();

		XmlSettingsManager::Instance ().RegisterObject ("ListenPort", this, "reconfigureServer");
	}

	void ServerManager::reconfigureServer ()
	{
		const auto port = XmlSettingsManager::Instance ().property ("ListenPort").toInt ();

		ArgcGenerator gen;
		gen.AddParm ("--docroot", "/usr/share/Wt;/favicon.ico,/resources,/style");
		gen.AddParm ("--http-address", "0.0.0.0");
		gen.AddParm ("--http-port", QString::number (port));
		Server_->setServerConfiguration (gen.GetArgc (), gen.GetArgv ());

		if (Server_->isRunning ())
			Server_->stop ();

		Server_->start ();
	}
}
}
}
