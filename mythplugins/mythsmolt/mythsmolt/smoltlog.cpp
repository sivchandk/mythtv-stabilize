#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <QKeyEvent>
#include <QFile>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuitext.h>
#include <mythdirs.h>
// mytharchive

#include "smoltlog.h"

#define LOC      QString("MythSmolt: ")
#define LOC_WARN QString("MythSmolt, Warning: ")
#define LOC_ERR  QString("MythSmolt, Error: ")




void showLogViewer(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    LogViewer *viewer = new LogViewer(mainStack);

    if (viewer->Create())
        mainStack->AddScreen(viewer);
}

LogViewer::LogViewer(MythScreenStack *parent)
          : MythScreenType(parent, "logviewer")
{
    if ( (gCoreContext->GetSetting("HOSTPubUUID", "-1" )) == "-1" )
    {
        ShowOkPopup(QObject::tr("Smolt is a hardware profiler for MythTV.\n"
                                 "Submitting your profile is a great way to give back to the community. \n"
                                 "Submissions are anonymous, so submit your profile today!"));

    }
    setFilenames();
    QString fileprefix = GetConfDir();
    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);
    fileprefix += "/MythSmolt";
    dir = QDir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);


    QString commandline;
    QStringList list;
    commandline = "python2 " + GetShareDir() + "mythsmolt/scripts/sendProfile.py -p >" + m_fullLog;
    system( qPrintable(commandline));
}

LogViewer::~LogViewer(void)
{}

bool LogViewer::Create(void)
{
    bool foundtheme = false;
    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("smolt-ui.xml", "smoltviewer", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_logList, "loglist", &err);
    UIUtilE::Assign(this, m_logText, "logitem_text", &err);
    UIUtilE::Assign(this, m_submitButton, "submit_button", &err);
    UIUtilE::Assign(this, m_updateButton, "update_button", &err);
    UIUtilE::Assign(this, m_exitButton, "exit_button", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Cannot load screen 'logviewer'");
        return false;
    }

    connect(m_submitButton, SIGNAL(Clicked()), this, SLOT(submitProfile()));
    connect(m_updateButton, SIGNAL(Clicked()), this, SLOT(updateClicked()));
    connect(m_exitButton, SIGNAL(Clicked()), this, SLOT(Close()));
    connect(m_logList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(updateLogItem(MythUIButtonListItem*)));

    connect(m_logList, SIGNAL(itemClicked(MythUIButtonListItem*)),
                    this, SLOT(openLogURL(MythUIButtonListItem*)));
    BuildFocusList();
//     if (!BuildFocusList())
//         VERBOSE(VB_IMPORTANT, LOC + "Failed to build a focuslist. Something is wrong");

    SetFocusWidget(m_logList);
    return true;
}

void LogViewer::Init(void)
{
    updateClicked();
    if (m_logList->GetCount() > 0)
        m_logList->SetItemCurrent(1);
}

bool LogViewer::keyPressEvent(QKeyEvent *event)
{
    int ts;
    if (GetFocusWidget()->keyPressEvent(event))
         return true;
//     ts =  GetMythMainWindow()->GetMainStack()->TotalScreens();
//     VERBOSE(VB_IMPORTANT, "---------------------");
//     VERBOSE(VB_IMPORTANT,  ts);
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            showMenu();
        else
            handled = false;
    }


     if (!handled && MythScreenType::keyPressEvent(event))
         handled = true;

    return handled;
}


void LogViewer::updateLogItem(MythUIButtonListItem *item)
{
    if (item)
        m_logText->SetText(item->GetText());

}

void LogViewer::openLogURL(MythUIButtonListItem *item)
{
    QString url = "" ;
    QString item_text;

    if (item)
    {
        item_text = item->GetText();
        if (item_text.contains("http://"))
        {
            int start_pos;
            int end_pos;
            int str_len;
            start_pos = item_text.indexOf("http://");
            end_pos = item_text.indexOf(" ",start_pos);
            str_len = item_text.size();
            url = item_text.mid(start_pos,end_pos-start_pos);
            loadUrl(url);
        }
    }
}


void LogViewer::submitProfile(void)
{
    QString commandline;
    commandline = "python2 " + GetShareDir() + "mythsmolt/scripts/sendProfile.py --submitOnly -a  2>/tmp/smoltfile.err  >" + m_statusLog;
    int state = system( qPrintable(commandline));
    if ( state == 0 )
    {
        ShowOkPopup(QObject::tr("Profile was submitted successfully"));
        storeSmoltUid();
    }
    else
        ShowOkPopup(QObject::tr("Failed to submit profile, please try again."));
    m_currentLog = m_statusLog;
    m_logList->Reset();
    updateClicked();
}

void LogViewer::deleteProfile(void)
{
    QString commandline;
    commandline = "python2 " + GetShareDir() + "mythsmolt/scripts/deleteProfile.py  2>/tmp/smoltfile.err  >" + m_statusLog;
    int state = system( qPrintable(commandline));
    if ( state == 0 )
    {
        ShowOkPopup(QObject::tr("Profile was deleted successfully"));
    }
    else
        ShowOkPopup(QObject::tr("Failed to delete profile."));

    m_currentLog = m_statusLog;
    m_logList->Reset();
    updateClicked();

}


void LogViewer::updateClicked(void)
{
    QString commandline;
    QStringList list;
    //VERBOSE(VB_IMPORTANT,m_currentLog);
    loadFile(m_currentLog, list, m_logList->GetCount());

    if (list.size() > 0)
    {

        for (int x = 0; x < list.size(); x++)
            new MythUIButtonListItem(m_logList, list[x]);
        m_logList->SetItemCurrent(1);
    }

}

bool LogViewer::loadFile(QString filename, QStringList &list, int startline)
{
    list.clear();

    QFile file(filename);

    if (!file.exists())
        return false;

    if (file.open( QIODevice::ReadOnly ))
    {
        QString s;
        QTextStream stream(&file);

         // ignore the first startline lines
        while ( !stream.atEnd() && startline > 0)
        {
            stream.readLine();
            startline--;
        }

         // read rest of file
        while ( !stream.atEnd() )
        {
            s = stream.readLine();
            list.append(s);
        }
        file.close();
    }

    else
        return false;

    return true;
}

void LogViewer::setFilenames(void)
{
    QString conf_dir = GetConfDir();
    QString share_dir = GetShareDir();
    m_privacyLog = share_dir + "/mythsmolt/scripts/privacy.txt" ;
    m_fullLog =  conf_dir + "/MythSmolt/smoltinfo" ;
    m_statusLog = conf_dir + "/MythSmolt/smolt_admin" ;
    m_errataLog = conf_dir + "/MythSmolt/smolt_errata" ;
    m_currentLog = m_fullLog;
}

void LogViewer::showPrivacyLog(void)
{
    m_currentLog = m_privacyLog;
    m_logList->Reset();
    updateClicked();
}

void LogViewer::showFullLog(void)
{
    m_currentLog = m_fullLog;
    m_logList->Reset();
    updateClicked();
}

void LogViewer::showAdminLog(void)
{
    m_currentLog = m_statusLog;
    m_logList->Reset();
    updateClicked();
}

void LogViewer::showErrataLog(void)
{
    QString commandline;
    commandline = "python2 " + GetShareDir() + "mythsmolt/scripts/sendProfile.py -S   2>/tmp/smoltfile.err  >" + m_errataLog;
    system( qPrintable(commandline));
    m_currentLog = m_errataLog;
    m_logList->Reset();
    updateClicked();
}


void LogViewer::loadUrl(QString url)
{
    GetMythMainWindow()->HandleMedia("WebBrowser", url);
}

void LogViewer::showWebProfile(void)
{
    //http://www.smolts.org/client/show/pub_965ab877-d195-48e4-aa44-fc51fde8c72f
    QString url;
    QFile file(m_statusLog);
    if (file.open( QIODevice::ReadOnly ))
    {
        QString s;
        QTextStream stream(&file);
        while ( !stream.atEnd() )
        {
            s = stream.readLine();
            if (s.contains("http://"))
            {
                url=s.trimmed();
            }
        }
       file.close();
       loadUrl(url);
    }
}

void LogViewer::storeSmoltUid(void)
{
    QString hwuuid_file = GetConfDir() + "/MythSmolt/hw-uuid";
    QString hwuuid;
    QFile file(hwuuid_file);
    if (file.open( QIODevice::ReadOnly ))
    {
        QTextStream stream(&file);
        hwuuid = stream.readLine();
        file.close();

//        VERBOSE(VB_IMPORTANT,hwuuid);
        QString pubuuid_file = GetConfDir() + "/MythSmolt/uuiddb.cfg";
        QString pubuuid;
        QFile pubfile(pubuuid_file);
        if (pubfile.open( QIODevice::ReadOnly ))
        {
            QString s;
            QTextStream stream(&pubfile);
            while ( !stream.atEnd() )
            {
                s = stream.readLine();
                if (s.contains(hwuuid))
                {
                    pubuuid = s.section("=",1,1);
                    pubuuid = pubuuid.trimmed();
                }
            }
            pubfile.close();
            gCoreContext->SaveSetting("HOSTPubUUID",pubuuid);
        }
    }
}

void LogViewer::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");
    menuPopup->AddButton(tr("Show Privacy statement"), SLOT(showPrivacyLog()));
    menuPopup->AddButton(tr("View Profile"), SLOT(showFullLog()));
    menuPopup->AddButton(tr("View Profile on server"), SLOT(showWebProfile()));
    menuPopup->AddButton(tr("View Admin info"), SLOT(showAdminLog()));
    menuPopup->AddButton(tr("View Errata"), SLOT(showErrataLog()));
    menuPopup->AddButton(tr("Remove profile"), SLOT(deleteProfile()));
    menuPopup->AddButton(tr("Cancel"), NULL);
}
