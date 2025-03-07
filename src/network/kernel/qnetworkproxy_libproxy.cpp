// Copyright (C) 2016 The Qt Company Ltd.
// Copyright (C) 2017 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qnetworkproxy.h"

#ifndef QT_NO_NETWORKPROXY

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QSemaphore>
#include <QtCore/QUrl>
#include <QtCore/private/qeventdispatcher_unix_p.h>
#include <QtCore/private/qthread_p.h>
#include <QtCore/qapplicationstatic.h>

#include <proxy.h>
#include <dlfcn.h>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

static bool isThreadingNeeded()
{
    // Try to guess if the libproxy we linked to is from the libproxy project
    // or if it is from pacrunner. Neither library is thread-safe, but the one
    // from libproxy is worse, since it may launch JS engines that don't take
    // kindly to being executed from multiple threads (even if at different
    // times). The pacrunner implementation doesn't suffer from this because
    // the JS execution is out of process, in the pacrunner daemon.

    void *sym;

#ifdef Q_CC_GNU
    // Search for the mangled name of the virtual table of the pacrunner
    // extension. Even if libproxy begins using -fvisibility=hidden, this
    // symbol can't be hidden.
    sym = dlsym(RTLD_DEFAULT, "_ZTVN8libproxy19pacrunner_extensionE");
#else
    // The default libproxy one uses libmodman for its module management and
    // leaks symbols because it doesn't use -fvisibility=hidden (as of
    // v0.4.15).
    sym = dlsym(RTLD_DEFAULT, "mm_info_ignore_hostname");
#endif

    return sym != nullptr;
}

class QLibProxyWrapper : public QDaemonThread
{
    Q_OBJECT
public:
    QLibProxyWrapper();
    ~QLibProxyWrapper();

    QList<QUrl> getProxies(const QUrl &url);

private:
    struct Data {
        // we leave the conversion to/from QUrl to the calling thread
        const char *url;
        char **proxies;
        QSemaphore replyReady;
    };

    void run() override;

    pxProxyFactory *factory;    // not subject to the mutex

    QMutex mutex;
    QSemaphore requestReady;
    Data *request;
};

Q_APPLICATION_STATIC(QLibProxyWrapper, libProxyWrapper)

QLibProxyWrapper::QLibProxyWrapper()
{
    if (isThreadingNeeded()) {
        setEventDispatcher(new QEventDispatcherUNIX);   // don't allow the Glib one
        start();
    } else {
        factory = px_proxy_factory_new();
        Q_CHECK_PTR(factory);
    }
}

QLibProxyWrapper::~QLibProxyWrapper()
{
    if (isRunning()) {
        requestInterruption();
        requestReady.release();
        wait();
    } else {
        px_proxy_factory_free(factory);
    }
}

/*
    Gets the list of proxies from libproxy, converted to QUrl list. Apply
    thread-safety, though its documentation says otherwise, libproxy isn't
    thread-safe.
*/
QList<QUrl> QLibProxyWrapper::getProxies(const QUrl &url)
{
    QByteArray encodedUrl = url.toEncoded();
    Data data;
    data.url = encodedUrl.constData();

    {
        QMutexLocker locker(&mutex);
        if (isRunning()) {
            // threaded mode
            // it's safe to write to request because we hold the mutex:
            // our aux thread is blocked waiting for work and no other thread
            // could have got here
            request = &data;
            requestReady.release();

            // wait for the reply
            data.replyReady.acquire();
        } else {
            // non-threaded mode
            data.proxies = px_proxy_factory_get_proxies(factory, data.url);
        }
    }

    QList<QUrl> ret;
    if (data.proxies) {
        for (int i = 0; data.proxies[i]; i++) {
            ret.append(QUrl::fromEncoded(data.proxies[i]));
            free(data.proxies[i]);
        }
        free(data.proxies);
    }
    return ret;
}

void QLibProxyWrapper::run()
{
    factory = px_proxy_factory_new();
    Q_CHECK_PTR(factory);

    forever {
        requestReady.acquire();
        if (isInterruptionRequested())
            break;
        request->proxies = px_proxy_factory_get_proxies(factory, request->url);
        request->replyReady.release();
    }

    px_proxy_factory_free(factory);
}

QList<QNetworkProxy> QNetworkProxyFactory::systemProxyForQuery(const QNetworkProxyQuery &query)
{
    QList<QNetworkProxy> proxyList;

    QUrl queryUrl;
    QNetworkProxy::Capabilities requiredCapabilities(0);
    switch (query.queryType()) {
    //URL requests are directly supported by libproxy
    case QNetworkProxyQuery::UrlRequest:
        queryUrl = query.url();
        break;
    // fake URLs to get libproxy to tell us the SOCKS proxy
    case QNetworkProxyQuery::TcpSocket:
        if (queryUrl.scheme().isEmpty())
            queryUrl.setScheme(QStringLiteral("tcp"));
        queryUrl.setHost(query.peerHostName());
        queryUrl.setPort(query.peerPort());
        requiredCapabilities |= QNetworkProxy::TunnelingCapability;
        break;
    case QNetworkProxyQuery::UdpSocket:
        if (queryUrl.scheme().isEmpty())
            queryUrl.setScheme(QStringLiteral("udp"));
        queryUrl.setHost(query.peerHostName());
        queryUrl.setPort(query.peerPort());
        requiredCapabilities |= QNetworkProxy::UdpTunnelingCapability;
        break;
    default:
        proxyList.append(QNetworkProxy(QNetworkProxy::NoProxy));
        return proxyList;
    }

    const QList<QUrl> rawProxies = libProxyWrapper()->getProxies(queryUrl);

    bool haveDirectConnection = false;
    for (const QUrl& url : rawProxies) {
        QNetworkProxy::ProxyType type;
        const QString scheme = url.scheme();
        if (scheme == "http"_L1) {
            type = QNetworkProxy::HttpProxy;
        } else if (scheme == "socks"_L1 || scheme == "socks5"_L1) {
            type = QNetworkProxy::Socks5Proxy;
        } else if (scheme == "ftp"_L1) {
            type = QNetworkProxy::FtpCachingProxy;
        } else if (scheme == "direct"_L1) {
            type = QNetworkProxy::NoProxy;
            haveDirectConnection = true;
        } else {
            continue; //unsupported proxy type e.g. socks4
        }

        QNetworkProxy proxy(type,
            url.host(QUrl::EncodeUnicode),
            url.port(0),
            url.userName(QUrl::FullyDecoded),
            url.password(QUrl::FullyDecoded));

        if ((proxy.capabilities() & requiredCapabilities) == requiredCapabilities)
            proxyList.append(proxy);
    }

    // fallback is direct connection
    if (proxyList.isEmpty() || !haveDirectConnection)
        proxyList.append(QNetworkProxy(QNetworkProxy::NoProxy));

    return proxyList;
}

QT_END_NAMESPACE

#include "qnetworkproxy_libproxy.moc"

#endif
