AsciiWall - Windows x64

INSTALL
If you downloaded Setup.exe, run it and follow the installer.
If you downloaded the ZIP, extract the complete archive to a folder and keep all files
together. Double-click asciiwall.exe to start the wallpaper.

OPEN SETTINGS
The visual settings window is built into asciiwall.exe. Use the "AsciiWall Settings"
Start menu shortcut after installation, the included shortcut in the ZIP, or the tray
icon. The shortcut starts AsciiWall if it is not already running.

SETTINGS AND STARTUP
Your active settings are stored in %APPDATA%\AsciiWall\settings.ini.
The first launch copies the included settings.ini there if no settings file exists yet.
Use the Screen settings to enable or disable the wallpaper and to control Windows startup.
The separate Screensaver tab can select AsciiWall for this Windows account or start it
immediately. This per-user option is not a named entry in Windows' screensaver picker;
Windows' idle timeout and sign-in options remain unchanged.
Disabling AsciiWall restores the previous Windows screensaver selection unless you chose
another saver in Windows while AsciiWall was enabled.

EXIT OR REMOVE
To close AsciiWall, choose Exit AsciiWall from its tray menu.
Before removing a ZIP program folder, turn off both Run at Windows startup and the
AsciiWall screensaver in Screen settings. The installer restores the previous screensaver
selection on uninstall if AsciiWall is still selected.
To also remove your personal settings, delete %APPDATA%\AsciiWall.

The ZIP release is not an installer. w64devkit is only needed to build AsciiWall from
source; it is not needed to run either release.

AsciiWall is distributed under the MIT License; see LICENSE.
Lua's separate license notice is in THIRD_PARTY_NOTICES.txt.
See ABOUT.md for the creator's note and contact details.
