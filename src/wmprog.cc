/*
 * IceWM
 *
 * Copyright (C) 1997-2002 Marko Macek
 */
#include "config.h"

#ifndef NO_CONFIGURE_MENUS

#define NEED_TIME_H

#include "ykey.h"
#include "objmenu.h"
#include "wmprog.h"
#include "wmoption.h"
#include "wmaction.h"
#include "wmconfig.h"
#include "ybutton.h"
#include "objbutton.h"
#include "objbar.h"
#include "prefs.h"
#include "wmapp.h"
#include "sysdep.h"
#include "base.h"
#include "wmmgr.h"
#include "ymenuitem.h"
#include "themes.h"
#include "browse.h"
#include "wmtaskbar.h"
#include "yicon.h"
#include "intl.h"

extern bool parseKey(const char *arg, KeySym *key, unsigned int *mod);

DObjectMenuItem::DObjectMenuItem(DObject *object):
    YMenuItem(object->getName(), -3, null, this, 0)
{
    fObject = object;
#ifndef LITE
    if (object->getIcon() != null)
        setIcon(object->getIcon());
#endif
}

DObjectMenuItem::~DObjectMenuItem() {
    delete fObject;
}

void DObjectMenuItem::actionPerformed(YActionListener * /*listener*/, YAction * /*action*/, unsigned int /*modifiers*/) {
#ifdef CONFIG_GUIEVENTS
    wmapp->signalGuiEvent(geLaunchApp);
#endif
    fObject->open();
}

DFile::DFile(const ustring &name, ref<YIcon> icon, upath path): DObject(name, icon) {
    fPath = path;
}

DFile::~DFile() {
}

void DFile::open() {
    const char *args[] = { openCommand, cstring(fPath.path()).c_str(), 0 };
    app->runProgram(openCommand, args);
}

ObjectMenu::ObjectMenu(YWindow *parent): YMenu(parent) {
    setActionListener(wmapp);
}

ObjectMenu::~ObjectMenu() {
}

void ObjectMenu::addObject(DObject *fObject) {
    add(new DObjectMenuItem(fObject));
}

void ObjectMenu::addSeparator() {
    YMenu::addSeparator();
}

#ifdef LITE
void ObjectMenu::addContainer(const ustring &name, ref<YIcon> /*icon*/, ObjectContainer *container) {
#else
void ObjectMenu::addContainer(const ustring &name, ref<YIcon> icon, ObjectContainer *container) {
#endif
    if (container) {
#ifndef LITE
        YMenuItem *item =
#endif
            addSubmenu(name, -3, (ObjectMenu *)container);

#ifndef LITE
        if (item && icon != null)
            item->setIcon(icon);
#endif
    }
}

DObject::DObject(const ustring &name, ref<YIcon> icon):
    fName(name), fIcon(icon)
{
}

DObject::~DObject() {
    fIcon = null;
}

void DObject::open() {
}

DProgram::DProgram(const ustring &name, ref<YIcon> icon, const bool restart,
                   const char *wmclass, upath exe, YStringArray &args):
    DObject(name, icon), fRestart(restart),
    fRes(newstr(wmclass)), fCmd(exe), fArgs(args)
{
    if (fArgs.isEmpty() || fArgs.getString(fArgs.getCount() - 1))
        fArgs.append(0);
}

DProgram::~DProgram() {
    delete[] fRes;
}

void DProgram::open() {
    if (fRestart)
        wmapp->restartClient(cstring(fCmd.path()).c_str(), fArgs.getCArray());
    else if (fRes)
        wmapp->runOnce(fRes, cstring(fCmd.path()).c_str(), fArgs.getCArray());
    else
        app->runProgram(cstring(fCmd.path()).c_str(), fArgs.getCArray());
}

DProgram *DProgram::newProgram(const char *name, ref<YIcon> icon,
                               const bool restart, const char *wmclass,
                               upath exe, YStringArray &args) {

    if (exe != null) {
        MSG(("LOOKING FOR: %s\n", cstring(exe.path()).c_str()));
        upath fullname = findPath(getenv("PATH"), X_OK, exe);
        if (fullname == null) {
            MSG(("Program %s (%s) not found.", name, cstring(exe.path()).c_str()));
            return 0;
        }

        DProgram *program =
            new DProgram(name, icon, restart, wmclass, fullname, args);

        return program;
    }
    return NULL;
}

char *getWord(char *word, int maxlen, char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        p++;
    while (*p && isalnum(*p) && maxlen > 1) {
        *word++ = *p++;
        maxlen--;
    }
    *word++ = 0;
    return p;
}

static char *getCommandArgs(char *p, char **command,
                            YStringArray &args)
{
    p = YConfig::getArgument(command, p, false);
    if (p == 0) {
        msg(_("Missing command argument"));
        return p;
    }
    args.append(*command);

    while (*p) {
        char *argx;

//push to the next word or line end to get the arg
        while (*p && (*p == ' ' || *p == '\t'))
            p++;
//stop on EOL
        if (*p == '\n')
            break;

        // parse the argument into argx and set the new position
        p = YConfig::getArgument(&argx, p, false);
        if (p == 0) {
            msg(_("Bad argument %d"), args.getCount() + 1);
            return p;
        }

        args.append(argx);
        MSG(("ARG: %s\n", argx));
        delete[] argx;
    }
    args.append(0);

    return p;
}

KProgram *keyProgs = 0;

KProgram::KProgram(const char *key, DProgram *prog) {
    parseKey(key, &fKey, &fMod);
    fProg = prog;
    fNext = keyProgs;
    keyProgs = this;
}

char *parseIncludeStatement(char *p, ObjectContainer *container) {
    char *filename;

    p = YConfig::getArgument(&filename, p, false);
    if (p == 0) {
        warn("invalid include filename");
        return p;
    }

    upath path(filename);
    delete[] filename;

    if (!path.isAbsolute())
        path = YApplication::findConfigFile(path);

    if (path != null)
        loadMenus(path, container);

    return p;
}

char *parseMenus(char *data, ObjectContainer *container) {
    char *p = data;
    char word[32];

    while (p && *p) {
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;
        if (*p == '#') {
            while (*p && *p != '\n')
                p++;
            continue;
        }
        p = getWord(word, sizeof(word), p);

        if (container) {
            if (!strcmp(word, "separator"))
                container->addSeparator();
            else if (!(strcmp(word, "prog") &&
                       strcmp(word, "restart") &&
                       strcmp(word, "runonce")))
            {
                char *name;

                p = YConfig::getArgument(&name, p, false);
                if (p == 0) return p;

                char *icons;

                p = YConfig::getArgument(&icons, p, false);
                if (p == 0) return p;

                char *wmclass = 0;

                if (word[1] == 'u') {
                    p = YConfig::getArgument(&wmclass, p, false);
                    if (p == 0) return p;
                }

                char *command;
                YStringArray args;

                p = getCommandArgs(p, &command, args);
                if (p == 0) {
                    msg(_("Error at prog %s"), name); return p;
                }

                ref<YIcon> icon;
#ifndef LITE
                if (icons[0] != '-') icon = YIcon::getIcon(icons);
#endif
                DProgram * prog =
                    DProgram::newProgram(name, icon,
                                         word[1] == 'e', word[1] == 'u' ? wmclass : 0,
                                         command, args);

                if (prog) container->addObject(prog);

                delete[] name;
                delete[] icons;
                delete[] wmclass;
                delete[] command;
            } else if (!strcmp(word, "menu")) {
                char *name;

                p = YConfig::getArgument(&name, p, false);
                if (p == 0) return p;

                char *icons;

                p = YConfig::getArgument(&icons, p, false);
                if (p == 0) return p;

                p = getWord(word, sizeof(word), p);
                if (*p != '{') return 0;
                p++;

                ref<YIcon> icon;
#ifndef LITE
                if (icons[0] != '-')
                    icon = YIcon::getIcon(icons);
#endif

                ObjectMenu *sub = new ObjectMenu();

                if (sub) {
                    p = parseMenus(p, sub);

                    if (sub->itemCount() == 0)
                        delete sub;
                    else
                        container->addContainer(name, icon, sub);

                } else {
                    msg(_("Unexepected keyword: %s"), word);
                    return p;
                }
                delete[] name;
                delete[] icons;
            } else if (!strcmp(word, "menufile")) {
                char *name;

                p = YConfig::getArgument(&name, p, false);
                if (p == 0) return p;

                char *icons;

                p = YConfig::getArgument(&icons, p, false);
                if (p == 0) return p;

                char *menufile;

                p = YConfig::getArgument(&menufile, p, false);
                if (p == 0) return p;

                ref<YIcon> icon;
#ifndef LITE
                if (icons[0] != '-')
                    icon = YIcon::getIcon(icons);
#endif
                ObjectMenu *filemenu = new MenuFileMenu(menufile, 0);

                if (menufile)
                    container->addContainer(name, icon, filemenu);
                delete[] name;
                delete[] icons;
                delete[] menufile;
            } else if (!strcmp(word, "menuprog")) {
                char *name;

                p = YConfig::getArgument(&name, p, false);
                if (p == 0) return p;

                char *icons;

                p = YConfig::getArgument(&icons, p, false);
                if (p == 0) return p;

                char *command;
                YStringArray args;

                p = getCommandArgs(p, &command, args);
                if (p == 0) {
                    msg(_("Error at prog %s"), name); return p;
                }

                ref<YIcon> icon;
#ifndef LITE
                if (icons[0] != '-')
                    icon = YIcon::getIcon(icons);
#endif
                MSG(("menuprog %s %s", name, command));

                upath fullPath = findPath(getenv("PATH"), X_OK, command);
                if (fullPath != null) {
                    ObjectMenu *progmenu = new MenuProgMenu(name, command, args, 0);
                    if (progmenu)
                        container->addContainer(name, icon, progmenu);
                }
                delete[] name;
                delete[] icons;
                delete[] command;
            } else if (!strcmp(word, "menuprogreload")) {
                char *name;

                p = YConfig::getArgument(&name, p, false);
                if (p == 0) return p;

                char *icons;

                p = YConfig::getArgument(&icons, p, false);
                if (p == 0) return p;

                time_t timeout;
                char *timeoutStr;

                p = YConfig::getArgument(&timeoutStr, p, false);
                if (p == 0) return p;
                timeout = atoi(timeoutStr);

                char *command;
                YStringArray args;

                p = getCommandArgs(p, &command, args);
                if (p == 0) {
                    msg(_("Error at prog %s"), name); return p;
                }

                ref<YIcon> icon;
#ifndef LITE
                if (icons[0] != '-')
                    icon = YIcon::getIcon(icons);
#endif
                MSG(("menuprogreload %s %s", name, command));

                upath fullPath = findPath(getenv("PATH"), X_OK, command);
                if (fullPath != null) {
                    ObjectMenu *progmenu = new MenuProgReloadMenu(name, timeout, command, args, 0);
                    if (progmenu)
                        container->addContainer(name, icon, progmenu);
                }
                delete[] name;
                delete[] icons;
                delete[] timeoutStr;
                delete[] command;
            } else if (!strcmp(word, "include"))
                p = parseIncludeStatement(p, container);
            else if (*p == '}')
                return ++p;
            else {
                return 0;
            }
        } else {
            if (!(strcmp(word, "key") &&
                  strcmp(word, "runonce")))
            {
                char *key;

                p = YConfig::getArgument(&key, p, false);
                if (p == 0) return p;

                char *wmclass = 0;

                if (*word == 'r') {
                    p = YConfig::getArgument(&wmclass, p, false);
                    if (p == 0) return p;
                }

                char *command;
                YStringArray args;

                p = getCommandArgs(p, &command, args);
                if (p == 0) {
                    msg(_("Error at key %s"), key);
                    return p;
                }

                DProgram *prog =
                    DProgram::newProgram(key, null, false,
                                         *word == 'r' ? wmclass : 0,
                                         command, args);

                if (prog) new KProgram(key, prog);
                delete[] key;
                delete[] wmclass;
                delete[] command;
            } else {
                return 0;
            }
        }
    }
    return p;
}

static void loadMenus(int fd, ObjectContainer *container) {
    if (fd == -1) return;

    struct stat sb;
    if (fstat(fd, &sb) == -1) { close(fd); return; }

    MSG(("sb.st_size: %d", sb.st_size));
    char *buf = 0;
    if (sb.st_size == 0) {
        int len = 0;
        int got = 0;
        buf = new char[len + 1];

        while (1) {
            if (len - got == 0) {
                len += 4096;
                char *buf2 = new char[len + 1];
                memcpy(buf2, buf, got);
                delete [] buf;
                buf = buf2;
            }
            int len2 = read(fd, buf + got, len - got);
            if (len2 == 0)
                break;
            got += len2;
        }
        buf[got] = '\0';
    } else {
        buf = new char[sb.st_size + 1];
        if (buf == 0) { close(fd); return; }

        read(fd, buf, sb.st_size);
        buf[sb.st_size] = '\0';
    }
    close(fd);

    parseMenus(buf, container);

    delete[] buf;
}

void loadMenus(upath menufile, ObjectContainer *container) {
    MSG(("menufile: %s", cstring(menufile.path()).c_str()));
    cstring cs(menufile.path());
    loadMenus(open(cs.c_str(), O_RDONLY | O_TEXT), container);
}

MenuFileMenu::MenuFileMenu(ustring name, YWindow *parent): ObjectMenu(parent), fName(name) {
    fName = name;
    fPath = 0;
    fModTime = 0;
    ///    updatePopup();
    ///    refresh();
}

MenuFileMenu::~MenuFileMenu() {
}

void MenuFileMenu::updatePopup() {
    if (!autoReloadMenus && fPath != null)
        return;

    upath np = YApplication::findConfigFile(upath(fName));
    bool rel = false;


    if (fPath == null) {
        fPath = np;
        rel = true;
    } else {
        if (np == null || np.equals(fPath)) {
            fPath = np;
            rel = true;
        } else
            np = null;
    }

    if (fPath == null) {
        refresh();
    } else {
        struct stat sb;
        cstring cs(fPath.path());
        if (stat(cs.c_str(), &sb) != 0) {
            fPath = 0;
            refresh();
        } else if (sb.st_mtime > fModTime || rel) {
            fModTime = sb.st_mtime;
            refresh();
        }
    }
}

void MenuFileMenu::refresh() {
    removeAll();
    if (fPath != null)
        loadMenus(fPath, this);
}

void loadMenusProg(const char *command, char *const argv[], ObjectContainer *container) {
    int fds[2];
    pid_t child_pid;
    int status;

    ///msg("loadMenusProg %s %s %s %s", command, argv[0], argv[1], argv[2]);

    if (!pipe(fds)) {
        switch ((child_pid = fork())) {
        case 0:
            close(0);
            close(1);

            close(fds[0]);
            dup2(fds[1], 1);

            execvp(command, argv);
            _exit(99);
            break;

        default:
            close(fds[1]);

            loadMenus(fds[0], container);
            waitpid(child_pid, &status, 0);
            close(fds[0]);
            break;

        case -1:
            warn("Forking failed (errno=%d)", errno);
            break;
        }
    }
}

MenuProgMenu::MenuProgMenu(ustring name, upath command, YStringArray &args, YWindow *parent): ObjectMenu(parent), fName(name), fCommand(command), fArgs(args) {
    fName = name;
    fCommand = command;
    fArgs.append(0);
    fModTime = 0;
    ///    updatePopup();
    ///    refresh();
}

MenuProgMenu::~MenuProgMenu() {
}

void MenuProgMenu::updatePopup() {
#if 0
    if (!autoReloadMenus && fPath != 0)
        return;
    struct stat sb;
    char *np = app->findConfigFile(fName);
    bool rel = false;


    if (fPath == 0) {
        fPath = np;
        rel = true;
    } else {
        if (!np || strcmp(np, fPath) != 0) {
            delete[] fPath;
            fPath = np;
            rel = true;
        } else
            delete[] np;
    }

    if (!autoReloadMenus)
        return;

    if (fPath == 0) {
        refresh();
    } else {
        if (stat(fPath, &sb) != 0) {
            delete[] fPath;
            fPath = 0;
            refresh();
        } else if (sb.st_mtime > fModTime || rel) {
            fModTime = sb.st_mtime;
            refresh();
        }
    }
#endif
    if (fModTime == 0)
        refresh();
    fModTime = time(NULL);
/// TODO #warning "figure out some way for this to work"
}

void MenuProgMenu::refresh() {
    removeAll();
    if (fCommand != null)
        loadMenusProg(cstring(fCommand.path()).c_str(), fArgs.getCArray(), this);
}

MenuProgReloadMenu::MenuProgReloadMenu(const char *name, time_t timeout, const char *command, YStringArray &args, YWindow *parent) : MenuProgMenu(name, command, args, parent) {
    fTimeout = timeout;
}

void MenuProgReloadMenu::updatePopup() {
    if (fModTime == 0 || time(NULL) >= fModTime + fTimeout) {
        refresh();
        fModTime = time(NULL);
    }
}

StartMenu::StartMenu(const char *name, YWindow *parent): MenuFileMenu(name, parent) {
    fHasGnomeAppsMenu =
        fHasGnomeUserMenu =
        fHasKDEMenu = false;

    ///    updatePopup();
    ///    refresh();
}

bool StartMenu::handleKey(const XKeyEvent &key) {
    // If meta key, close the popup
    if (key.type == KeyPress) {
        KeySym k = XKeycodeToKeysym(xapp->display(), (KeyCode)key.keycode, 0);
        int m = KEY_MODMASK(key.state);

        if (((k == xapp->Win_L) || (k == xapp->Win_R)) && m == 0) {
            cancelPopup();
            return true;
        }
    }
    return MenuFileMenu::handleKey(key);
}

void StartMenu::updatePopup() {
    MenuFileMenu::updatePopup();
}

void StartMenu::refresh() {
    MenuFileMenu::refresh();

    if (itemCount())
        addSeparator();

/// TODO #warning "make this into a menuprog (ala gnome.cc), and use mime"
    if (openCommand && openCommand[0]) {
        const char *path[2];
        YMenu *sub;
#ifndef LITE
        ref<YIcon> folder = YIcon::getIcon("folder");
#endif
        path[0] = "/";
        path[1] = getenv("HOME");

        for (unsigned int i = 0; i < sizeof(path)/sizeof(path[0]); i++) {
            const char *p = path[i];

            sub = new BrowseMenu(p);
            DFile *file = new DFile(p, null, p);
            YMenuItem *item = add(new DObjectMenuItem(file));
            if (item && sub) {
                item->setSubmenu(sub);
#ifndef LITE
                if (folder != null)
                    item->setIcon(folder);
#endif
            }
        }
        addSeparator();
    }

#ifdef CONFIG_WINLIST
    int const oldItemCount = itemCount();
#endif

    if (showPrograms) {
        ObjectMenu *programs = new MenuFileMenu("programs", 0);
        ///    if (programs->itemCount() > 0)
        addSubmenu(_("Programs"), 0, programs);
    }

    if (showRun) {
        if (runDlgCommand && runDlgCommand[0])
            addItem(_("_Run..."), -2, "", actionRun);
    }

#ifdef CONFIG_WINLIST
#ifdef CONFIG_WINMENU
    if (itemCount() != oldItemCount) addSeparator();
    if (showWindowList)
        addItem(_("_Windows"), -2, actionWindowList, windowListMenu);
#endif
#endif

    if (
#ifndef LITE
#ifdef CONFIG_TASKBAR
        (!showTaskBar && showAbout) ||
#endif
        showHelp ||
#endif
        showSettingsMenu
    )
#ifdef CONFIG_WINLIST
#ifdef CONFIG_WINMENU
        if (showWindowList)
#endif
#endif
        addSeparator();

#ifndef LITE
#ifdef CONFIG_TASKBAR
    if (!showTaskBar) {
        if (showAbout)
            addItem(_("_About"), -2, actionAbout, 0);
    }
#endif

    if (showHelp) {
        YStringArray args(3);
        args.append(ICEHELPEXE);
        args.append(ICEHELPIDX);
        args.append(0);

        DProgram *help =
            DProgram::newProgram(_("_Help"), null, false, "browser.IceHelp",
                                 ICEHELPEXE, args);

        if (help) addObject(help);
    }
#endif

    if (showSettingsMenu) {
        YMenu *settings = new YMenu();

        if (showFocusModeMenu) {
            YMenu *focus = new YMenu();
            YMenuItem *i = 0;

            i = focus->addItem(_("_Click to focus"), -2, "", actionFocusClickToFocus);
            if (focusMode == 1) {
                i->setEnabled(false);
                i->setChecked(true);
            }
            i = focus->addItem(_("_Sloppy mouse focus"), -2, "", actionFocusMouseSloppy);
            if (focusMode == 2) {
                i->setEnabled(false);
                i->setChecked(true);
            }
            i = focus->addItem(_("Custo_m"), -2, "", actionFocusCustom);
            if (focusMode == 0) {
                i->setEnabled(false);
                i->setChecked(true);
            }

            settings->addSubmenu(_("_Focus"), -2, focus);
        }


        if (showThemesMenu) {
            YMenu *themes = new ThemesMenu();
            if (themes)
                settings->addSubmenu(_("_Themes"), -2, themes);
        }
        addSubmenu(_("Se_ttings"), -2, settings);
    }

    if (logoutMenu) {
        addSeparator();
        if (showLogoutSubMenu)
            addItem(_("_Logout..."), -2, actionLogout, logoutMenu);
        else
            addItem(_("_Logout..."), -2, null, actionLogout);
    }
}
#endif
