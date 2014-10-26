/*
 * This file is part of YDNS Updater.
 *
 * Copyright (c) 2014 TFMT UG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <QCloseEvent>
#include <QDebug>
#include <QHostAddress>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QUrlQuery>
#include "maindialog.h"
#include "ui_maindialog.h"
#include "version.h"

MainDialog::MainDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MainDialog),
      m_trayIcon(0),
      m_trayMenu(0),
      m_timer(0)
{
    ui->setupUi(this);

    setLayout(ui->verticalLayout);
    ui->groupBox->setLayout(ui->verticalLayoutGroup);

    /* load configuration */
    QSettings settings;

    if (settings.contains("host"))
        ui->host->setText(settings.value("host").toString());
    if (settings.contains("email"))
        ui->email->setText(settings.value("email").toString());
    if (settings.contains("password"))
        ui->password->setText(settings.value("password").toString());

    createActions();
    createTrayIcon();
    createTimer();

    /* show tray icons */
    m_trayIcon->show();

    /* check instant update */
    QTimer::singleShot(0, this, SLOT(checkForUpdate()));
}

MainDialog::~MainDialog()
{
    delete ui;

    if (m_actionStatus)
        delete m_actionStatus;
    if (m_actionQuit)
        delete m_actionQuit;
    if (m_trayMenu)
        delete m_trayMenu;
    if (m_trayIcon)
        delete m_trayIcon;
    if (m_timer)
        delete m_timer;
}

/**
 * Catch change events for the window state, so we can react appropriately
 * on window state changes.
 *
 * If a user minimizes the dialog, it should be set to hidden, as it's
 * minimized to system tray.
 *
 * @param event Pointer to QEvent
 */
void MainDialog::changeEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::WindowStateChange:
        if (windowState() & Qt::WindowMinimized)
            QTimer::singleShot(0, this, SLOT(hide()));
        break;
    default:
        break;
    }

    QDialog::changeEvent(event);
}

/**
 * Check whether a update to the host is required. [slot]
 *
 * Instead of simply updating a record again and again, we'll use ask the
 * YDNS server for our WAN ip address.
 */
void MainDialog::checkForUpdate()
{
    const QUrl url("https://ydns.eu/api/v1/ip");
    QNetworkRequest request = createRequest(url);

    m_netReply = m_netAccessMgr.get(request);
    connect(m_netReply, SIGNAL(finished()),
            this, SLOT(readReplyForCurrentAddress()));
}

/**
 * Create a request object with appropriate headers.
 *
 * @param url URL for the request
 */
QNetworkRequest MainDialog::createRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    QSettings settings;

    request.setRawHeader("User-agent", "YDNS Updater/" YDNS_UPDATER_VERSION);

    /* add authorization header */
    QString concatenated = settings.value("email").toString()
            + ":" + settings.value("password").toString();
    QByteArray data = concatenated.toLocal8Bit().toBase64();
    QString headerData = "Basic " + data;
    request.setRawHeader("Authorization", headerData.toLocal8Bit());

    return request;
}

/**
 * Read reply from YDNS server about our current WAN IP address.
 *
 * The IP address is compared with a stored one (if available); if the address
 * has not changed, we'll not perform a update at all.
 */
void MainDialog::readReplyForCurrentAddress()
{
    QSettings settings;

    m_lastAddress.setAddress(QString(m_netReply->readAll()));

    if (settings.contains("lastAddress")) {
        QHostAddress lastStoreAddress(settings.value("lastAddress").toString());

        if (m_lastAddress == lastStoreAddress) {
            m_netReply->deleteLater();
            return;
        }
    }

    m_netReply->deleteLater();
    m_netReply = 0;

    // workaround: have to do this, since deleteLater() on the current
    // m_netReply object is not done yet
    QTimer::singleShot(0, this, SLOT(updateHost()));
}

/**
 * Read reply from YDNS server about the update call.
 *
 * This checks whether the update succeeded or not and will display
 * additional information in case of errors.
 */
void MainDialog::readReplyForUpdate()
{
    QVariant statusCode = m_netReply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute);

    if (statusCode.isValid()) {
        int status = statusCode.toInt();
        QSettings settings;
        const QString host = settings.value("host").toString();

        switch (status) {
            case HTTP_STATUS_OK: {
                m_trayIcon->showMessage(tr("YDNS Updater"),
                                        tr("Host \"%1\" updated successfully:"
                                           "\n%2")
                                            .arg(host)
                                            .arg(m_lastAddress.toString()));
                m_actionStatus->setText(tr("%1: OK").arg(host));
                m_actionStatus->setToolTip(m_lastAddress.toString());

                /* remember the last successful updated address */
                settings.setValue("lastAddress", m_lastAddress.toString());
                settings.sync();

                break;
            }

            case HTTP_STATUS_BAD_REQUEST: {
                m_trayIcon->showMessage(tr("YDNS Updater"),
                                        tr("Host update failed: Malformed "
                                           "input"),
                                        QSystemTrayIcon::Critical);
                m_actionStatus->setText(tr("%1: Input error").arg(host));
                m_actionStatus->setToolTip(tr("Input parameters are "
                                              "malformed"));
                break;
            }

            case HTTP_STATUS_FORBIDDEN: {
                m_trayIcon->showMessage(tr("YDNS Updater"),
                                        tr("Host update failed: Authentication "
                                           "failed"),
                                        QSystemTrayIcon::Critical);
                m_actionStatus->setText(tr("%1: Authentication error")
                                            .arg(host));
                m_actionStatus->setToolTip(tr("Invalid authentication "
                                              "information"));
                break;
            }

            case HTTP_STATUS_NOT_FOUND: {
                m_trayIcon->showMessage(tr("YDNS Updater"),
                                        tr("Host update failed: Host or record "
                                           "not found"),
                                        QSystemTrayIcon::Critical);
                m_actionStatus->setText(tr("%1: Object not found").arg(host));
                m_actionStatus->setToolTip(tr("Host or record not found"));
                break;
            }

            default: {
                m_trayIcon->showMessage(tr("YDNS Updater"),
                                        tr("Host update failed: %1")
                                            .arg(m_netReply->errorString()),
                                        QSystemTrayIcon::Critical);
                m_actionStatus->setText(tr("%1: Error").arg(host));
                m_actionStatus->setToolTip(m_netReply->errorString());
                break;
            }
        }

        m_actionStatus->setEnabled(true);
    }

    m_netReply->deleteLater();
    m_netReply = 0;
}

/**
 * Create a update host request.
 */
void MainDialog::updateHost()
{
    QSettings settings;
    QUrl url("https://ydns.eu/api/v1/update/");
    QUrlQuery query;
    QList<QPair<QString, QString> > queryItems;

    queryItems.append(QPair<QString, QString>(
                          "host", settings.value("host").toString()));
    query.setQueryItems(queryItems);
    url.setQuery(query);

    qDebug()<<"updateHost"<<url.toString();
    /* perform request */
    QNetworkRequest request = createRequest(url);
    m_netReply = m_netAccessMgr.get(request);
    connect(m_netReply, SIGNAL(finished()), this, SLOT(readReplyForUpdate()));
}

void MainDialog::closeEvent(QCloseEvent *event)
 {
     if (m_trayIcon->isVisible()) {
         hide();
         event->ignore();
     }
 }

void MainDialog::createActions()
{
    m_actionStatus = new QAction(tr("No update made yet"), this);
    m_actionStatus->setDisabled(true);
    connect(m_actionStatus, SIGNAL(triggered()),
            this, SLOT(show()));

    m_actionQuit = new QAction(tr("Quit"), this);
    connect(m_actionQuit, SIGNAL(triggered()),
            qApp, SLOT(quit()));
}

void MainDialog::createTimer()
{
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(checkForUpdate()));
    m_timer->start(300000);
}

void MainDialog::createTrayIcon()
{
    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction(m_actionStatus);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_actionQuit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setIcon(QIcon(":/assets/logo16.png"));
    connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
}

void MainDialog::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::DoubleClick:
        show();
        break;
    default:
        break;
    }
}

void MainDialog::on_pushButton_clicked()
{
    if (ui->host->text().isEmpty()) {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Please provide your YDNS host."));
        ui->host->setFocus();
    } else if (ui->email->text().isEmpty()) {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Please provide your YDNS username or "
                                 "E-mail address."));
        ui->email->setFocus();
    } else if (ui->password->text().isEmpty()) {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Please provide your YDNS password."));
        ui->password->setFocus();
    } else {
        QSettings settings;

        settings.setValue("host", ui->host->text());
        settings.setValue("email", ui->email->text());
        settings.setValue("password", ui->password->text());
        settings.sync();

        m_trayIcon->showMessage(tr("YDNS Updater"),
                                tr("Your settings have been saved."));
    }
}
