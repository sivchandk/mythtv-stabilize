#include <QTcpServer>

class MythSocket;

class MythServer : public QTcpServer
{
    Q_OBJECT

  signals:
    void newConnect(MythSocket*);

  protected:
    virtual void incomingConnection(int socket);
};

