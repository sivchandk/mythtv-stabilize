#ifndef LOGVIEWER_H_
#define LOGVIEWER_H_

// qt
#include <QTimer>

// myth
#include <libmythui/mythscreentype.h>

class MythUIButton;
class MythUIButtonList;
class MythUIText;

void showLogViewer(void);

class LogViewer : public MythScreenType
{
  Q_OBJECT

  public:

    LogViewer(MythScreenStack *parent);
   ~LogViewer(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *e);
    void setFilenames(void);
  protected slots:
    void cancelClicked(void);
    void updateClicked(void);

    bool loadFile(QString filename, QStringList &list, int startline);
    void loadUrl(QString url);
    void showPrivacyLog(void);
    void showFullLog(void);
    void showAdminLog(void);
    void showErrataLog(void);
    void showMenu(void);
    void showWebProfile(void);
    void updateLogItem(MythUIButtonListItem *item);
    void openLogURL(MythUIButtonListItem *item);
    void submitProfile(void);
    void deleteProfile(void);
    void storeSmoltUid(void);

  private:
    void Init(void);


    QString             m_currentLog;
    QString             m_privacyLog;
    QString             m_fullLog;
    QString             m_statusLog;
    QString             m_errataLog;

    MythUIButtonList   *m_logList;
    MythUIText         *m_logText;

    MythUIButton       *m_exitButton;
    MythUIButton       *m_submitButton;
    MythUIButton       *m_updateButton;
};

#endif
