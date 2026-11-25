# AsciiWall

AsciiWall is a customizable animated ASCII winter campfire wallpaper for Windows.

## Download and run

Download either the ZIP or the setup program from [GitHub Releases](https://github.com/Vovanosa/ASCII-wall/releases).

- **Setup:** Run the installer and follow its instructions.
- **ZIP:** Extract the complete archive to a folder, keep its files together, and run `asciiwall.exe`.

Open the settings window with the **AsciiWall Settings** shortcut or the tray icon. The Screen settings let you enable or disable the wallpaper and choose whether it starts with Windows. Your settings are stored in `%APPDATA%\AsciiWall\settings.ini`.

## Build from source

Building requires Windows x64 and [w64devkit](https://github.com/skeeto/w64devkit), which provides GCC and Make. From the project directory:

```text
make
make package VERSION=0.1.0
```

To build the Windows installer, also install [Inno Setup 6](https://jrsoftware.org/isinfo.php), then run:

```text
make installer VERSION=0.1.0
```

Release files are created under `build\`. End users do not need w64devkit or Inno Setup.

## License

AsciiWall is licensed under the MIT License; see [LICENSE](LICENSE). Lua's license is included in `dist\THIRD_PARTY_NOTICES.txt`.
