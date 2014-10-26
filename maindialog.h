#ifndef MAINDIALOG_H
#define MAINDIALOG_H

#include <QAction>
#include <QDialog>
#include <QHostAddress>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSystemTrayIcon>

// HTTP status codes used by YDNS API v1
enum
{
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404
};

namespace Ui {
class MainDialog;
}

class MainDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MainDialog(QWidget *parent = 0);
    ~MainDialog();

private:
    void createActions();
    QNetworkRequest createRequest(const QUrl& url);
    void createTrayIcon();
    void createTimer();

    Ui::MainDialog *ui;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_actionStatus;
    QAction *m_actionQuit;
    QTimer *m_timer;
    QNetworkAccessManager m_netAccessMgr;
    QNetworkReply *m_netReply;
    QHostAddress m_lastAddress;


private slots:
    void checkForUpdate();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void readReplyForCurrentAddress();
    void readReplyForUpdate();
    void updateHost();

    void on_pushButton_clicked();

protected:
    void changeEvent(QEvent *event);
    void closeEvent(QCloseEvent *event);
};

#endif // MAINDIALOG_H
