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
Settings are organized into General (Screen and Screensaver), Scene (Weather, Fire,
Trees and Lights, Ground and Effects, and Size), and Interaction categories. Scene >
Size selects Small, Medium, or Large and immediately applies that size's scene profile.
Click Save to keep the changes. The size selection is saved and is not changed by Defaults.
Search finds settings by name or keyword; results are alphabetized and paged. Selecting a
result opens its page and focuses the control.
Use General > Screen to enable or disable the wallpaper and control Windows startup.
General > Screensaver can select AsciiWall for this Windows account or start it immediately.
This per-user option is not a named entry in Windows' screensaver picker; Windows' idle
timeout and sign-in options remain unchanged.
Disabling AsciiWall restores the previous Windows screensaver selection unless you chose
another saver in Windows while AsciiWall was enabled.

INTERACTION
Mouse interaction is enabled by default and does not capture clicks, so desktop icons
remain usable. Hold the left mouse button to gather nearby snow; releasing it lets the
snow fall. Moving the pointer alone does not attract snow. Clicking snowy trees shakes
snow loose without magnetizing snow during that press. The Interaction page controls fire
gust power, snow attraction, the tree snow shower, and ground-trail lifetime. Trails fade
after 10 seconds by default, can be set up to 60 seconds, or can be kept until the scene
reloads.
Fire speed is independent of FPS; setting Fire power or Fire speed to 0 hides the
campfire, including its logs, embers, and glow.

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
