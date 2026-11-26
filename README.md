# AsciiWall

AsciiWall is a customizable animated ASCII winter campfire wallpaper for Windows.
See [ABOUT.md](ABOUT.md) for the creator's note and contact details.

## Download and run

Download either the ZIP or the setup program from [GitHub Releases](https://github.com/Vovanosa/ASCII-wall/releases).

- **Setup:** Run the installer and follow its instructions.
- **ZIP:** Extract the complete archive to a folder, keep its files together, and run `asciiwall.exe`.

Open the settings window with the **AsciiWall Settings** shortcut or the tray icon. Settings are organized into **General** (Screen and Screensaver), **Scene** (Weather, Fire, Trees and Lights, Ground and Effects, and Size), and **Interaction** categories. **Scene > Size** selects Small, Medium, or Large and immediately applies that size's scene profile; click **Save** to keep the changes. The size selection is saved and is not changed by the **Defaults** button. Use **Search** to find a setting by name or keyword; results are alphabetized and paged, and selecting one opens its page and focuses the control. The Screen page lets you enable or disable the wallpaper and choose whether it starts with Windows. The Screensaver page lets you optionally use AsciiWall as this account's screensaver or start it immediately; Windows' idle timeout and sign-in options stay unchanged. Because ZIP and setup installs are per-user, this screensaver is selected through AsciiWall Settings rather than appearing as a named entry in Windows' picker. Disabling it restores the prior Windows saver unless you chose another one in Windows in the meantime. Your wallpaper settings are stored in `%APPDATA%\AsciiWall\settings.ini`.

The Interaction page enables mouse effects by default: hold the left mouse button to gather nearby snow, then release to let it fall; clicking snowy trees shakes snow loose. Moving the pointer alone does not attract snow. Tune fire gust power, snow attraction radius and pull, held-flake limit, tree snow showers, and ground-trail lifetime there. Mouse input is observed only while the pointer is over the desktop; it does not capture clicks, so desktop icons and their context menus remain usable. Ground trails can fade after 1-60 seconds or remain until the scene reloads.

## Build from source

Building requires Windows x64 and [w64devkit](https://github.com/skeeto/w64devkit), which provides GCC and Make. From the project directory:

```text
make
make package VERSION=0.1.4
```

To build the Windows installer, also install [Inno Setup 6](https://jrsoftware.org/isinfo.php), then run:

```text
make installer VERSION=0.1.4
```

Release files are created under `build\`. End users do not need w64devkit or Inno Setup.
The `VERSION` value also determines the version shown in the Settings window.

For local development, release testing, and publishing new versions, see [RELEASING.md](RELEASING.md).

## License

AsciiWall is licensed under the MIT License; see [LICENSE](LICENSE). Lua's license is included in `dist\THIRD_PARTY_NOTICES.txt`.
