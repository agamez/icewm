#ifndef __GUIEVENT_H
#define __GUIEVENT_H

/* events don't get queued... is that a problem ? */

#define XA_GUI_EVENT_NAME "ICEWM_GUI_EVENT"

enum GUIEvent {
    geStartup,             //
    geShutdown,            //
    geRestart,             //
    geCloseAll,
    geLaunchApp,           //
    geWorkspaceChange,     //
    geWindowOpened,        //
    geWindowClosed,        //
    geDialogOpened,
    geDialogClosed,
    geWindowMax,           //
    geWindowRestore,       //
    geWindowMin,           //
    geWindowHide,          //
    geWindowRollup,        //
    geWindowMoved,
    geWindowSized,
    geWindowLower
};

#endif
