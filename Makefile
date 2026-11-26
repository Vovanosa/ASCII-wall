.RECIPEPREFIX := >
CC      := gcc
VERSION ?= 0.1.4
CFLAGS  := -O2 -Wall -DNDEBUG -Ilua -DASCIIWALL_VERSION=\"$(VERSION)\"
LDFLAGS := -s -static -mwindows -municode
LIBS    := -luser32 -lgdi32 -ladvapi32 -lwtsapi32 -ldwmapi -lshell32 -lcomctl32
LUA     := $(filter-out lua/lua.c lua/luac.c lua/onelua.c,$(wildcard lua/*.c))

.PHONY: all run stop clean package installer FORCE

all: dist/asciiwall.exe dist/asciiwall.scr

FORCE:

dist/asciiwall.exe: FORCE src/main.c src/settings_ui.c src/settings_ui.h build/app.res.o $(LUA)
> @mkdir -p dist
> $(CC) $(CFLAGS) src/main.c src/settings_ui.c build/app.res.o $(LUA) -o $@ $(LDFLAGS) $(LIBS)

dist/asciiwall.scr: dist/asciiwall.exe
> cp $< $@

build/app.res.o: src/app.rc src/app.manifest src/asciiwall.ico
> @mkdir -p build
> windres src/app.rc -O coff -o $@

run:
> -@taskkill /f /im asciiwall.exe >/dev/null 2>&1; sleep 1
> $(MAKE) all
> ./dist/asciiwall.exe &

stop:
> -@./dist/asciiwall.exe --quit

package: dist/asciiwall.exe dist/asciiwall.scr
> powershell.exe -NoProfile -ExecutionPolicy Bypass -File "scripts\package-release.ps1" -Version "$(VERSION)"

installer: dist/asciiwall.exe dist/asciiwall.scr
> powershell.exe -NoProfile -ExecutionPolicy Bypass -File "scripts\build-installer.ps1" -Version "$(VERSION)"

clean:
> rm -rf build dist/asciiwall.exe dist/asciiwall.scr