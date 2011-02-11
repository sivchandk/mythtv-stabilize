// POSIX headers
#include <sys/time.h>     // for setpriority
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#endif

#include "mythconfig.h"
#if CONFIG_DARWIN
    #include <sys/aio.h>    // O_SYNC
#endif

// C headers
#include <cstdlib>
#include <cerrno>

#include <QCoreApplication>
#include <QFileInfo>
#include <QRegExp>
#include <QFile>
#include <QDir>
#include <QMap>

#include "mainserver.h"
#include "remoteutil.h"

#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythdb.h"
#include "exitcodes.h"
#include "compat.h"
#include "storagegroup.h"
#include "dbcheck.h"
#include "mythcommandlineparser.h"
#include "mythsystemevent.h"
#include "servercontext.h"
#include "main_helpers.h"
#include "util.h"

#define LOC      QString("MythFileServer: ")
#define LOC_WARN QString("MythFileServer, Warning: ")
#define LOC_ERR  QString("MythFileServer, Error: ")

bool setup_context(const MythCommandLineParser &cmdline)
{
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext.");
        return false;
    }
    gCoreContext->SetBackend(!cmdline.HasBackendCommand());

    QMap<QString,QString> settingsOverride = cmdline.GetSettingsOverride();
    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                    .arg(it.key()).arg(*it));
            gCoreContext->OverrideSettingForSession(it.key(), *it);
        }
    }

    return true;
}

void cleanup(void)
{
    delete gContext;
    gContext = NULL;

    if (pidfile.size())
    {
        unlink(pidfile.toAscii().constData());
        pidfile.clear();
    }

    signal(SIGHUP, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
}

int log_rotate(int report_error)
{
    /* http://www.gossamer-threads.com/lists/mythtv/dev/110113 */

    int new_logfd = open(logfile.toLocal8Bit().constData(),
                         O_WRONLY|O_CREAT|O_APPEND|O_SYNC, 0664);
    if (new_logfd < 0)
    {
        // If we can't open the new logfile, send data to /dev/null
        if (report_error)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Cannot open logfile '%1'").arg(logfile));
            return -1;
        }
        new_logfd = open("/dev/null", O_WRONLY);
        if (new_logfd < 0)
        {
            // There's not much we can do, so punt.
            return -1;
        }
    }
    while (dup2(new_logfd, 1) < 0 && errno == EINTR) ;
    while (dup2(new_logfd, 2) < 0 && errno == EINTR) ;
    while (close(new_logfd) < 0 && errno == EINTR) ;
    return 0;
}

void log_rotate_handler(int)
{
    log_rotate(0);
}

void showUsage(const MythCommandLineParser &cmdlineparser, const QString &version)
{
    QString    help  = cmdlineparser.GetHelpString(false);
    QByteArray ahelp = help.toLocal8Bit();

    cerr << qPrintable(version) << endl <<
    "Valid options are: " << endl <<
    "-h or --help                   List valid command line parameters"
         << endl << ahelp.constData() << endl;
}

void setupLogfile(void)
{
    if (!logfile.isEmpty())
    {
        if (log_rotate(1) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Cannot open logfile; using stdout/stderr instead");
        }
        else
            signal(SIGHUP, &log_rotate_handler);
    }
}

bool openPidfile(ofstream &pidfs, const QString &pidfile)
{
    if (!pidfile.isEmpty())
    {
        pidfs.open(pidfile.toAscii().constData());
        if (!pidfs)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Could not open pid file" + ENO);
            return false;
        }
    }
    return true;
}

bool setUser(const QString &username)
{
    if (username.isEmpty())
        return true;

#ifdef _WIN32
    VERBOSE(VB_IMPORTANT, "--user option is not supported on Windows");
    return false;
#else // ! _WIN32
    struct passwd *user_info = getpwnam(username.toLocal8Bit().constData());
    const uid_t user_id = geteuid();

    if (user_id && (!user_info || user_id != user_info->pw_uid))
    {
        VERBOSE(VB_IMPORTANT,
                "You must be running as root to use the --user switch.");
        return false;
    }
    else if (user_info && user_id == user_info->pw_uid)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Already running as '%1'").arg(username));
    }
    else if (!user_id && user_info)
    {
        if (setenv("HOME", user_info->pw_dir,1) == -1)
        {
            VERBOSE(VB_IMPORTANT, "Error setting home directory.");
            return false;
        }
        if (setgid(user_info->pw_gid) == -1)
        {
            VERBOSE(VB_IMPORTANT, "Error setting effective group.");
            return false;
        }
        if (initgroups(user_info->pw_name, user_info->pw_gid) == -1)
        {
            VERBOSE(VB_IMPORTANT, "Error setting groups.");
            return false;
        }
        if (setuid(user_info->pw_uid) == -1)
        {
            VERBOSE(VB_IMPORTANT, "Error setting effective user.");
            return false;
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Invalid user '%1' specified with --user")
                .arg(username));
        return false;
    }
    return true;
#endif // ! _WIN32
}

int handle_command(const MythCommandLineParser &cmdline)
{
    if (cmdline.SetVerbose())
    {
        if (gCoreContext->ConnectToMasterServer())
        {
            QString message = "SET_VERBOSE ";
            message += cmdline.GetNewVerbose();

            RemoteSendMessage(message);
            VERBOSE(VB_IMPORTANT, QString("Sent '%1' message").arg(message));
            return BACKEND_EXIT_OK;
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    "Unable to connect to backend, verbose level unchanged ");
            return BACKEND_EXIT_NO_CONNECT;
        }
    }

    // This should never actually be reached..
    return BACKEND_EXIT_OK;
}

int connect_to_master(void)
{
    MythSocket *tempMonitorConnection = new MythSocket();
    if (tempMonitorConnection->connect(
            gCoreContext->GetSetting("MasterServerIP", "127.0.0.1"),
            gCoreContext->GetNumSetting("MasterServerPort", 6543)))
    {
        if (!gCoreContext->CheckProtoVersion(tempMonitorConnection))
        {
            VERBOSE(VB_IMPORTANT, "Master backend is incompatible with "
                    "this backend.\nCannot become a slave.");
            return BACKEND_EXIT_NO_CONNECT;
        }

        QStringList tempMonitorDone("DONE");

        QStringList tempMonitorAnnounce("ANN Monitor tzcheck 0");
        tempMonitorConnection->writeStringList(tempMonitorAnnounce);
        tempMonitorConnection->readStringList(tempMonitorAnnounce);
        if (tempMonitorAnnounce.empty() ||
            tempMonitorAnnounce[0] == "ERROR")
        {
            tempMonitorConnection->DownRef();
            tempMonitorConnection = NULL;
            if (tempMonitorAnnounce.empty())
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to open event socket, timeout");
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to open event socket" +
                        ((tempMonitorAnnounce.size() >= 2) ?
                         QString(", error was %1").arg(tempMonitorAnnounce[1]) :
                         QString(", remote error")));
            }
        }

        QStringList tzCheck("QUERY_TIME_ZONE");
        if (tempMonitorConnection)
        {
            tempMonitorConnection->writeStringList(tzCheck);
            tempMonitorConnection->readStringList(tzCheck);
        }
        if (tzCheck.size() && !checkTimeZone(tzCheck))
        {
            // Check for different time zones, different offsets, different
            // times
            VERBOSE(VB_IMPORTANT, "The time and/or time zone settings on "
                    "this system do not match those in use on the master "
                    "backend. Please ensure all frontend and backend "
                    "systems are configured to use the same time zone and "
                    "have the current time properly set.");
            VERBOSE(VB_IMPORTANT,
                    "Unable to run with invalid time settings. Exiting.");
            tempMonitorConnection->writeStringList(tempMonitorDone);
            tempMonitorConnection->DownRef();
            return BACKEND_EXIT_INVALID_TIMEZONE;
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Backend is running in %1 time zone.")
                    .arg(getTimeZoneID()));
        }
        if (tempMonitorConnection)
            tempMonitorConnection->writeStringList(tempMonitorDone);
    }
    if (tempMonitorConnection)
        tempMonitorConnection->DownRef();

    return BACKEND_EXIT_OK;
}

int setup_basics(const MythCommandLineParser &cmdline)
{
    ofstream pidfs;
    if (!openPidfile(pidfs, cmdline.GetPIDFilename()))
        return BACKEND_EXIT_OPENING_PIDFILE_ERROR;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to ignore SIGPIPE");

    if (cmdline.IsDaemonizeEnabled() && (daemon(0, 1) < 0))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to daemonize" + ENO);
        return BACKEND_EXIT_DAEMONIZING_ERROR;
    }

    QString username = cmdline.GetUsername();
    if (!username.isEmpty() && !setUser(username))
        return BACKEND_EXIT_PERMISSIONS_ERROR;

    if (pidfs)
    {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    return BACKEND_EXIT_OK;
}

int run_fileserver(const MythCommandLineParser &cmdline)
{
    if (!setup_context(cmdline))
        return BACKEND_EXIT_NO_MYTHCONTEXT;

    if (!UpgradeTVDatabaseSchema(true, true))
    {
        VERBOSE(VB_IMPORTANT, "Couldn't upgrade database to new schema");
        return BACKEND_EXIT_DB_OUTOFDATE;
    }

    ///////////////////////////////////////////

    int ret = connect_to_master();
    if (BACKEND_EXIT_OK != ret)
        return ret;

    QString myip = gCoreContext->GetSetting("BackendServerIP");
    int     port = gCoreContext->GetNumSetting("BackendServerPort", 6543);
    if (myip.isEmpty())
    {
        cerr << "No setting found for this machine's BackendServerIP.\n"
             << "Please run setup on this machine and modify the first page\n"
             << "of the general settings.\n";
        return BACKEND_EXIT_NO_IP_ADDRESS;
    }

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    VERBOSE(VB_GENERAL, LOC + "Running fileserver.");
    gCoreContext->LogEntry("mythfileserver", LP_INFO,
                            "MythFileServer started", "");

    VERBOSE(VB_IMPORTANT, QString("Enabled verbose msgs: %1")
            .arg(verboseString));

    MainServer *mainServer = new MainServer(false, port);

    int exitCode = mainServer->GetExitCode();
    if (exitCode != BACKEND_EXIT_OK)
    {
        VERBOSE(VB_IMPORTANT, "FileServer exiting, MainServer initialization "
                "error.");
        delete mainServer;
        return exitCode;
    }

    StorageGroup::CheckAllStorageGroupDirs();

    ///////////////////////////////
    ///////////////////////////////
    exitCode = qApp->exec();
    ///////////////////////////////
    ///////////////////////////////

    gCoreContext->LogEntry("mythfileserver", LP_INFO,
                            "MythFileServer exiting", "");

    delete sysEventHandler;
    delete mainServer;

    return exitCode;
}
