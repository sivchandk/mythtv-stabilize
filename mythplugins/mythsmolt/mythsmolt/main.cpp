
// C++ headers
#include <unistd.h>

// QT headers
#include <QApplication>

// MythTV headers
#include <mythcontext.h>
#include <mythplugin.h>
#include <mythpluginapi.h>
#include <mythversion.h>
#include <mythmainwindow.h>

// MythSmolt headers
//#include "mythhello.h"
#include "smoltlog.h"

using namespace std;

void runSmolt(void);
int  RunSmolt(void);

void setupKeys(void)
{
    REG_JUMP("MythSmolt", QT_TRANSLATE_NOOP("MythSmolt",
        "Sample plugin"), "", runSmolt);
}


int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythhello",
        libversion,
                                    MYTH_BINARY_VERSION))
        return -1;
    setupKeys();
    return 0;
}

void runSmolt(void)
{
    RunSmolt();
}

int RunSmolt(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    LogViewer *viewer = new LogViewer(mainStack);

    if (viewer->Create())
    {
        mainStack->AddScreen(viewer);
        return 0;
    }
    else
    {
        delete viewer;
        return -1;
    }
}

int mythplugin_run(void)
{
    return RunSmolt();
}

int mythplugin_config(void)
{

}

void mythplugin_destroy(void)
{
    //QCoreApplication::exit();
}




