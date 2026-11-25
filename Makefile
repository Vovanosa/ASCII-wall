.RECIPEPREFIX := >
CC      := gcc
CFLAGS  := -O2 -Wall -DNDEBUG -Ilua
LDFLAGS := -s -static -mwindows -municode
LIBS    := -luser32 -lgdi32 -ladvapi32 -lwtsapi32 -ldwmapi -lshell32 -lcomctl32
LUA     := $(filter-out lua/lua.c lua/luac.c lua/onelua.c,$(wildcard lua/*.c))
VERSION ?= 0.1.0

.PHONY: run stop clean package installer

dist/asciiwall.exe: src/main.c src/settings_ui.c src/settings_ui.h build/app.res.o $(LUA)
> @mkdir -p dist
> $(CC) $(CFLAGS) src/main.c src/settings_ui.c build/app.res.o $(LUA) -o $@ $(LDFLAGS) $(LIBS)

build/app.res.o: src/app.rc src/app.manifest src/asciiwall.ico
> @mkdir -p build
> windres src/app.rc -O coff -o $@

run:
> -@taskkill /f /im asciiwall.exe >/dev/null 2>&1; sleep 1
> $(MAKE) dist/asciiwall.exe
> ./dist/asciiwall.exe &

stop:
> -@./dist/asciiwall.exe --quit

package: dist/asciiwall.exe
> powershell.exe -NoProfile -ExecutionPolicy Bypass -File "scripts\package-release.ps1" -Version "$(VERSION)"

installer: dist/asciiwall.exe
> powershell.exe -NoProfile -ExecutionPolicy Bypass -File "scripts\build-installer.ps1" -Version "$(VERSION)"

clean:
> rm -rf build dist/asciiwall.exe