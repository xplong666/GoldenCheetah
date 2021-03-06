/*
 * Copyright (c) 2010 Justin Knotzke (jknotzke@shampoo.ca)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "Secrets.h"

#include "OAuthDialog.h"
#include "Athlete.h"
#include "Context.h"
#include "Settings.h"
#include "TimeUtils.h"

#if QT_VERSION > 0x050000
#include "GoogleDrive.h"

#include <QJsonParseError>
#endif

OAuthDialog::OAuthDialog(Context *context, OAuthSite site, QString baseURL, QString clientsecret) :
    context(context), site(site), baseURL(baseURL), clientsecret(clientsecret)
{

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("OAuth"));

    // check if SSL is available - if not - message and end
    if (!QSslSocket::supportsSsl()) {
        QString text = QString(tr("SSL Security Libraries required for 'Authorise' are missing in this installation."));
        QMessageBox sslMissing(QMessageBox::Critical, tr("Authorization Error"), text);
        sslMissing.exec();
        noSSLlib = true;
        return;
    }

    // SSL is available - so authorisation can take place
    noSSLlib = false;

    layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(2,0,2,2);
    setLayout(layout);


    #if defined(NOWEBKIT) || (QT_VERSION > 0x050000 && defined(Q_OS_MAC))
    view = new QWebEngineView();
    #else
    view = new QWebView();
    #endif

    view->setContentsMargins(0,0,0,0);
    view->page()->view()->setContentsMargins(0,0,0,0);
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view->setAcceptDrops(false);
    layout->addWidget(view);

    QString urlstr = "";
    if (site == STRAVA) {
        urlstr = QString("https://www.strava.com/oauth/authorize?");
        urlstr.append("client_id=").append(GC_STRAVA_CLIENT_ID).append("&");
        urlstr.append("scope=view_private,write&");
        urlstr.append("redirect_uri=http://www.goldencheetah.org/&");
        urlstr.append("response_type=code&");
        urlstr.append("approval_prompt=force");

    } else if (site == DROPBOX) {

        urlstr = QString("https://www.dropbox.com/1/oauth2/authorize?");

#ifdef GC_DROPBOX_CLIENT_ID
        urlstr.append("client_id=").append(GC_DROPBOX_CLIENT_ID).append("&");
#endif
        urlstr.append("redirect_uri=https://goldencheetah.github.io/blank.html&");
        urlstr.append("response_type=code&");
        urlstr.append("force_reapprove=true");
    } else if (site == TWITTER) {

#ifdef GC_HAVE_KQOAUTH
        oauthRequest = new KQOAuthRequest;
        oauthManager = new KQOAuthManager(this);

        connect(oauthManager, SIGNAL(temporaryTokenReceived(QString,QString)),
                this, SLOT(onTemporaryTokenReceived(QString, QString)));

        connect(oauthManager, SIGNAL(authorizationReceived(QString,QString)),
                this, SLOT( onAuthorizationReceived(QString, QString)));

        connect(oauthManager, SIGNAL(accessTokenReceived(QString,QString)),
                this, SLOT(onAccessTokenReceived(QString,QString)));

        connect(oauthManager, SIGNAL(requestReady(QByteArray)),
                this, SLOT(onRequestReady(QByteArray)));

        connect(oauthManager, SIGNAL(authorizationPageRequested(QUrl)),
                this, SLOT(onAuthorizationPageRequested(QUrl)));


        oauthRequest->initRequest(KQOAuthRequest::TemporaryCredentials, QUrl("https://api.twitter.com/oauth/request_token"));

        oauthRequest->setConsumerKey(GC_TWITTER_CONSUMER_KEY);
        oauthRequest->setConsumerSecretKey(GC_TWITTER_CONSUMER_SECRET);

        oauthManager->setHandleUserAuthorization(true);
        oauthManager->setHandleAuthorizationPageOpening(false);

        oauthManager->executeRequest(oauthRequest);

#endif
    } else if (site == CYCLING_ANALYTICS) {

        urlstr = QString("https://www.cyclinganalytics.com/api/auth?");
        urlstr.append("client_id=").append(GC_CYCLINGANALYTICS_CLIENT_ID).append("&");
        urlstr.append("scope=modify_rides&");
        urlstr.append("redirect_uri=http://www.goldencheetah.org/&");
        urlstr.append("response_type=code&");
        urlstr.append("approval_prompt=force");

    } else if (site == GOOGLE_CALENDAR) {
        // OAUTH 2.0 - Google flow for installed applications
        urlstr = QString("https://accounts.google.com/o/oauth2/auth?");
        urlstr.append("scope=https://www.googleapis.com/auth/calendar&");
        urlstr.append("redirect_uri=urn:ietf:wg:oauth:2.0:oob&");
        urlstr.append("response_type=code&");
        urlstr.append("client_id=").append(GC_GOOGLE_CALENDAR_CLIENT_ID);
#if QT_VERSION >= 0x050000
    } else if (site == GOOGLE_DRIVE) {
        const QString scope = GoogleDrive::GetScope(context);
        // OAUTH 2.0 - Google flow for installed applications
        urlstr = QString("https://accounts.google.com/o/oauth2/auth?");
        // We only request access to the application data folder, not all files.
        urlstr.append("scope=https://www.googleapis.com/auth/" + scope + "&");
        urlstr.append("redirect_uri=urn:ietf:wg:oauth:2.0:oob&");
        urlstr.append("response_type=code&");
        urlstr.append("client_id=").append(GC_GOOGLE_DRIVE_CLIENT_ID);
#endif
    } else if (site == TODAYSPLAN) {
        //urlstr = QString("https://whats.todaysplan.com.au/en/authorize/");
        if (baseURL=="") baseURL="https://whats.todaysplan.com.au";
        urlstr = QString("%1/authorize/").arg(baseURL);
        urlstr.append(GC_TODAYSPLAN_CLIENT_ID);
    } else if (site == WITHINGS) {

#ifdef GC_HAVE_KQOAUTH
        oauthRequest = new KQOAuthRequest;
        oauthManager = new KQOAuthManager(this);

        connect(oauthManager, SIGNAL(temporaryTokenReceived(QString,QString)),
                this, SLOT(onTemporaryTokenReceived(QString, QString)));

        connect(oauthManager, SIGNAL(authorizationReceived(QString,QString)),
                this, SLOT( onAuthorizationReceived(QString, QString)));

        connect(oauthManager, SIGNAL(accessTokenReceived(QString,QString)),
                this, SLOT(onAccessTokenReceived(QString,QString)));

        connect(oauthManager, SIGNAL(requestReady(QByteArray)),
                this, SLOT(onRequestReady(QByteArray)));

        connect(oauthManager, SIGNAL(authorizationPageRequested(QUrl)),
                this, SLOT(onAuthorizationPageRequested(QUrl)));


        oauthRequest->initRequest(KQOAuthRequest::TemporaryCredentials, QUrl("https://oauth.withings.com/account/request_token"));
        //oauthRequest->setEnableDebugOutput(true);
        oauthRequest->setHttpMethod(KQOAuthRequest::GET);

        oauthRequest->setConsumerKey(GC_WITHINGS_CONSUMER_KEY);
        oauthRequest->setConsumerSecretKey(GC_WITHINGS_CONSUMER_SECRET);
        //oauthRequest->setCallbackUrl(QUrl("http://www.goldencheetah.org"));

        oauthManager->setHandleUserAuthorization(true); // false to use callback
        oauthManager->setHandleAuthorizationPageOpening(false);

        oauthManager->executeRequest(oauthRequest);

#endif
    }

    // different process to get the token for STRAVA, CYCLINGANALYTICS vs.
    // TWITTER or WITHINGS
    if (site == DROPBOX || site == STRAVA || site == CYCLING_ANALYTICS ||
        site == GOOGLE_CALENDAR || site == GOOGLE_DRIVE || site == TODAYSPLAN) {
        url = QUrl(urlstr);
        view->setUrl(url);
        // connects
        connect(view, SIGNAL(urlChanged(const QUrl&)), this,
                SLOT(urlChanged(const QUrl&)));
        connect(view, SIGNAL(loadFinished(bool)), this,
                SLOT(loadFinished(bool)));
    }
}

#ifdef GC_HAVE_KQOAUTH
// ****************** OAUTH V1 (TWITTER, WITHINGS)  ************************************************

void OAuthDialog::onTemporaryTokenReceived(QString, QString)
{
    //qDebug() << "onTemporaryTokenReceived";
    QUrl userAuthURL;

    if (site == TWITTER) {
        userAuthURL = "https://api.twitter.com/oauth/authorize";
    } else if (site == WITHINGS) {
        userAuthURL = "https://oauth.withings.com/account/authorize";
    }

    if( oauthManager->lastError() == KQOAuthManager::NoError) {
        oauthManager->getUserAuthorization(userAuthURL);
    } else
        qDebug() << "error" << oauthManager->lastError();

}

void OAuthDialog::onAuthorizationReceived(QString, QString) {
    //qDebug() << "Authorization token received: " << token << verifier;

    if (site == TWITTER) {
        oauthManager->getUserAccessTokens(QUrl("https://api.twitter.com/oauth/access_token"));
    } else if (site == WITHINGS) {
        oauthManager->getUserAccessTokens(QUrl("https://oauth.withings.com/account/access_token"));
    }

    if( oauthManager->lastError() != KQOAuthManager::NoError) {
        QString error = QString(tr("Error fetching OAuth credentials - Endpoint: /oauth/access_token"));
        QMessageBox oautherr(QMessageBox::Critical, tr("Authorization Error"), error);
        oautherr.exec();
        accept();
    }
}

void OAuthDialog::onAccessTokenReceived(QString token, QString tokenSecret) {
    //qDebug() << "Access token received: " << token << tokenSecret;

    QString info;
    if (site == TWITTER) {
        appsettings->setCValue(context->athlete->cyclist, GC_TWITTER_TOKEN, token);
        appsettings->setCValue(context->athlete->cyclist, GC_TWITTER_SECRET,  tokenSecret);

        info = QString(tr("Twitter authorization was successful."));
    } else if (site == WITHINGS) {
        appsettings->setCValue(context->athlete->cyclist, GC_WITHINGS_TOKEN, token);
        appsettings->setCValue(context->athlete->cyclist, GC_WITHINGS_SECRET,  tokenSecret);

        info = QString(tr("Withings authorization was successful."));
    }


    QMessageBox information(QMessageBox::Information, tr("Information"), info);
    information.exec();
    accept();
}

void OAuthDialog::onAuthorizedRequestDone() {
    // request sent - do nothing
}

void OAuthDialog::onRequestReady(QByteArray response) {
    //qDebug() << "Response received: " << response;

    QString r = response;
    if (r.contains("\"errors\"", Qt::CaseInsensitive))
    {
        QMessageBox oautherr(QMessageBox::Critical, tr("Error in authorization"),
             tr("There was an error during authorization. Please check the error description."));
             oautherr.setDetailedText(r); // probably blank
         oautherr.exec();
    } else {
        if (site == WITHINGS) {
            QString userid;

    #if QT_VERSION > 0x050000
            QUrlQuery params;
            params.setQuery(response);
    #else
            QUrl params;
            params.setEncodedQuery(response);
    #endif
            userid = params.queryItemValue("userid");

            if (userid.isEmpty() == false) {
                appsettings->setCValue(context->athlete->cyclist, GC_WIUSER, userid);
            }
        }
    }
}


void OAuthDialog::onAuthorizationPageRequested(QUrl url) {
    //qDebug() << "AuthorizationPageRequested: " << url;

    // open Authorization page in view
    view->setUrl(url);

}
#endif

// ****************** Strava / Cyclinganalytics / Google authorization ***************************************

void
OAuthDialog::urlChanged(const QUrl &url)
{
    // STRAVA & CYCLINGANALYTICS work with Call-back URLs / change of URL indicates next step is required
    if (site == DROPBOX || site == STRAVA || site == CYCLING_ANALYTICS || site == TODAYSPLAN) {
        if (url.toString().startsWith("http://www.goldencheetah.org/?state=&code=") ||
                url.toString().contains("blank.html?code=") ||
                url.toString().startsWith("http://www.goldencheetah.org/?code=")) {
            QString code = url.toString().right(url.toString().length()-url.toString().indexOf("code=")-5);
            QByteArray data;
#if QT_VERSION > 0x050000
            QUrlQuery params;
#else
            QUrl params;
#endif
            QString urlstr = "";

            // now get the final token to store
            if (site == DROPBOX) {
                urlstr = QString("https://api.dropboxapi.com/1/oauth2/token?");
                urlstr.append("redirect_uri=https://goldencheetah.github.io/blank.html&");
                params.addQueryItem("grant_type", "authorization_code");
#ifdef GC_DROPBOX_CLIENT_ID
                params.addQueryItem("client_id", GC_DROPBOX_CLIENT_ID);
#endif
#ifdef GC_DROPBOX_CLIENT_SECRET
                params.addQueryItem("client_secret", GC_DROPBOX_CLIENT_SECRET);
#endif
            }

            // now get the final token to store
            else if (site == STRAVA) {
                urlstr = QString("https://www.strava.com/oauth/token?");
                params.addQueryItem("client_id", GC_STRAVA_CLIENT_ID);
#ifdef GC_STRAVA_CLIENT_SECRET
                params.addQueryItem("client_secret", GC_STRAVA_CLIENT_SECRET);
#endif
            }

            else if (site == CYCLING_ANALYTICS) {
                urlstr = QString("https://www.cyclinganalytics.com/api/token?");
                params.addQueryItem("client_id", GC_CYCLINGANALYTICS_CLIENT_ID);
#ifdef GC_CYCLINGANALYTICS_CLIENT_SECRET
                params.addQueryItem("client_secret", GC_CYCLINGANALYTICS_CLIENT_SECRET);
#endif
                params.addQueryItem("grant_type", "authorization_code");

            }

            else if (site == TODAYSPLAN) {
                if (baseURL=="") baseURL="https://whats.todaysplan.com.au";
                urlstr = QString("%1/rest/oauth/access_token?").arg(baseURL);
                params.addQueryItem("client_id", GC_TODAYSPLAN_CLIENT_ID);
#ifdef GC_TODAYSPLAN_CLIENT_SECRET
                if (clientsecret != "")
                    params.addQueryItem("client_secret", clientsecret);
                else
                    params.addQueryItem("client_secret", GC_TODAYSPLAN_CLIENT_SECRET);

#endif
                params.addQueryItem("grant_type", "authorization_code");
                params.addQueryItem("redirect_uri", "https://goldencheetah.github.io/blank.html");

            }

            params.addQueryItem("code", code);

#if QT_VERSION > 0x050000
            data.append(params.query(QUrl::FullyEncoded));
#else
            data=params.encodedQuery();
#endif

            // trade-in the temporary access code retrieved by the Call-Back URL for the finale token
            QUrl url = QUrl(urlstr);
            QNetworkRequest request = QNetworkRequest(url);
            request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");

            // now get the final token - but ignore errors
            manager = new QNetworkAccessManager(this);
            connect(manager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(onSslErrors(QNetworkReply*, const QList<QSslError> & )));
            connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkRequestFinished(QNetworkReply*)));
            manager->post(request, data);

        }
    }
}

// just ignore handshake errors
void
OAuthDialog::onSslErrors(QNetworkReply *reply, const QList<QSslError>&)
{
    reply->ignoreSslErrors();
}

void
OAuthDialog::loadFinished(bool ok) {

    // GOOGLE OAUTH 2.0 sends the code as part of the title of the HTML page they re-direct too
    if (site == GOOGLE_CALENDAR || site == GOOGLE_DRIVE) {
        if (ok && url.toString().startsWith(
                "https://accounts.google.com/o/oauth2/auth")) {

            // retrieve the code from the HTML page title
            QString title = view->title();
            if (title.contains("code")) {
                QString code = title.right(title.length()-title.indexOf("code=")-5);
                QByteArray data;
#if QT_VERSION > 0x050000
                QUrlQuery params;
#else
                QUrl params;
#endif
                QString urlstr = "https://www.googleapis.com/oauth2/v3/token?";
                if (site == GOOGLE_CALENDAR) {
                    params.addQueryItem("client_id",
                                        GC_GOOGLE_CALENDAR_CLIENT_ID);
                } else {
                    params.addQueryItem("client_id",
                                        GC_GOOGLE_DRIVE_CLIENT_ID);
                }
                if (site == GOOGLE_CALENDAR) {
                    params.addQueryItem("client_secret",
                                        GC_GOOGLE_CALENDAR_CLIENT_SECRET);
                }
                if (site == GOOGLE_DRIVE) {
                    params.addQueryItem("client_secret",
                                        GC_GOOGLE_DRIVE_CLIENT_SECRET);
                }
                params.addQueryItem("code", code);
                params.addQueryItem("redirect_uri", "urn:ietf:wg:oauth:2.0:oob");
                params.addQueryItem("grant_type", "authorization_code");

#if QT_VERSION > 0x050000
                data.append(params.query(QUrl::FullyEncoded));
#else
                data=params.encodedQuery();
#endif

                // trade-in the temporary access code retrieved by the
                // Call-Back URL for the finale token
                QUrl url = QUrl(urlstr);
                QNetworkRequest request = QNetworkRequest(url);
                request.setHeader(QNetworkRequest::ContentTypeHeader,
                                  "application/x-www-form-urlencoded");

                // not get the final token - ignoring errors
                manager = new QNetworkAccessManager(this);
                connect(manager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(onSslErrors(QNetworkReply*, const QList<QSslError> & )));
                connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkRequestFinished(QNetworkReply*)));
                manager->post(request, data);
            }
        }
    }
}

#if QT_VERSION < 0x050000
static QString RawJsonStringGrab(const QByteArray& payload,
                                 const QString& needle) {
    // A RegExp based JSON string parser. Not the best, but it does the job.
    QString regex =
        // This matches the key.
        "(" + needle + "|\"" + needle + "\"|'" + needle + "')"
        // Matches the separator.
        "[\\s]*:[\\s]*"
        // matches the value.
        "(\"\\S+\"|'\\S+')";
    QRegExp q(regex);
    if (!q.isValid()) {
        // Somehow failed to build the regex.
        return "";
    }
    int start = q.indexIn(payload);
    int pos = q.pos(2);
    if (start == -1 || pos == -1) {
        // Failed to find the key or the value.
        return "";
    }
    QString ret = payload.mid(pos, q.matchedLength() + start - pos);
    // Remove " or ' from the value.
    ret.remove(0, 1);
    ret.remove(ret.size() - 1, 1);
    return ret;
}
#endif

void OAuthDialog::networkRequestFinished(QNetworkReply *reply) {

    // we can handle SSL handshake errors, if we got here then some kind of protocol was agreed
    if (reply->error() == QNetworkReply::NoError || reply->error() == QNetworkReply::SslHandshakeFailedError) {
        QByteArray payload = reply->readAll(); // JSON
        QString refresh_token;
        QString access_token;
#if QT_VERSION > 0x050000
        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            refresh_token = document.object()["refresh_token"].toString();
            access_token = document.object()["access_token"].toString();
        }
#else
        refresh_token = RawJsonStringGrab(payload, "refresh_token");
        access_token = RawJsonStringGrab(payload, "access_token");
#endif
        if (((site == GOOGLE_DRIVE || site == GOOGLE_CALENDAR) &&
             refresh_token == "" ) || access_token == "") {
            // Something failed.
            // Only Google uses both refresh and access tokens.
            QString error = QString(
                tr("Error retrieving authoriation credentials"));
            QMessageBox oautherr(QMessageBox::Critical,
                                 tr("Authorization Error"), error);
            oautherr.setDetailedText(error);
            oautherr.exec();
            return;
        }

        if (site == DROPBOX) {
            appsettings->setCValue(context->athlete->cyclist, GC_DROPBOX_TOKEN,
                                   access_token);
            QString info = QString(tr("Dropbox authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();
        } else if (site == STRAVA) {
            appsettings->setCValue(context->athlete->cyclist, GC_STRAVA_TOKEN, access_token);
            QString info = QString(tr("Strava authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();
        } else if (site == CYCLING_ANALYTICS) {
            appsettings->setCValue(context->athlete->cyclist, GC_CYCLINGANALYTICS_TOKEN, access_token);
            QString info = QString(tr("Cycling Analytics authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();
        } else if (site == GOOGLE_CALENDAR) {
            // remove the Google Page first
            url = QUrl("http://www.goldencheetah.org");
            view->setUrl(url);
            appsettings->setCValue(
                context->athlete->cyclist,
                GC_GOOGLE_CALENDAR_REFRESH_TOKEN, refresh_token);
            QString info = QString(
                tr("Google Calendar authorization was successful."));
            QMessageBox information(QMessageBox::Information,
                                    tr("Information"), info);
            information.exec();
        } else if (site == GOOGLE_DRIVE) {
            // remove the Google Page first
            appsettings->setCValue(
                context->athlete->cyclist,
                GC_GOOGLE_DRIVE_REFRESH_TOKEN, refresh_token);
            appsettings->setCValue(
                context->athlete->cyclist,
                GC_GOOGLE_DRIVE_ACCESS_TOKEN, access_token);
            appsettings->setCValue(
                context->athlete->cyclist,
                GC_GOOGLE_DRIVE_LAST_ACCESS_TOKEN_REFRESH,
                QDateTime::currentDateTime());

            QString info = QString(
                tr("Google Drive authorization was successful."));
            QMessageBox information(QMessageBox::Information,
                                    tr("Information"), info);
            information.exec();
        } else if (site == TODAYSPLAN) {
            appsettings->setCValue(context->athlete->cyclist, GC_TODAYSPLAN_TOKEN, access_token);

            listUsers();
        }
    } else {

            QString error = QString(tr("Error retrieving access token, %1 (%2)")).arg(reply->errorString()).arg(reply->error());
            QMessageBox oautherr(QMessageBox::Critical, tr("SSL Token Refresh Error"), error);
            oautherr.setDetailedText(error);
            oautherr.exec();
    }
    // job done, dialog can be closed
    accept();
}

void
OAuthDialog::listUsers()
{
    if (site == TODAYSPLAN) {
        // use the configed URL
        QString url = QString("%1/rest/users/delegates/users")
          .arg(appsettings->cvalue(context->athlete->cyclist, GC_TODAYSPLAN_URL, "https://whats.todaysplan.com.au").toString());

        // request using the bearer token
        QNetworkRequest request(url);
        QString token = appsettings->cvalue(context->athlete->cyclist, GC_TODAYSPLAN_TOKEN, "").toString();
        request.setRawHeader("Authorization", (QString("Bearer %1").arg(token)).toLatin1());

        QNetworkAccessManager *man = new QNetworkAccessManager(this);
        connect(man, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(onSslErrors(QNetworkReply*, const QList<QSslError> & )));

        QNetworkReply *reply = man->get(request);

        // blocking request
        QEventLoop loop;
        connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();

        // did we get a good response ?
        QByteArray r = reply->readAll();

        #if QT_VERSION > 0x050000
            QJsonParseError parseError;
            QJsonDocument document = QJsonDocument::fromJson(r, &parseError);


            if (parseError.error == QJsonParseError::NoError) {
                //qDebug() << "response: " << r;
                if (document.array().count()>1) {
                    layout->removeWidget(view);
                    delete view;

                    layout->setMargin(10);
                    QLabel *label = new QLabel(tr("Select an athlete"));
                    label->setFont(QFont("Helvetica", 15, QFont::Bold));

                    layout->addWidget(label);
                    layout->addSpacing(5);

                    QList<QCheckBox*> users;

                    for (int i=0;i<document.array().count();i++) {
                        if (document.array()[i].toObject()["relationship"].toString() == "coach" ||
                            document.array()[i].toObject()["relationship"].toString() == "manager") {
                            QCheckBox *ck = new QCheckBox(document.array()[i].toObject()["_name"].toString());
                            ck->setFont(QFont("Helvetica", 15));
                            ck->setProperty("id", document.array()[i].toObject()["id"].toInt());
                            ck->setProperty("name", document.array()[i].toObject()["_name"].toString());
                            users.append(ck);
                            layout->addWidget(ck);
                            //qDebug()<< document.array()[i].toObject()["id"].toInt() << document.array()[i].toObject()["_name"].toString();
                        }
                    }
                    layout->addSpacing(5);
                    QPushButton *btnSelect = new QPushButton(tr("Select"));
                    btnSelect->setMaximumWidth(200);
                    layout->addWidget(btnSelect);
                    layout->addStretch();

                    QEventLoop loop;
                    connect(btnSelect, SIGNAL(clicked()), &loop, SLOT(quit()));
                    loop.exec();

                    for (int i=0;i<users.count();i++) {
                        QCheckBox *ck = users.at(i);
                        if (ck->isChecked()) {
                            appsettings->setCValue(context->athlete->cyclist, GC_TODAYSPLAN_ATHLETE_ID, ck->property("id"));
                            appsettings->setCValue(context->athlete->cyclist, GC_TODAYSPLAN_ATHLETE_NAME, ck->property("name"));

                            return;
                        }

                    }
                }
            } else {
                //qDebug() << "response: " << r << "error:" << reply->errorString();
            }
        #endif

        // Use default athlete
        QString info = QString(tr("Today's Plan authorization was successful."));
        QMessageBox information(QMessageBox::Information, tr("Information"), info);
        information.exec();
    }
}
