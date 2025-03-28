#include "lc_global.h"
#include "lc_http.h"

#ifdef Q_OS_WIN

#include <windows.h>
#include <wininet.h>

lcHttpReply::lcHttpReply(QObject* Parent, const QString& URL)
	: QThread(Parent)
{
	mError = true;
	mAbort = false;
	mURL = URL;
}

void lcHttpReply::run()
{
	HINTERNET Session = nullptr;
	HINTERNET Request = nullptr;

	static_assert(sizeof(wchar_t) == sizeof(QChar), "Character size mismatch");

	Session = InternetOpen(L"LeoCAD", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

	if (!Session)
		return;

	Request = InternetOpenUrl(Session, (WCHAR*)mURL.data(), NULL, 0, INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP, 0);

	if (!Request)
	{
		InternetCloseHandle(Session);
		return;
	}

	for (;;)
	{
		char Buffer[1024];
		DWORD BytesRead;

		if (mAbort)
			break;

		if (!InternetReadFile(Request, Buffer, sizeof(Buffer), &BytesRead))
			break;

		if (BytesRead)
			mBuffer.append(Buffer, BytesRead);
		else
		{
			mError = false;
			break;
		}

		Sleep(0);
	}

	InternetCloseHandle(Request);
	InternetCloseHandle(Session);
}

lcHttpManager::lcHttpManager(QObject* Owner)
	: QObject(Owner)
{
}

lcHttpReply* lcHttpManager::DownloadFile(const QString& Url)
{
	lcHttpReply* Reply = new lcHttpReply(this, Url);
	connect(Reply, &QThread::finished, [this, Reply] { emit DownloadFinished(Reply); });
	Reply->start();
	return Reply;
}

#else

lcHttpManager::lcHttpManager(QObject* Owner)
	: QNetworkAccessManager(Owner)
{
	connect(this, SIGNAL(finished(QNetworkReply*)), this, SLOT(Finished(QNetworkReply*)));
}

lcHttpReply* lcHttpManager::lcHttpManager::DownloadFile(const QString& Url)
{
	QNetworkRequest Request = QNetworkRequest(QUrl(Url));

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0)) // default changed in Qt6
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
	Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#elif (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	Request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
#endif

	return (lcHttpReply*)get(Request);
}

void lcHttpManager::Finished(QNetworkReply* Reply)
{
	emit DownloadFinished((lcHttpReply*)Reply);
}

#endif
