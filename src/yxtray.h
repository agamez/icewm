#ifndef __YXTRAY_H
#define __YXTRAY_H

#include "yxembed.h"

#define SYSTEM_TRAY_REQUEST_DOCK 0
#define SYSTEM_TRAY_BEGIN_MESSAGE 1
#define SYSTEM_TRAY_CANCEL_MESSAGE 2

class YXTrayProxy;

class YXTrayNotifier {
public:
    virtual void trayChanged() = 0;
};

class YXTray: public YXEmbed {
public:
    YXTray(YXTrayNotifier *notifier, const char *atom, YWindow *aParent = 0);
    virtual ~YXTray();

    virtual void paint(Graphics &g, const YRect &r);
    virtual void configure(const YRect &r, const bool resized);
    virtual void handleConfigureRequest(const XConfigureRequestEvent &configureRequest);

    void relayout();

    void trayRequestDock(Window win);
    virtual void destroyedClient(Window win);
    void detachTray();
private:
    YXTrayProxy *fTrayProxy;
    YObjectArray<YXEmbedClient> fDocked;
    YXTrayNotifier *fNotifier;
};

#endif