#ifndef HANDLER_H_
#define HANDLER_H_

// Qt
#include <QObject>
#include <QString>
#include <QMutex>
#include <QStringList>

// MythTV
#include "mythsocket.h"

class MainServer;

typedef enum {
    kPBSEvents_None       = 0,
    kPBSEvents_Normal     = 1,
    kPBSEvents_NonSystem  = 2,
    kPBSEvents_SystemOnly = 3
} ProtoSockEventsMode;

class ProtoHandler : public QObject
{
  public:
    ProtoHandler()                      { gCoreContext->addListener(this); }
   ~ProtoHandler() {};

    virtual QString getType(void)           const { return QString("NONE"); }

    virtual bool HandleAnnounce(MainServer *ms, MythSocket *socket,
                        QStringList &commands, QStringList &slist)
                                                  { return false; }
    virtual bool HandleCommand(MainServer *ms, MythSocket *socket,
                        QStringList &commands, QStringList &slist)
                                                  { return false; }

    virtual bool HandleDisconnect(MainServer *ms, MythSocket *socket)
                                                  { return false; }
};

class ProtoSocketHandler : public QObject
{
  public:
    ProtoSocketHandler(MainServer *parent, MythSocket *sock,
                                       QString hostname);
    ProtoSocketHandler(ProtoSocketHandler &other);
   ~ProtoSocketHandler();

    void UpRef(void);
    bool DownRef(void);

    void setIP(QString &lip)            { m_ip = lip; }
    QString getIP(void)           const { return m_ip; }

    void SetDisconnected(void)          { m_disconnected = true; }
    bool IsDisconnected(void)     const { return m_disconnected; }

    MythSocket *getSocket(void)   const { return m_sock; }
    QString getHostname(void)     const { return m_hostname; }
    virtual QString getType(void) const { return QString("NONE"); }
    MainServer *getParent(void)   const { return m_parent; }

    bool isLocal(void)            const { return m_local; }

    void SetEventsMode(ProtoSockEventsMode eventsMode)
                                        { m_eventsMode = eventsMode; }
    bool wantsEvents(void) const;
    bool wantsNonSystemEvents(void) const;
    bool wantsSystemEvents(void) const;
    bool wantsOnlySystemEvents(void) const;
    ProtoSockEventsMode eventsMode(void) const { return m_eventsMode; }

    void SetBlockShutdown(bool block)   { m_blockshutdown = block; }
    bool getBlockShutdown(void)   const { return m_blockshutdown; }

    bool SendStringList(QStringList &strlist)
                                { return m_sock->writeStringList(strlist); }

    virtual void Shutdown(void)         {};

  protected:
    bool SendReceiveStringlist(QStringList &strlist,
                               uint min_reply_length = 0);

    MainServer *m_parent;
    MythSocket *m_sock;

    bool        m_disconnected;
    QString     m_hostname;
    QString     m_ip;

    bool                m_local;
    bool                m_blockshutdown;
    ProtoSockEventsMode m_eventsMode;

    int         m_refCount;
    QMutex      m_refLock;
};

#endif
