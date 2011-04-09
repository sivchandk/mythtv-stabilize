#ifndef FILETRANSFER_H_
#define FILETRANSFER_H_

// POSIX headers
#include <pthread.h>

// ANSI C headers
#include <stdint.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QMutex>
#include <QWaitCondition>

// MythTV headers
#include "mythsocketmanager.h"
#include "filesysteminfo.h"

class ProgramInfo;
class RingBuffer;
class MythSocket;
class QString;
class FileTransfer;

typedef QMap<int, FileTransfer*> FileTransferMap;
typedef QMap<QString, MythSocket*> FileServerMap;

class FileTransferHandler : public SocketRequestHandler
{
  public:
    bool HandleAnnounce(MythSocket *socket, QStringList &commands,
                        QStringList &slist);
    bool HandleQuery(MythSocket *socket, QStringList &commands,
                     QStringList &slist);
    QString GetHandlerName(void)                    { return "FILETRANSFER"; }

    void connectionAnnounced(MythSocket *socket, QStringList &commands,
                             QStringList &slist);
    void connectionClosed(MythSocket *socket);

    QList<FileSystemInfo> QueryFileSystems(void);
    QList<FileSystemInfo> QueryAllFileSystems(void);

  signals:
    void unlinkFailed(QString path);
    void fileUnlinked(QString path);

  protected slots:
    void deleteThreadTerminated(void);
    void deferredDeleteSlot(void);

  private:
    bool HandleQueryFreeSpace(MythSocket *socket);
    bool HandleQueryFreeSpaceList(MythSocket *socket);
    bool HandleQueryFreeSpaceSummary(MythSocket *socket);

    bool HandleQueryFileExists(MythSocket *socket, QStringList &slist);
    bool HandleQueryFileHash(MythSocket *socket, QStringList &slist);

    bool HandleDeleteFile(MythSocket *socket, QStringList &slist);
    bool HandleDeleteFile(MythSocket *socket, QString filename,
                          QString storagegroup);
    bool HandleDeleteFile(QString filename, QString storagegroup)
                        { return HandleDeleteFile((MythSocket*)NULL,
                                                  filename, storagegroup); }
    
    bool HandleGetFileList(MythSocket *socket, QStringList &slist);
    bool HandleFileQuery(MythSocket *socket, QStringList &slist);
    bool HandleQueryFileTransfer(MythSocket *socket, QStringList &commands,
                                 QStringList &slist);
    bool HandleDownloadFile(MythSocket *socket, QStringList &slist);

    QString LocalFilePath(const QUrl &url, const QString &wantgroup);
    void RunDeleteThread(void);

    FileTransferMap         m_ftMap;
    QReadWriteLock          m_ftLock;

    FileServerMap           m_fsMap;
    QReadWriteLock          m_fsLock;

    QMutex                  m_downloadURLsLock;
    QMap<QString, QString>  m_downloadURLs;
};

class FileTransfer
{
    friend class QObject; // quiet OSX gcc warning

  public:
    FileTransfer(QString &filename, MythSocket *remote,
                 bool usereadahead, int timeout_ms);
    FileTransfer(QString &filename, MythSocket *remote, bool write);

    MythSocket *getSocket() { return sock; }

    bool isOpen(void);

    void Stop(void);

    void UpRef(void);
    bool DownRef(void);

    void Pause(void);
    void Unpause(void);
    int RequestBlock(int size);
    int WriteBlock(int size);

    long long Seek(long long curpos, long long pos, int whence);

    uint64_t GetFileSize(void);

    void SetTimeout(bool fast);

  private:
   ~FileTransfer();

    volatile bool  readthreadlive;
    bool           readsLocked;
    QWaitCondition readsUnlockedCond;

    ProgramInfo *pginfo;
    RingBuffer *rbuffer;
    MythSocket *sock;
    bool ateof;

    vector<char> requestBuffer;

    QMutex lock;
    QMutex refLock;
    int refCount;

    bool writemode;
};

#endif
