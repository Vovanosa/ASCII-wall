#ifndef ASCIIWALL_SETTINGS_UI_H
#define ASCIIWALL_SETTINGS_UI_H

#include <windows.h>

typedef void (*SettingsReloadFn)(void);
typedef BOOL (*SettingsStartupFn)(BOOL enabled);

BOOL settings_ui_init(HINSTANCE instance);
void settings_ui_show(HWND owner, const WCHAR *ini, SettingsReloadFn reload,
                      SettingsStartupFn set_startup, BOOL startup_enabled);
void settings_ui_close(void);

#endif
