#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <fcntl.h>

using namespace std;

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QUrl>
#include <QStringList>
#include <QString>
#include <QDir>
#include <QWriteLocker>
#include <QMutexLocker>
#include <QThread>

#include "filetransfer.h"
#include "ringbuffer.h"
#include "util.h"
#include "mythsocket.h"
#include "programinfo.h"
#include "mythverbose.h"
#include "storagegroup.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "backendutil.h"
#include "decodeencode.h"
#include "mythdownloadmanager.h"

DeleteThread *deletethread = NULL;

DeleteThread::DeleteThread(void) : m_increment(4194304), m_run(true), m_timeout(20000)
{
    m_slow = (bool) gCoreContext->GetNumSetting("TruncateDeletesSlowly", 0);

    int cards = 5;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT COUNT(cardid) FROM capturecard;");
    if (query.exec() && query.isActive() && query.size() && query.next())
        cards = query.value(0).toInt();

    size_t calc_tps = (size_t) (cards * 1.2 * (22200000LL / 8));
    m_increment     = max(m_increment, calc_tps/2);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
    m_timer.start(m_timeout);
}

void DeleteThread::run(void)
{
    while (gCoreContext && m_run)
    {
        ProcessNew();
        ProcessOld();
        usleep(500000);
    }

    if (!m_files.empty())
    {
        // this will only happen if the program is closing, so fast
        // deletion is not a problem
        QList<deletestruct *>::iterator i;
        for (i = m_files.begin(); i != m_files.end(); ++i)
        {
            close((*i)->fd);
            delete (*i);
        }
    }
}

bool DeleteThread::AddFile(QString path)
{
    QFileInfo finfo(path);
    if (!finfo.exists())
        return false;

    QMutexLocker lock(&m_newlock);
    m_newfiles << path;
    return true;
}

void DeleteThread::ProcessNew(void)
{
    // loop through new files, unlinking and adding for deletion
    // until none are left
    // TODO: add support for symlinks
    while (true)
    {
        QString path;
        {
            QMutexLocker lock(&m_newlock);
            if (!m_newfiles.isEmpty())
                path = m_newfiles.takeFirst();
        }

        if (path.isEmpty())
            break;

        m_timer.start(m_timeout);

        const char *cpath = path.toLocal8Bit().constData();

        QFileInfo finfo(path);
        VERBOSE(VB_FILE, QString("About to unlink/delete file: '%1'")
                                .arg(path));
        int fd = open(cpath, O_WRONLY);
        if (fd == -1)
        {
            VERBOSE(VB_IMPORTANT, QString("Error deleting '%1': "
                        "could not open ").arg(path) + ENO);
            emit unlinkFailed(path);
            continue;
        }

        if (unlink(cpath))
        {
            VERBOSE(VB_IMPORTANT, QString("Error deleting '%1': "
                        "could not unlink ").arg(path) + ENO);
            emit unlinkFailed(path);
            close(fd);
            continue;
        }

        emit fileUnlinked(path);

        deletestruct *ds = new deletestruct;
        ds->path = path;
        ds->fd = fd;
        ds->size = finfo.size();

        m_files << ds;
    }
}

void DeleteThread::ProcessOld(void)
{
    // im the only thread accessing this, so no need for a lock
    if (m_files.empty())
        return;

    m_timer.start(m_timeout);

    while (true)
    {
        deletestruct *ds = m_files.first();
        
        if (m_slow)
        {
            ds->size -= m_increment;
            int err = ftruncate(ds->fd, ds->size);

            if (err)
            {
                VERBOSE(VB_IMPORTANT, QString("Error truncating '%1'")
                            .arg(ds->path) + ENO);
                ds->size = 0;
            }
        }
        else
            ds->size = 0;

        if (ds->size == 0)
        {
            close(ds->fd);
            m_files.removeFirst();
            delete ds;
        }

        // fast delete can close out all, but slow delete needs
        // to return to sleep
        if (m_slow || m_files.empty())
            break;
    }
}


void FileTransferHandler::connectionClosed(MythSocket *socket)
{
    // iterate through transfer list and close if
    // socket matches connected transfer
    {
        QWriteLocker wlock(&m_ftLock);
        FileTransferMap::iterator i;
        for (i = m_ftMap.begin(); i != m_ftMap.end(); ++i)
        {
            if ((*i)->getSocket() == socket)
            {
                (*i)->DownRef();
                m_ftMap.remove(i.key());
                return;
            }
        }
    }

    // iterate through file server list and close 
    // if socket matched connected server
    {
        QWriteLocker wlock(&m_fsLock);
        FileServerMap::iterator i;
        for (i = m_fsMap.begin(); i != m_fsMap.end(); ++i)
        {
            if ((*i) == socket)
            {
                (*i)->DownRef();
                m_fsMap.remove(i.key());
                return;
            }
        }
    }
}

QString FileTransferHandler::LocalFilePath(const QUrl &url,
                                           const QString &wantgroup)
{
    QString lpath = url.path();

    if (lpath.section('/', -2, -2) == "channels")
    {
        // This must be an icon request. Check channel.icon to be safe.
        QString querytext;

        QString file = lpath.section('/', -1);
        lpath = "";

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT icon FROM channel WHERE icon LIKE :FILENAME ;");
        query.bindValue(":FILENAME", QString("%/") + file);

        if (query.exec() && query.isActive() && query.size())
        {
            query.next();
            lpath = query.value(0).toString();
        }
        else
        {
            MythDB::DBError("Icon path", query);
        }
    }
    else
    {
        lpath = lpath.section('/', -1);

        QString fpath = lpath;
        if (fpath.right(4) == ".png")
            fpath = fpath.left(fpath.length() - 4);

        ProgramInfo pginfo(fpath);
        if (pginfo.GetChanID())
        {
            QString pburl = GetPlaybackURL(&pginfo);
            if (pburl.left(1) == "/")
            {
                lpath = pburl.section('/', 0, -2) + "/" + lpath;
                VERBOSE(VB_FILE, QString("Local file path: %1").arg(lpath));
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("ERROR: LocalFilePath unable to find local "
                                "path for '%1', found '%2' instead.")
                                .arg(lpath).arg(pburl));
                lpath = "";
            }
        }
        else if (!lpath.isEmpty())
        {
            // For securities sake, make sure filename is really the pathless.
            QString opath = lpath;
            StorageGroup sgroup;

            if (!wantgroup.isEmpty())
            {
                sgroup.Init(wantgroup);
                lpath = url.toString();
            }
            else
            {
                lpath = QFileInfo(lpath).fileName();
            }

            QString tmpFile = sgroup.FindRecordingFile(lpath);
            if (!tmpFile.isEmpty())
            {
                lpath = tmpFile;
                VERBOSE(VB_FILE,
                        QString("LocalFilePath(%1 '%2'), found through "
                                "exhaustive search at '%3'")
                            .arg(url.toString()).arg(opath).arg(lpath));
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("ERROR: LocalFilePath unable to "
                                              "find local path for '%1'.")
                                .arg(opath));
                lpath = "";
            }

        }
        else
        {
            lpath = "";
        }
    }

    return lpath;
}

void FileTransferHandler::RunDeleteThread(void)
{
    if (deletethread == NULL)
    {
        deletethread = new DeleteThread();
        connect(deletethread, SIGNAL(unlinkFailed(QString)),
                this, SIGNAL(unlinkFailed(QString)));
        connect(deletethread, SIGNAL(fileUnlinked(QString)),
                this, SIGNAL(fileUnlinked(QString)));
        connect(deletethread, SIGNAL(finished()),
                this, SLOT(deleteThreadTerminated()));
        deletethread->start();
    }
}

void FileTransferHandler::deleteThreadTerminated(void)
{
    delete deletethread;
    deletethread = NULL;
}

bool FileTransferHandler::HandleAnnounce(MythSocket *socket,
                  QStringList &commands, QStringList &slist)
{
    if (commands[1] == "FileServer")
    {
        if (slist.size() >= 3)
        {
            QWriteLocker wlock(&m_fsLock);
            socket->UpRef();
            m_fsMap.insert(commands[2], socket);

            slist.clear();
            slist << "ok";
            socket->writeStringList(slist);
            return true;
        }
        return false;
    }

    if (commands[1] != "FileTransfer")
        return false;

    if (slist.size() < 3)
        return false;

    if ((commands.size() < 3) || (commands.size() > 6))
        return false;

    FileTransfer *ft    = NULL;
    QString hostname    = "";
    QString filename    = "";
    bool writemode      = false;
    bool usereadahead   = true;
    int timeout_ms      = 2000;

    switch (commands.size())
    {
      case 6:
        timeout_ms      = commands[5].toInt();
      case 5:
        usereadahead    = commands[4].toInt();
      case 4:
        writemode       = commands[3].toInt();
      default:
        hostname        = commands[2];
    }

    QStringList::const_iterator it = slist.begin();
    QUrl qurl           = *(++it);
    QString wantgroup   = *(++it);

    QStringList checkfiles;
    while (it != slist.end())
        checkfiles += *(++it);

    slist.clear();

    VERBOSE(VB_GENERAL, "FileTransferHandler::HandleAnnounce");
    VERBOSE(VB_IMPORTANT, QString("adding: %1 as remote file transfer")
                            .arg(hostname));

    if (writemode)
    {
        if (wantgroup.isEmpty())
            wantgroup = "Default";

        StorageGroup sgroup(wantgroup, gCoreContext->GetHostName(), false);
        QString dir = sgroup.FindNextDirMostFree();
        if (dir.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, "Unable to determine directory "
                    "to write to in FileTransfer write command");

            slist << "ERROR" << "filetransfer_directory_not_found";
            socket->writeStringList(slist);
            return true;
        }

        QString basename = qurl.path();
        if (basename.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, QString("ERROR: FileTransfer write "
                    "filename is empty in url '%1'.")
                    .arg(qurl.toString()));

            slist << "ERROR" << "filetransfer_filename_empty";
            socket->writeStringList(slist);
            return true;
        }

        if ((basename.contains("/../")) ||
            (basename.startsWith("../")))
        {
            VERBOSE(VB_IMPORTANT, QString("ERROR: FileTransfer write "
                    "filename '%1' does not pass sanity checks.")
                    .arg(basename));

            slist << "ERROR" << "filetransfer_filename_dangerous";
            socket->writeStringList(slist);
            return true;
        }

        filename = dir + "/" + basename;
    }
    else
        filename = LocalFilePath(qurl, wantgroup);

    QFileInfo finfo(filename);
    if (finfo.isDir())
    {
        VERBOSE(VB_IMPORTANT, QString("ERROR: FileTransfer filename "
                "'%1' is actually a directory, cannot transfer.")
                .arg(filename));

        slist << "ERROR" << "filetransfer_filename_is_a_directory";
        socket->writeStringList(slist);
        return true;
    }

    if (writemode)
    {
        QString dirPath = finfo.absolutePath();
        QDir qdir(dirPath);
        if (!qdir.exists())
        {
            if (!qdir.mkpath(dirPath))
            {
                VERBOSE(VB_IMPORTANT, QString("ERROR: FileTransfer "
                        "filename '%1' is in a subdirectory which does "
                        "not exist, but can not be created.")
                        .arg(filename));

                slist << "ERROR" << "filetransfer_unable_to_create_subdirectory";
                socket->writeStringList(slist);
                return true;
            }
        }

        ft = new FileTransfer(filename, socket, writemode);
    }
    else
        ft = new FileTransfer(filename, socket, usereadahead, timeout_ms);

    {
        QWriteLocker wlock(&m_ftLock);
        m_ftMap.insert(socket->socket(), ft);
    }

    slist << QString::number(socket->socket());
    encodeLongLong(slist, ft->GetFileSize());

    if (checkfiles.size())
    {
        QFileInfo fi(filename);
        QDir dir = fi.absoluteDir();
        for (it = checkfiles.begin(); it != checkfiles.end(); ++it)
        {
            if (dir.exists(*it) &&
                QFileInfo(dir, *it).size() >= kReadTestSize)
                    slist << *it;
        }
    }

    socket->writeStringList(slist);
    return true;
}

void FileTransferHandler::connectionAnnounced(MythSocket *socket,
                                QStringList &commands, QStringList &slist)
{
    if (commands[1] == "SlaveBackend")
    {
        // were not going to handle these, but we still want to track them
        // for commands that need access to these sockets
        if (slist.size() >= 3)
        {
            QWriteLocker wlock(&m_fsLock);
            socket->UpRef();
            m_fsMap.insert(commands[2], socket);
        }
    }

}

bool FileTransferHandler::HandleQuery(MythSocket *socket, QStringList &commands,
                                    QStringList &slist)
{
    bool handled = false;
    QString command = commands[0];

    if (command == "QUERY_FREE_SPACE")
        handled = HandleQueryFreeSpace(socket);
    else if (command == "QUERY_FREE_SPACE_LIST")
        handled = HandleQueryFreeSpaceList(socket);
    else if (command == "QUERY_FREE_SPACE_SUMMARY")
        handled = HandleQueryFreeSpaceSummary(socket);
    else if (command == "QUERY_FILE_EXISTS")
        handled = HandleQueryFileExists(socket, slist);
    else if (command == "QUERY_FILE_HASH")
        handled = HandleQueryFileHash(socket, slist);
    else if (command == "DELETE_FILE")
        handled = HandleDeleteFile(socket, slist);
    else if (command == "QUERY_SG_GETFILELIST")
        handled = HandleGetFileList(socket, slist);
    else if (command == "QUERY_SG_FILEQUERY")
        handled = HandleFileQuery(socket, slist);
    else if (command == "QUERY_FILETRANSFER")
        handled = HandleQueryFileTransfer(socket, commands, slist);
    else if (command == "DOWNLOAD_FILE" || command == "DOWNLOAD_FILE_NOW")
        handled = HandleDownloadFile(socket, slist);
    return handled;
}

bool FileTransferHandler::HandleQueryFreeSpace(MythSocket *socket)
{
    QStringList res;

    QList<FileSystemInfo> disks = QueryFileSystems();
    QList<FileSystemInfo>::const_iterator i;
    for (i = disks.begin(); i != disks.end(); ++i)
        i->ToStringList(res);

    socket->writeStringList(res);
    return true;
}

bool FileTransferHandler::HandleQueryFreeSpaceList(MythSocket *socket)
{
    QStringList res;
    QStringList hosts;

    QList<FileSystemInfo> disks = QueryAllFileSystems();
    QList<FileSystemInfo>::const_iterator i;
    for (i = disks.begin(); i != disks.end(); ++i)
        if (!hosts.contains(i->getHostname()))
            hosts << i->getHostname();

    // TODO: get max bitrate from encoderlink
    FileSystemInfo::Consolidate(disks, true, 14000);

    long long total = 0;
    long long used = 0;
    for (i = disks.begin(); i != disks.end(); ++i)
    {
        i->ToStringList(res);
        total += i->getTotalSpace();
        used  += i->getUsedSpace();
    }

    res << hosts.join(",")
        << "TotalDiskSpace"
        << "0"
        << "-2"
        << "-2"
        << "0"
        << QString::number(total)
        << QString::number(used);

    socket->writeStringList(res);
    return true;
}

bool FileTransferHandler::HandleQueryFreeSpaceSummary(MythSocket *socket)
{
    QStringList res;
    QList<FileSystemInfo> disks = QueryAllFileSystems();
    // TODO: get max bitrate from encoderlink
    FileSystemInfo::Consolidate(disks, true, 14000);

    QList<FileSystemInfo>::const_iterator i;
    long long total = 0;
    long long used = 0;
    for (i = disks.begin(); i != disks.end(); ++i)
    {
        total += i->getTotalSpace();
        used  += i->getUsedSpace();
    }

    res << QString::number(total) << QString::number(used);
    socket->writeStringList(res);
    return true;
}

QList<FileSystemInfo> FileTransferHandler::QueryFileSystems(void)
{
    QStringList groups(StorageGroup::kSpecialGroups);
    groups.removeAll("LiveTV");
    QString specialGroups = groups.join("', '");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT MIN(id),dirname "
                            "FROM storagegroup "
                           "WHERE hostname = :HOSTNAME "
                             "AND groupname NOT IN ( '%1' ) "
                           "GROUP BY dirname;").arg(specialGroups));
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    QList<FileSystemInfo> disks;
    if (query.exec() && query.isActive())
    {
        if (!query.size())
        {
            query.prepare("SELECT MIN(id),dirname "
                            "FROM storagegroup "
                           "WHERE groupname = :GROUP "
                           "GROUP BY dirname;");
            query.bindValue(":GROUP", "Default");
            if (!query.exec())
                MythDB::DBError("BackendQueryFileSystems", query);
        }

        QDir checkDir("");
        QString currentDir;
        FileSystemInfo disk;
        QMap <QString, bool>foundDirs;

        while (query.next())
        {
            disk.clear();
            disk.setHostname(gCoreContext->GetHostName());
            disk.setLocal();
            disk.setBlockSize(0);
            disk.setGroupID(query.value(0).toInt());

            /* The storagegroup.dirname column uses utf8_bin collation, so Qt
             * uses QString::fromAscii() for toString(). Explicitly convert the
             * value using QString::fromUtf8() to prevent corruption. */
            currentDir = QString::fromUtf8(query.value(1)
                                           .toByteArray().constData());
            disk.setPath(currentDir);

            if (currentDir.right(1) == "/")
                currentDir.remove(currentDir.length() - 1, 1);

            checkDir.setPath(currentDir);
            if (!foundDirs.contains(currentDir))
            {
                if (checkDir.exists())
                {
                    disk.PopulateDiskSpace();
                    disk.PopulateFSProp();
                    disks << disk;

                    foundDirs[currentDir] = true;
                }
                else
                    foundDirs[currentDir] = false;
            }
        }
    }

    return disks;
}

QList<FileSystemInfo> FileTransferHandler::QueryAllFileSystems(void)
{
    QList<FileSystemInfo> disks = QueryFileSystems();

    {
        QReadLocker rlock(&m_fsLock);

        FileServerMap::iterator i;
        for (i = m_fsMap.begin(); i != m_fsMap.end(); ++i)
            disks << FileSystemInfo::RemoteGetInfo(*i);
    }

    return disks;
}

bool FileTransferHandler::HandleQueryFileExists(MythSocket *socket,
                                                QStringList &slist)
{
    QString storageGroup = "Default";
    QStringList res;

    if (slist.size() == 3)
    {
        if (!slist[2].isEmpty())
            storageGroup = slist[2];
    }
    else if (slist.size() != 2)
        return false;

    QString filename = slist[1];
    if ((filename.isEmpty()) || 
        (filename.contains("/../")) || 
        (filename.startsWith("../")))
    {
        VERBOSE(VB_IMPORTANT, QString("ERROR checking for file, filename '%1' "
                    "fails sanity checks").arg(filename));
        res << "";
        socket->writeStringList(res);
        return true;
    }

    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName());
    QString fullname = sgroup.FindRecordingFile(filename);

    if (!fullname.isEmpty())
    {
        res << "1"
            << fullname;

        // TODO: convert me to QFile
        struct stat fileinfo;
        if (stat(fullname.toLocal8Bit().constData(), &fileinfo) >= 0)
        {
            res << QString::number(fileinfo.st_dev)
                << QString::number(fileinfo.st_ino)
                << QString::number(fileinfo.st_mode)
                << QString::number(fileinfo.st_nlink)
                << QString::number(fileinfo.st_uid)
                << QString::number(fileinfo.st_gid)
                << QString::number(fileinfo.st_rdev)
                << QString::number(fileinfo.st_size)
#ifdef USING_MINGW
                << "0"
                << "0"
#else
                << QString::number(fileinfo.st_blksize)
                << QString::number(fileinfo.st_blocks)
#endif
                << QString::number(fileinfo.st_atime)
                << QString::number(fileinfo.st_mtime)
                << QString::number(fileinfo.st_ctime);
        }
    }
    else
        res << "0";

    socket->writeStringList(res);
    return true;
}

bool FileTransferHandler::HandleQueryFileHash(MythSocket *socket,
                                              QStringList &slist)
{
    QString storageGroup = "Default";
    QStringList res;

    if (slist.size() == 3)
    {
        if (!slist[2].isEmpty())
            storageGroup = slist[2];
    }
    else if (slist.size() != 2)
        return false;

    QString filename = slist[1];
    if ((filename.isEmpty()) || 
        (filename.contains("/../")) || 
        (filename.startsWith("../")))
    {
        VERBOSE(VB_IMPORTANT, QString("ERROR checking for file, filename '%1' "
                    "fails sanity checks").arg(filename));
        res << "";
        socket->writeStringList(res);
        return true;
    }

    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName());
    QString fullname = sgroup.FindRecordingFile(filename);
    QString hash = FileHash(fullname);

    res << hash;
    socket->writeStringList(res);

    return true;
}

bool FileTransferHandler::HandleDeleteFile(MythSocket *socket,
                                           QStringList &slist)
{
    if (slist.size() != 3)
        return false;

    return HandleDeleteFile(socket, slist[1], slist[2]);
}

bool FileTransferHandler::DeleteFile(QString filename, QString storagegroup)
{
    return HandleDeleteFile(NULL, filename, storagegroup);
}

bool FileTransferHandler::HandleDeleteFile(MythSocket *socket,
                                QString filename, QString storagegroup)
{
    StorageGroup sgroup(storagegroup, "", false);
    QStringList res;

    if ((filename.isEmpty()) ||
        (filename.contains("/../")) ||
        (filename.startsWith("../")))
    {
        VERBOSE(VB_IMPORTANT, QString("ERROR deleting file, filename '%1' "
                "fails sanity checks").arg(filename));
        if (socket)
        {
            res << "0";
            socket->writeStringList(res);
            return true;
        }
        return false;
    }

    QString fullfile = sgroup.FindRecordingFile(filename);

    if (fullfile.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to find %1 in HandleDeleteFile()")
                .arg(filename));
        if (socket)
        {
            res << "0";
            socket->writeStringList(res);
            return true;
        }
        return false;
    }

    QFile checkFile(fullfile);
    if (checkFile.exists())
    {
        if (socket)
        {
            res << "1";
            socket->writeStringList(res);
        }
        RunDeleteThread();
        deletethread->AddFile(fullfile);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Error deleting file: '%1'")
                        .arg(fullfile));
        if (socket)
        {
            res << "0";
            socket->writeStringList(res);
        }
    }

    return true;
}

bool FileTransferHandler::HandleGetFileList(MythSocket *socket,
                                            QStringList &slist)
{
    QStringList res;

    bool fileNamesOnly = false;
    if (slist.size() == 5)
        fileNamesOnly = slist[4].toInt();
    else if (slist.size() != 4)
    {
        VERBOSE(VB_IMPORTANT, QString("HandleSGGetFileList: Invalid Request. "
                                      "%1").arg(slist.join("[]:[]")));
        res << "EMPTY LIST";
        socket->writeStringList(res);
        return true;
    }

    QString host = gCoreContext->GetHostName();
    QString wantHost = slist[1];
    QString groupname = slist[2];
    QString path = slist[3];

    VERBOSE(VB_FILE, QString("HandleSGGetFileList: group = %1  host = %2  "
                             "path = %3 wanthost = %4").arg(groupname)
                                .arg(host).arg(path).arg(wantHost));

    if ((host.toLower() == wantHost.toLower()) ||
        (gCoreContext->GetSetting("BackendServerIP") == wantHost))
    {
        StorageGroup sg(groupname, host);
        VERBOSE(VB_FILE, "HandleSGGetFileList: Getting local info");
        if (fileNamesOnly)
            res = sg.GetFileList(path);
        else
            res = sg.GetFileInfoList(path);

        if (res.count() == 0)
            res << "EMPTY LIST";
    }
    else
    {
        // handle request on remote server
        MythSocket *remsock = NULL;
        {
            QReadLocker rlock(&m_fsLock);
            if (m_fsMap.contains(wantHost))
            {
                remsock = m_fsMap[wantHost];
                remsock->UpRef();
            }
        }

        if (remsock)
        {
            VERBOSE(VB_FILE, "HandleSGGetFileList: Getting remote info");
            res << "QUERY_SG_GETFILELIST" << wantHost << groupname << path
                << QString::number(fileNamesOnly);
            remsock->SendReceiveStringList(res);
            remsock->DownRef();
        }
        else
        {
            VERBOSE(VB_FILE, QString("HandleSGGetFileList: Failed to grab "
                                     "slave socket : %1 :").arg(wantHost));
            res << "SLAVE UNREACHABLE: " << wantHost;
        }
    }

    socket->writeStringList(res);
    return true;
}

bool FileTransferHandler::HandleFileQuery(MythSocket *socket,
                                          QStringList &slist)
{
    QStringList res;

    if (slist.size() != 4)
    {
        VERBOSE(VB_IMPORTANT, QString("HandleSGFileQuery: Invalid Request. %1")
                .arg(slist.join("[]:[]")));
        res << "EMPTY LIST";
        socket->writeStringList(res);
        return true;
    }

    QString wantHost  = slist[1];
    QString groupname = slist[2];
    QString filename  = slist[3];

    VERBOSE(VB_FILE, QString("HandleSGFileQuery: myth://%1@%2/%3")
                             .arg(groupname).arg(wantHost).arg(filename));

    if ((wantHost.toLower() == gCoreContext->GetHostName().toLower()) ||
        (wantHost == gCoreContext->GetSetting("BackendServerIP")))
    {
        // handle request locally
        VERBOSE(VB_FILE, QString("HandleSGFileQuery: Getting local info"));
        StorageGroup sg(groupname, gCoreContext->GetHostName());
        res = sg.GetFileInfo(filename);

        if (res.count() == 0)
            res << "EMPTY LIST";
    }
    else
    {
        // handle request on remote server
        MythSocket *remsock = NULL;
        {
            QReadLocker rlock(&m_fsLock);
            if (m_fsMap.contains(wantHost))
            {
                remsock = m_fsMap[wantHost];
                remsock->UpRef();
            }
        }

        if (remsock)
        {
            res << "QUERY_SG_FILEQUERY" << wantHost << groupname << filename;
            remsock->SendReceiveStringList(res);
            remsock->DownRef();
        }
        else
        {
            res << "SLAVE UNREACHABLE: " << wantHost;
        }
    }

    socket->writeStringList(res);
    return true;
}

bool FileTransferHandler::HandleQueryFileTransfer(MythSocket *socket,
                        QStringList &commands, QStringList &slist)
{
    if (commands.size() != 2)
        return false;

    if (slist.size() < 2)
        return false;

    QStringList res;
    int recnum = commands[1].toInt();
    FileTransfer *ft;

    {
        QReadLocker rlock(&m_ftLock);
        if (!m_ftMap.contains(recnum))
        {
            if (slist[1] == "DONE")
                res << "ok";
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown file transfer "
                                              "socket: %1").arg(recnum));
                res << "ERROR"
                    << "unknown_file_transfer_socket";
            }

            socket->writeStringList(res);
            return true;
        }

        ft = m_ftMap[recnum];
        ft->UpRef();
    }

    if (slist[1] == "IS_OPEN")
    {
        res << QString::number(ft->isOpen());
    }
    else if (slist[1] == "DONE")
    {
        ft->Stop();
        res << "ok";
    }
    else if (slist[1] == "REQUEST_BLOCK")
    {
        if (slist.size() != 3)
        {
            VERBOSE(VB_IMPORTANT, "Invalid QUERY_FILETRANSFER "
                                  "REQUEST_BLOCK call");
            res << "ERROR" << "invalid_call";
        }
        else
        {
            int size = slist[2].toInt();
            res << QString::number(ft->RequestBlock(size));
        }
    }
    else if (slist[1] == "WRITE_BLOCK")
    {
        if (slist.size() != 3)
        {
            VERBOSE(VB_IMPORTANT, "Invalid QUERY_FILETRANSFER "
                                  "WRITE_BLOCK call");
            res << "ERROR" << "invalid_call";
        }
        else
        {
            int size = slist[2].toInt();
            res << QString::number(ft->WriteBlock(size));
        }
    }
    else if (slist[1] == "SEEK")
    {
        if (slist.size() != 5)
        {
            VERBOSE(VB_IMPORTANT, "Invalid QUERY_FILETRANSFER SEEK call");
            res << "ERROR" << "invalid_call";
        }
        else
        {
            long long pos = slist[2].toLongLong();
            int whence = slist[3].toInt();
            long long curpos = slist[4].toLongLong();

            res << QString::number(ft->Seek(curpos, pos, whence));
        }
    }
    else if (slist[1] == "SET_TIMEOUT")
    {
        if (slist.size() != 3)
        {
            VERBOSE(VB_IMPORTANT, "Invalid QUERY_FILETRANSFER "
                                  "SET_TIMEOUT call");
            res << "ERROR" << "invalid_call";
        }
        else
        {
            bool fast = slist[2].toInt();
            ft->SetTimeout(fast);
            res << "ok";
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Invalid QUERY_FILETRANSFER call");
        res << "ERROR" << "invalid_call";
    }

    ft->DownRef();
    socket->writeStringList(res);
    return true;
}

bool FileTransferHandler::HandleDownloadFile(MythSocket *socket,
                                             QStringList &slist)
{
    QStringList res;

    if (slist.size() != 4)
    {
        res << "ERROR" << QString("Bad %1 command").arg(slist[0]);
        socket->writeStringList(res);
        return true;
    }

    bool synchronous = (slist[0] == "DOWNLOAD_FILE_NOW");
    QString srcURL = slist[1];
    QString storageGroup = slist[2];
    QString filename = slist[3];
    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName(), false);
    QString outDir = sgroup.FindNextDirMostFree();
    QString outFile;
    QStringList retlist;

    if (filename.isEmpty())
    {
        QFileInfo finfo(srcURL);
        filename = finfo.fileName();
    }

    if (outDir.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Unable to determine directory "
                "to write to in %1 write command").arg(slist[0]));
        res << "ERROR" << "downloadfile_directory_not_found";
        socket->writeStringList(res);
        return true;
    }

    if ((filename.contains("/../")) ||
        (filename.startsWith("../")))
    {
        VERBOSE(VB_IMPORTANT, QString("ERROR: %1 write "
                "filename '%2' does not pass sanity checks.")
                .arg(slist[0]).arg(filename));
        res << "ERROR" << "downloadfile_filename_dangerous";
        socket->writeStringList(res);
        return true;
    }

    outFile = outDir + "/" + filename;

    if (synchronous)
    {
        if (GetMythDownloadManager()->download(srcURL, outFile))
        {
            res << "OK"
                << gCoreContext->GetMasterHostPrefix(storageGroup)
                       + filename;
        }
        else
            res << "ERROR";
    }
    else
    {
        QMutexLocker locker(&m_downloadURLsLock);
        m_downloadURLs[outFile] =
            gCoreContext->GetMasterHostPrefix(storageGroup) +
            StorageGroup::GetRelativePathname(outFile);

        GetMythDownloadManager()->queueDownload(srcURL, outFile, this);
        res << "OK"
            << gCoreContext->GetMasterHostPrefix(storageGroup) + filename;
    }

    socket->writeStringList(res);
    return true;
}


FileTransfer::FileTransfer(QString &filename, MythSocket *remote,
                           bool usereadahead, int timeout_ms) :
    readthreadlive(true), readsLocked(false),
    rbuffer(RingBuffer::Create(filename, false, usereadahead, timeout_ms)),
    sock(remote), ateof(false), lock(QMutex::NonRecursive),
    refLock(QMutex::NonRecursive), refCount(0), writemode(false)
{
    pginfo = new ProgramInfo(filename);
    pginfo->MarkAsInUse(true, kFileTransferInUseID);
}

FileTransfer::FileTransfer(QString &filename, MythSocket *remote, bool write) :
    readthreadlive(true), readsLocked(false),
    rbuffer(RingBuffer::Create(filename, write)),
    sock(remote), ateof(false), lock(QMutex::NonRecursive),
    refLock(QMutex::NonRecursive), refCount(0), writemode(write)
{
    pginfo = new ProgramInfo(filename);
    pginfo->MarkAsInUse(true, kFileTransferInUseID);

    if (write)
        remote->useReadyReadCallback(false);
}

FileTransfer::~FileTransfer()
{
    Stop();

    if (rbuffer)
    {
        delete rbuffer;
        rbuffer = NULL;
    }

    if (pginfo)
    {
        pginfo->MarkAsInUse(false, kFileTransferInUseID);
        delete pginfo;
    }
}

void FileTransfer::UpRef(void)
{
    QMutexLocker locker(&refLock);
    refCount++;
}

bool FileTransfer::DownRef(void)
{
    int count = 0;
    {
        QMutexLocker locker(&refLock);
        count = --refCount;
    }

    if (count < 0)
        delete this;
    
    return (count < 0);
}

bool FileTransfer::isOpen(void)
{
    if (rbuffer && rbuffer->IsOpen())
        return true;
    return false;
}

void FileTransfer::Stop(void)
{
    if (readthreadlive)
    {
        readthreadlive = false;
        rbuffer->StopReads();
        QMutexLocker locker(&lock);
        readsLocked = true;
    }

    if (writemode)
        rbuffer->WriterFlush();

    if (pginfo)
        pginfo->UpdateInUseMark();
}

void FileTransfer::Pause(void)
{
    rbuffer->StopReads();
    QMutexLocker locker(&lock);
    readsLocked = true;

    if (pginfo)
        pginfo->UpdateInUseMark();
}

void FileTransfer::Unpause(void)
{
    rbuffer->StartReads();
    {
        QMutexLocker locker(&lock);
        readsLocked = false;
    }
    readsUnlockedCond.wakeAll();

    if (pginfo)
        pginfo->UpdateInUseMark();
}

int FileTransfer::RequestBlock(int size)
{
    if (!readthreadlive || !rbuffer)
        return -1;

    int tot = 0;
    int ret = 0;

    QMutexLocker locker(&lock);
    while (readsLocked)
        readsUnlockedCond.wait(&lock, 100 /*ms*/);

    requestBuffer.resize(max((size_t)max(size,0) + 128, requestBuffer.size()));
    char *buf = &requestBuffer[0];
    while (tot < size && !rbuffer->GetStopReads() && readthreadlive)
    {
        int request = size - tot;

        ret = rbuffer->Read(buf, request);
        
        if (rbuffer->GetStopReads() || ret <= 0)
            break;

        if (!sock->writeData(buf, (uint)ret))
        {
            tot = -1;
            break;
        }

        tot += ret;
        if (ret < request)
            break; // we hit eof
    }

    if (pginfo)
        pginfo->UpdateInUseMark();

    return (ret < 0) ? -1 : tot;
}

int FileTransfer::WriteBlock(int size)
{
    if (!writemode || !rbuffer)
        return -1;

    int tot = 0;
    int ret = 0;

    QMutexLocker locker(&lock);

    requestBuffer.resize(max((size_t)max(size,0) + 128, requestBuffer.size()));
    char *buf = &requestBuffer[0];
    while (tot < size)
    {
        int request = size - tot;

        if (!sock->readData(buf, (uint)request))
            break;

        ret = rbuffer->Write(buf, request);
        
        if (ret <= 0)
            break;

        tot += request;
    }

    if (pginfo)
        pginfo->UpdateInUseMark();

    return (ret < 0) ? -1 : tot;
}

long long FileTransfer::Seek(long long curpos, long long pos, int whence)
{
    if (pginfo)
        pginfo->UpdateInUseMark();

    if (!rbuffer)
        return -1;
    if (!readthreadlive)
        return -1;

    ateof = false;

    Pause();

    if (whence == SEEK_CUR)
    {
        long long desired = curpos + pos;
        long long realpos = rbuffer->GetReadPosition();

        pos = desired - realpos;
    }

    long long ret = rbuffer->Seek(pos, whence);

    Unpause();

    if (pginfo)
        pginfo->UpdateInUseMark();

    return ret;
}

uint64_t FileTransfer::GetFileSize(void)
{
    if (pginfo)
        pginfo->UpdateInUseMark();

    return QFileInfo(rbuffer->GetFilename()).size();
}

void FileTransfer::SetTimeout(bool fast)
{
    if (pginfo)
        pginfo->UpdateInUseMark();

    rbuffer->SetOldFile(fast);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
