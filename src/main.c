/* asciiwall — мінімальний рушій живих ASCII-шпалер (MVP, один файл) */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WINVER       0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <wtsapi32.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "settings_ui.h"

#ifndef EVENT_OBJECT_CLOAKED
#define EVENT_OBJECT_CLOAKED   0x8017
#define EVENT_OBJECT_UNCLOAKED 0x8018
#endif

#define CLS_CTL  L"AsciiWall.Ctl"
#define CLS_SAVER_CTL L"AsciiWall.SaverCtl"
#define CLS_WALL L"AsciiWall.View"
enum { TM_RELOAD = 1, TM_OCCLUDE, TM_REATTACH };
#define WM_APP_TRAY (WM_APP + 1)
#define WM_APP_SETTINGS (WM_APP + 2)
#define ID_TRAY_SETTINGS 4001
#define ID_TRAY_EXIT 4002
#define ID_TRAY_ICON 1
#define IDI_APP_ICON 1
#define SAVER_STATE_KEY L"Software\\AsciiWall\\ScreenSaver"
#define DESKTOP_KEY L"Control Panel\\Desktop"
#define RGBX(v) RGB(((v) >> 16) & 255, ((v) >> 8) & 255, (v) & 255)   /* 0xRRGGBB -> COLORREF */

typedef struct { WCHAR ch; COLORREF fg, bg; } Cell;
typedef struct {
    double x, y, dx, dy, vx, vy;
    BOOL inside, left_down, pressed, moved, has_last_move;
    LARGE_INTEGER last_move;
} MouseInput;

static const GUID kDisplayState = {0x6fe69556,0x704a,0x47a0,{0x8f,0x24,0xc2,0x8d,0x93,0x6f,0xda,0x47}};
static const GUID kAcDc         = {0x5d3e9a59,0xe9d5,0x4b00,{0xa6,0xbd,0xff,0x34,0xff,0x51,0x65,0x48}};

static struct {
    HWND ctl, wall;                      /* приховане керуюче вікно; вікно шпалер */
    UINT msg_taskbar;
    NOTIFYICONDATAW tray;
    BOOL tray_added;
    HANDLE frame_timer, dir_watch, ini_watch;
    RECT mon, work;                     /* основний монітор, екранні координати */
    HFONT font; WCHAR font_name[64]; int font_size;
    int cw, chh, cols, rows, vis_rows;
    Cell *back, *front; WCHAR *run; INT *dx;
    BOOL full_redraw;
    COLORREF bg;
    lua_State *L; BOOL scene_ok; char err[300];
    WCHAR dir[MAX_PATH], config_dir[MAX_PATH], ini[MAX_PATH], scene[MAX_PATH];
    FILETIME ini_time, scene_time;
    int fps, fps_battery;
    BOOL enabled, locked, display_off, covered, on_battery, running;
    BOOL screen_saver, mouse_interactions;
    HHOOK mouse_hook;
    MouseInput mouse;
    POINT input_origin;
    LARGE_INTEGER qpf, t0, tprev;
} g;

static void reset_mouse_input(void) {
    memset(&g.mouse, 0, sizeof g.mouse);
}
static void update_mouse_hook(void);

typedef struct {
    DWORD type, size;
    BYTE *data;
    BOOL present;
} RegistryValue;

/* ───────────── утиліти ───────────── */

static unsigned utf8_next(const unsigned char **ps, const unsigned char *e) {
    const unsigned char *p = *ps; unsigned c = *p++;
    if (c >= 0xC0 && c < 0xE0 && p < e)          c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
    else if (c >= 0xE0 && c < 0xF0 && p + 1 < e) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
    else if (c >= 0xF0) { while (p < e && (*p & 0xC0) == 0x80) p++; c = '?'; }  /* поза BMP */
    *ps = p; return c;
}

static FILETIME file_time(const WCHAR *path) {
    WIN32_FILE_ATTRIBUTE_DATA a; FILETIME z = { 0, 0 };
    return GetFileAttributesExW(path, GetFileExInfoStandard, &a) ? a.ftLastWriteTime : z;
}

static BOOL join_path(WCHAR *out, size_t capacity, const WCHAR *directory, const WCHAR *name) {
    size_t dir_len = wcslen(directory), name_len = wcslen(name);
    if (dir_len + 1 + name_len + 1 > capacity) return FALSE;
    wcscpy(out, directory);
    wcscat(out, L"\\");
    wcscat(out, name);
    return TRUE;
}

static BOOL initialize_settings_path(void) {
    WCHAR appdata[MAX_PATH], source[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                                NULL, SHGFP_TYPE_CURRENT, appdata)))
        return FALSE;
    if (!join_path(g.config_dir, ARRAYSIZE(g.config_dir), appdata, L"AsciiWall"))
        return FALSE;
    if (!CreateDirectoryW(g.config_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return FALSE;
    DWORD attributes = GetFileAttributesW(g.config_dir);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return FALSE;
    if (!join_path(g.ini, ARRAYSIZE(g.ini), g.config_dir, L"settings.ini") ||
        !join_path(source, ARRAYSIZE(source), g.dir, L"settings.ini"))
        return FALSE;

    attributes = GetFileAttributesW(g.ini);
    if (attributes != INVALID_FILE_ATTRIBUTES) return TRUE;
    DWORD error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return FALSE;
    if (CopyFileW(source, g.ini, TRUE)) return TRUE;
    return GetLastError() == ERROR_FILE_EXISTS;
}

/* Читаємо файл через W-API: шляхи з кирилицею працюють */
static char *read_file(const WCHAR *path, size_t *len) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL), rd = 0;
    char *buf = malloc(sz + 1);
    if (buf && ReadFile(f, buf, sz, &rd, NULL)) { buf[rd] = 0; *len = rd; }
    else { free(buf); buf = NULL; }
    CloseHandle(f);
    return buf;
}

static void set_err(const char *m) { lstrcpynA(g.err, m ? m : "unknown error", sizeof g.err); }

/* Запуск/пауза. Пауза = таймер скасовано, потік спить у MsgWait без пробуджень. */
static void set_running(BOOL force) {
    BOOL visible = g.wall && (g.screen_saver || g.enabled);
    if (g.wall && IsWindowVisible(g.wall) != visible) {
        HWND parent = visible ? NULL : GetParent(g.wall);
        ShowWindow(g.wall, visible ? SW_SHOWNA : SW_HIDE);
        if (parent)
            RedrawWindow(parent, NULL, NULL,
                         RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    BOOL run = visible && !g.locked && !g.display_off && !g.covered;
    if (run == g.running && !force) return;
    g.running = run;
    if (!run) {
        CancelWaitableTimer(g.frame_timer);
        reset_mouse_input();
        return;
    }
    int fps = g.on_battery ? g.fps_battery : g.fps;
    if (fps < 1) fps = 1;
    if (fps > 60) fps = 60;
    LARGE_INTEGER due = { .QuadPart = -10000 };             /* перший кадр через 1 мс */
    SetWaitableTimerEx(g.frame_timer, &due, 1000 / fps, NULL, NULL, NULL, 10); /* 10 мс допуску: Windows може об'єднувати пробудження */
    QueryPerformanceCounter(&g.tprev);
}

/* ───────────── сітка і рендер ───────────── */

static void set_cell(int x, int y, WCHAR ch, COLORREF fg, COLORREF bg) {
    if (x < 0 || y < 0 || x >= g.cols || y >= g.rows) return;
    Cell *c = &g.back[y * g.cols + x];
    c->ch = ch; c->fg = fg; c->bg = bg;
}

static BOOL same(const Cell *a, const Cell *b) { return a->ch == b->ch && a->fg == b->fg && a->bg == b->bg; }

/* Малює лише змінені клітинки, групуючи їх у «пробіги» з однаковими кольорами */
static void present(HDC dc) {
    if (!g.back) return;
    for (int y = 0; y < g.rows; y++) {
        Cell *b = g.back + y * g.cols, *f = g.front + y * g.cols;
        int x = 0;
        while (x < g.cols) {
            if (!g.full_redraw && same(&b[x], &f[x])) { x++; continue; }
            int x0 = x, last = x;
            COLORREF fg = b[x].fg, bg = b[x].bg;
            for (; x < g.cols && b[x].fg == fg && b[x].bg == bg; x++) {
                if (g.full_redraw || !same(&b[x], &f[x])) last = x;
                else if (x - last > 3) break;              /* не тягнемо довгі незмінні хвости */
            }
            int n = last - x0 + 1;
            for (int i = 0; i < n; i++) { g.run[i] = b[x0 + i].ch; f[x0 + i] = b[x0 + i]; }
            RECT rc = { x0 * g.cw, y * g.chh, (x0 + n) * g.cw, (y + 1) * g.chh };
            SetTextColor(dc, fg); SetBkColor(dc, bg);
            ExtTextOutW(dc, rc.left, rc.top, ETO_OPAQUE | ETO_CLIPPED, &rc, g.run, n, g.dx);
            x = last + 1;
        }
    }
    g.full_redraw = FALSE;
}

static void draw_err(void) {          /* помилка сцени — червоний рядок над панеллю задач */
    if (!g.err[0] || !g.back) return;
    const unsigned char *s = (const unsigned char *)g.err, *e = s + strlen(g.err);
    for (int x = 0; x < g.cols; x++) {
        WCHAR ch = s < e ? (WCHAR)utf8_next(&s, e) : L' ';
        set_cell(x, g.vis_rows - 1, ch < 32 ? L' ' : ch, RGB(255, 110, 110), RGB(60, 0, 0));
    }
}

static void update_monitor(void) {
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(MonitorFromPoint((POINT){ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    g.mon = mi.rcMonitor; g.work = mi.rcWork;
}

/* Створює шрифт і (пере)розподіляє сітку під розмір монітора */
static void apply_font(void) {
    HFONT f = CreateFontW(-MulDiv(g.font_size, GetDpiForSystem(), 96), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, g.font_name);
    HDC mdc = CreateCompatibleDC(NULL);
    HGDIOBJ old = SelectObject(mdc, f);
    TEXTMETRICW tm; GetTextMetricsW(mdc, &tm);
    SelectObject(mdc, old); DeleteDC(mdc);
    if (g.wall) { HDC dc = GetDC(g.wall); SelectObject(dc, f); ReleaseDC(g.wall, dc); } /* CS_OWNDC */
    if (g.font) DeleteObject(g.font);
    g.font = f; g.cw = max(1, tm.tmAveCharWidth); g.chh = max(1, tm.tmHeight);

    int cols = (g.mon.right - g.mon.left + g.cw - 1) / g.cw;
    int rows = (g.mon.bottom - g.mon.top + g.chh - 1) / g.chh;
    if (cols != g.cols || rows != g.rows) {
        free(g.back); free(g.front); free(g.run); free(g.dx);
        g.cols = cols; g.rows = rows;
        g.back  = malloc(sizeof(Cell) * cols * rows);
        g.front = calloc((size_t)cols * rows, sizeof(Cell));
        g.run   = malloc(sizeof(WCHAR) * cols);
        g.dx    = malloc(sizeof(INT) * cols);
        for (int i = 0; i < cols * rows; i++) g.back[i] = (Cell){ L' ', g.bg, g.bg };
    }
    for (int i = 0; i < g.cols; i++) g.dx[i] = g.cw;
    g.vis_rows = min(g.rows, (g.work.bottom - g.mon.top) / g.chh);
    g.full_redraw = TRUE;
}

/* ───────────── Lua API ───────────── */

static int argi(lua_State *L, int i) { return (int)floor(luaL_checknumber(L, i)); }
static COLORREF opt_color(lua_State *L, int i, COLORREF def) {
    return lua_isnoneornil(L, i) ? def : RGBX((unsigned)luaL_checkinteger(L, i));
}
static WCHAR arg_char(lua_State *L, int i) {             /* "█" або 9608 */
    if (lua_type(L, i) == LUA_TNUMBER) return (WCHAR)lua_tointeger(L, i);
    size_t n; const unsigned char *s = (const unsigned char *)luaL_checklstring(L, i, &n);
    return n ? (WCHAR)utf8_next(&s, s + n) : L' ';
}

/* put(x, y, ch [, fg [, bg]]) */
static int l_put(lua_State *L) {
    set_cell(argi(L, 1), argi(L, 2), arg_char(L, 3), opt_color(L, 4, RGB(255, 255, 255)), opt_color(L, 5, g.bg));
    return 0;
}
/* text(x, y, str [, fg [, bg]]) — пробіли прозорі, зручно для спрайтів */
static int l_text(lua_State *L) {
    int x = argi(L, 1), y = argi(L, 2);
    size_t n; const unsigned char *s = (const unsigned char *)luaL_checklstring(L, 3, &n), *e = s + n;
    COLORREF fg = opt_color(L, 4, RGB(255, 255, 255)), bg = opt_color(L, 5, g.bg);
    for (; s < e; x++) { WCHAR ch = (WCHAR)utf8_next(&s, e); if (ch != L' ') set_cell(x, y, ch, fg, bg); }
    return 0;
}
/* fill(x, y, w, h, ch [, fg [, bg]]) */
static int l_fill(lua_State *L) {
    int x0 = argi(L, 1), y0 = argi(L, 2), w = argi(L, 3), h = argi(L, 4);
    WCHAR ch = arg_char(L, 5);
    COLORREF fg = opt_color(L, 6, RGB(255, 255, 255)), bg = opt_color(L, 7, g.bg);
    for (int y = max(y0, 0); y < min(y0 + h, g.rows); y++)
        for (int x = max(x0, 0); x < min(x0 + w, g.cols); x++) set_cell(x, y, ch, fg, bg);
    return 0;
}
/* clear([bg]) */
static int l_clear(lua_State *L) {
    COLORREF bg = opt_color(L, 1, g.bg);
    for (int i = 0, n = g.cols * g.rows; i < n; i++) g.back[i] = (Cell){ L' ', bg, bg };
    return 0;
}
/* scene_value(name [, default]) — read a numeric value from the [scene] section */
static int l_scene_value(lua_State *L) {
    size_t n;
    const char *name = luaL_checklstring(L, 1, &n);
    double def = lua_isnoneornil(L, 2) ? 0.0 : luaL_checknumber(L, 2);

    char a[64];
    if (n >= sizeof a) { lua_pushnumber(L, def); return 1; }
    memcpy(a, name, n);
    a[n] = 0;

    WCHAR key[64];
    WCHAR value[64];
    MultiByteToWideChar(CP_UTF8, 0, a, -1, key, 64);
    if (!GetPrivateProfileStringW(L"scene", key, L"", value, 64, g.ini))
        value[0] = 0;

    WCHAR *end = NULL;
    double v = value[0] ? wcstod(value, &end) : def;
    if (!end || *end) v = def;
    lua_pushnumber(L, v);
    return 1;
}

static BOOL call_init(lua_State *L) {
    lua_pushinteger(L, g.cols);     lua_setglobal(L, "COLS");
    lua_pushinteger(L, g.rows);     lua_setglobal(L, "ROWS");
    lua_pushinteger(L, g.vis_rows); lua_setglobal(L, "VIS_ROWS");
    if (lua_getglobal(L, "init") != LUA_TFUNCTION) { lua_pop(L, 1); return TRUE; }
    if (lua_pcall(L, 0, 0, 0) == LUA_OK) return TRUE;
    set_err(lua_tostring(L, -1)); lua_pop(L, 1);
    return FALSE;
}

/* Завантаження сцени в НОВИЙ lua_State; стара сцена замінюється лише при успіху */
static void load_scene(void) {
    g.scene_time = file_time(g.scene);
    size_t len; char *src = read_file(g.scene, &len);
    if (!src) { set_err("cannot read scene file (check asciiwall.ini)"); return; }

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    (void)luaL_dostring(L, "collectgarbage('generational')");   /* коротші паузи GC */
    static const luaL_Reg api[] = {
        {"put", l_put}, {"text", l_text}, {"fill", l_fill},
        {"clear", l_clear}, {"scene_value", l_scene_value}, {NULL, NULL}
    };
    lua_pushglobaltable(L); luaL_setfuncs(L, api, 0); lua_pop(L, 1);

    const char *code = src;
    if (len >= 3 && !memcmp(code, "\xEF\xBB\xBF", 3)) { code += 3; len -= 3; }  /* UTF-8 BOM */
    int rc = luaL_loadbufferx(L, code, len, "=scene", "t");
    if (rc == LUA_OK) rc = lua_pcall(L, 0, 0, 0);
    free(src);
    if (rc != LUA_OK) { set_err(lua_tostring(L, -1)); lua_close(L); return; }

    /* Налаштування сцени: font = { name, size }, background = 0xRRGGBB */
    wcscpy(g.font_name, L"Consolas"); g.font_size = 16; g.bg = RGB(0, 0, 0);
    if (lua_getglobal(L, "font") == LUA_TTABLE) {
        if (lua_getfield(L, -1, "name") == LUA_TSTRING)
            MultiByteToWideChar(CP_UTF8, 0, lua_tostring(L, -1), -1, g.font_name, 64);
        if (lua_getfield(L, -2, "size") == LUA_TNUMBER) g.font_size = (int)lua_tonumber(L, -1);
        lua_pop(L, 2);
    }
    lua_pop(L, 1);
    if (lua_getglobal(L, "background") == LUA_TNUMBER) g.bg = RGBX((unsigned)lua_tointeger(L, -1));
    lua_pop(L, 1);

    apply_font();
    if (!call_init(L)) { lua_close(L); return; }
    if (g.L) lua_close(g.L);
    g.L = L; g.scene_ok = TRUE; g.err[0] = 0; g.full_redraw = TRUE;
    if (!g.screen_saver && g.mouse_interactions && !g.mouse_hook)
        set_err("could not enable mouse interactions");
}

static void read_ini(void) {
    WCHAR rel[MAX_PATH];
    GetPrivateProfileStringW(L"engine", L"scene", L"scenes\\campfire.lua", rel, MAX_PATH, g.ini);
    if (rel[0] && rel[1] == L':') wcscpy(g.scene, rel);
    else { wcscpy(g.scene, g.dir); wcscat(g.scene, L"\\"); wcscat(g.scene, rel); }
    g.fps         = GetPrivateProfileIntW(L"engine", L"fps", 15, g.ini);
    g.fps_battery = GetPrivateProfileIntW(L"engine", L"fps_battery", 8, g.ini);
    g.enabled     = GetPrivateProfileIntW(L"engine", L"enabled", 1, g.ini) != 0;
    g.mouse_interactions =
        GetPrivateProfileIntW(L"engine", L"mouse_interactions", 1, g.ini) != 0;
    reset_mouse_input();
    g.ini_time = file_time(g.ini);
}

static void check_reload(void) {
    FILETIME ti = file_time(g.ini), ts;
    BOOL ini_changed = CompareFileTime(&ti, &g.ini_time) != 0;
    if (ini_changed) {
        read_ini();
        update_mouse_hook();
        set_running(TRUE);
    }
    ts = file_time(g.scene);
    if (ini_changed || CompareFileTime(&ts, &g.scene_time) != 0) load_scene();
}

/* ───────────── вбудовування в робочий стіл ───────────── */

static BOOL CALLBACK find_classic(HWND top, LPARAM out) {
    if (FindWindowExW(top, NULL, L"SHELLDLL_DefView", NULL)) {
        *(HWND *)out = FindWindowExW(NULL, top, L"WorkerW", NULL);  /* наступний WorkerW за іконками */
        return FALSE;
    }
    return TRUE;
}

static void place_wall(HWND after, UINT flags) {
    POINT o = { g.mon.left, g.mon.top };
    MapWindowPoints(NULL, GetParent(g.wall), &o, 1);    /* екранні → координати батька */
    SetWindowPos(g.wall, after, o.x, o.y, g.mon.right - g.mon.left, g.mon.bottom - g.mon.top,
                 flags | SWP_NOACTIVATE);
}

static BOOL attach(void) {
    HWND progman = FindWindowW(L"Progman", NULL);
    if (!progman) return FALSE;
    DWORD_PTR res;
    SendMessageTimeoutW(progman, 0x052C, 0xD, 1, SMTO_NORMAL, 1000, &res);  /* створити шар WorkerW */

    HWND defview = FindWindowExW(progman, NULL, L"SHELLDLL_DefView", NULL);
    HWND parent = NULL;
    if (defview) parent = progman;                       /* Windows 11 24H2+: «піднятий» робочий стіл */
    else EnumWindows(find_classic, (LPARAM)&parent);     /* класична схема */
    if (!parent) return FALSE;

    HWND w = CreateWindowExW(WS_EX_NOPARENTNOTIFY | (defview ? WS_EX_LAYERED : 0),
                             CLS_WALL, L"", WS_POPUP, 0, 0, 1, 1, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!w) return FALSE;
    if (defview) SetLayeredWindowAttributes(w, 0, 255, LWA_ALPHA);   /* ДО SetParent */
    SetWindowLongPtrW(w, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS);
    SetParent(w, parent);
    g.wall = w;
    place_wall(defview ? defview : HWND_TOP,
               (g.enabled ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_FRAMECHANGED);
    if (g.font) { HDC dc = GetDC(w); SelectObject(dc, g.font); ReleaseDC(w, dc); }
    g.full_redraw = TRUE;
    return TRUE;
}

static BOOL create_screensaver_window(void) {
    update_monitor();
    g.work = g.mon;
    GetCursorPos(&g.input_origin);
    g.wall = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, CLS_WALL, L"AsciiWall",
        WS_POPUP, g.mon.left, g.mon.top, g.mon.right - g.mon.left,
        g.mon.bottom - g.mon.top, NULL, NULL, GetModuleHandleW(NULL), NULL);
    return g.wall != NULL;
}

/* Чи перекриває якесь вікно всю робочу область основного монітора? */
static BOOL desktop_covered(void) {
    for (HWND h = GetTopWindow(NULL); h; h = GetWindow(h, GW_HWNDNEXT)) {
        if (!IsWindowVisible(h) || IsIconic(h)) continue;
        DWORD cloaked = 0;
        DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof cloaked);
        if (cloaked) continue;                                        /* інші віртуальні столи тощо */
        if (GetWindowLongPtrW(h, GWL_EXSTYLE) & WS_EX_TRANSPARENT) continue;  /* оверлеї */
        WCHAR cls[32]; GetClassNameW(h, cls, 32);
        if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW")) break;   /* дійшли до робочого столу */
        if (!wcscmp(cls, L"Shell_TrayWnd")) continue;
        RECT r;
        if (GetWindowRect(h, &r) && r.left <= g.work.left && r.top <= g.work.top &&
            r.right >= g.work.right && r.bottom >= g.work.bottom) return TRUE;
    }
    return FALSE;
}

static void CALLBACK on_win_event(HWINEVENTHOOK hk, DWORD ev, HWND h, LONG obj, LONG child, DWORD tid, DWORD ms) {
    if (obj != OBJID_WINDOW || !h || GetAncestor(h, GA_ROOT) != h) return;  /* лише вікна верхнього рівня */
    SetTimer(g.ctl, TM_OCCLUDE, 150, NULL);                                 /* дебаунс */
}

/* ───────────── кадр і розкладка ───────────── */

static void update_mouse_input(POINT screen, BOOL over_desktop) {
    int px = screen.x - g.mon.left, py = screen.y - g.mon.top;
    double x = (double)px / max(1, g.cw);
    double y = (double)py / max(1, g.chh);
    BOOL inside = over_desktop && px >= 0 && py >= 0
        && x < g.cols && y < g.vis_rows;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    if (inside && g.mouse.inside) {
        double dx = x - g.mouse.x, dy = y - g.mouse.y;
        if (dx != 0 || dy != 0) {
            double elapsed = g.mouse.has_last_move
                ? (double)(now.QuadPart - g.mouse.last_move.QuadPart) / g.qpf.QuadPart
                : 0;
            g.mouse.dx += dx;
            g.mouse.dy += dy;
            g.mouse.moved = TRUE;
            if (elapsed > 0.001) {
                double vx = dx / elapsed, vy = dy / elapsed;
                if (vx * vx + vy * vy > g.mouse.vx * g.mouse.vx + g.mouse.vy * g.mouse.vy) {
                    g.mouse.vx = vx;
                    g.mouse.vy = vy;
                }
            }
        }
    }
    if (inside) {
        g.mouse.x = x;
        g.mouse.y = y;
    }
    g.mouse.inside = inside;
    g.mouse.last_move = now;
    g.mouse.has_last_move = TRUE;
}

static BOOL cursor_over_desktop(POINT p) {
    HWND window = WindowFromPoint(p);
    HWND root = window ? GetAncestor(window, GA_ROOT) : NULL;
    WCHAR class_name[32];
    if (!root || !GetClassNameW(root, class_name, ARRAYSIZE(class_name))) return FALSE;
    return !wcscmp(class_name, L"Progman") || !wcscmp(class_name, L"WorkerW");
}

static LRESULT CALLBACK mouse_hook_proc(int code, WPARAM message, LPARAM data) {
    if (code >= 0 && g.mouse_interactions && !g.screen_saver &&
        (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)) {
        const MSLLHOOKSTRUCT *event = (const MSLLHOOKSTRUCT *)data;
        BOOL over_desktop = cursor_over_desktop(event->pt);
        update_mouse_input(event->pt, over_desktop);
        if (message == WM_LBUTTONDOWN) {
            g.mouse.left_down = over_desktop;
            if (over_desktop) g.mouse.pressed = TRUE;
        } else if (message == WM_LBUTTONUP) {
            g.mouse.left_down = FALSE;
        }
    }
    return CallNextHookEx(g.mouse_hook, code, message, data);
}

static void update_mouse_hook(void) {
    BOOL needed = g.mouse_interactions && !g.screen_saver;
    if (needed && !g.mouse_hook) {
        g.mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, mouse_hook_proc,
                                         GetModuleHandleW(NULL), 0);
        if (!g.mouse_hook) set_err("could not enable mouse interactions");
    } else if (!needed && g.mouse_hook) {
        if (UnhookWindowsHookEx(g.mouse_hook)) g.mouse_hook = NULL;
        else set_err("could not disable mouse interactions");
    }
    if (!needed) reset_mouse_input();
}

static void push_mouse_input(lua_State *L) {
    lua_createtable(L, 0, 10);
    lua_pushboolean(L, g.mouse_interactions && g.mouse.inside);
    lua_setfield(L, -2, "inside");
    lua_pushboolean(L, g.mouse_interactions && g.mouse.left_down);
    lua_setfield(L, -2, "down");
    lua_pushboolean(L, g.mouse_interactions && g.mouse.pressed);
    lua_setfield(L, -2, "pressed");
    lua_pushboolean(L, g.mouse_interactions && g.mouse.moved);
    lua_setfield(L, -2, "moved");
    lua_pushnumber(L, g.mouse.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, g.mouse.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, g.mouse.dx);
    lua_setfield(L, -2, "dx");
    lua_pushnumber(L, g.mouse.dy);
    lua_setfield(L, -2, "dy");
    lua_pushnumber(L, g.mouse.vx);
    lua_setfield(L, -2, "vx");
    lua_pushnumber(L, g.mouse.vy);
    lua_setfield(L, -2, "vy");
}

static void tick(void) {
    if (!g.wall || !g.back) return;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    double t  = (double)(now.QuadPart - g.t0.QuadPart) / g.qpf.QuadPart;
    double dt = (double)(now.QuadPart - g.tprev.QuadPart) / g.qpf.QuadPart;
    g.tprev = now;
    if (dt > 0.1) dt = 0.1;                                  /* без стрибка після паузи */
    if (g.L && g.scene_ok) {
        lua_getglobal(g.L, "update");
        lua_pushnumber(g.L, t); lua_pushnumber(g.L, dt);
        push_mouse_input(g.L);
        if (lua_pcall(g.L, 3, 0, 0) != LUA_OK) {
            set_err(lua_tostring(g.L, -1)); lua_pop(g.L, 1);
            g.scene_ok = FALSE;                              /* чекаємо на виправлення файлу */
        }
    }
    g.mouse.dx = g.mouse.dy = g.mouse.vx = g.mouse.vy = 0;
    g.mouse.pressed = g.mouse.moved = FALSE;
    draw_err();
    HDC dc = GetDC(g.wall); present(dc); ReleaseDC(g.wall, dc);
}

static void relayout(void) {          /* зміна роздільності / панелі задач */
    update_monitor();
    if (g.screen_saver) g.work = g.mon;
    apply_font();
    if (g.wall) {
        if (g.screen_saver)
            SetWindowPos(g.wall, HWND_TOPMOST, g.mon.left, g.mon.top,
                         g.mon.right - g.mon.left, g.mon.bottom - g.mon.top,
                         SWP_NOACTIVATE);
        else
            place_wall(NULL, SWP_NOZORDER);
    }
    if (g.L && g.scene_ok) g.scene_ok = call_init(g.L);
}

/* ───────────── віконні процедури ───────────── */

static LRESULT CALLBACK wall_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_NCHITTEST:
        return g.screen_saver ? HTCLIENT : HTTRANSPARENT;
    case WM_SETCURSOR:
        if (g.screen_saver) {
            SetCursor(NULL);
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
        if (g.screen_saver) {
            POINT p;
            if (GetCursorPos(&p) &&
                (abs(p.x - g.input_origin.x) >= 4 || abs(p.y - g.input_origin.y) >= 4))
                PostMessageW(g.ctl, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_MOUSEWHEEL:
        if (g.screen_saver) {
            PostMessageW(g.ctl, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (g.screen_saver) {
            PostMessageW(g.ctl, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g.screen_saver) {
            PostMessageW(g.ctl, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        g.full_redraw = TRUE; present(dc);
        EndPaint(h, &ps); return 0;
    }
    case WM_DESTROY:                                   /* напр., Explorer упав */
        if (h == g.wall) {
            g.wall = NULL;
            set_running(FALSE);
            if (g.screen_saver && g.ctl && IsWindow(g.ctl))
                PostMessageW(g.ctl, WM_CLOSE, 0, 0);
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static BOOL autostart_set(BOOL on);
static BOOL autostart_get(BOOL *on);
static BOOL screensaver_set(BOOL on);
static BOOL screensaver_get(BOOL *on);
static BOOL screensaver_start_now(void);

static void show_settings(void) {
    BOOL startup_enabled = FALSE, screensaver_enabled = FALSE;
    if (!autostart_get(&startup_enabled) || !screensaver_get(&screensaver_enabled)) {
        MessageBoxW(g.ctl, L"Could not read a Windows startup or screensaver setting.",
                    L"AsciiWall", MB_OK | MB_ICONERROR);
        return;
    }
    settings_ui_show(g.ctl, g.ini, check_reload, autostart_set, startup_enabled,
                     screensaver_set, screensaver_enabled, screensaver_start_now);
}

static BOOL add_tray_icon(void) {
    memset(&g.tray, 0, sizeof g.tray);
    g.tray.cbSize = sizeof g.tray;
    g.tray.hWnd = g.ctl;
    g.tray.uID = ID_TRAY_ICON;
    g.tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g.tray.uCallbackMessage = WM_APP_TRAY;
    g.tray.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!g.tray.hIcon) g.tray.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcscpy(g.tray.szTip, L"AsciiWall - animated ASCII wallpaper");
    g.tray_added = Shell_NotifyIconW(NIM_ADD, &g.tray);
    return g.tray_added;
}

static void show_tray_menu(void) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit AsciiWall");
    POINT p;
    GetCursorPos(&p);
    SetForegroundWindow(g.ctl);
    UINT cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                              p.x, p.y, 0, g.ctl, NULL);
    DestroyMenu(menu);
    PostMessageW(g.ctl, WM_NULL, 0, 0);
    if (cmd == ID_TRAY_SETTINGS) show_settings();
    if (cmd == ID_TRAY_EXIT) PostMessageW(g.ctl, WM_CLOSE, 0, 0);
}

static LRESULT CALLBACK ctl_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_APP_TRAY) {
        if (l == WM_LBUTTONDBLCLK) show_settings();
        else if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) show_tray_menu();
        return 0;
    }
    if (m == WM_APP_SETTINGS) { show_settings(); return 0; }
    switch (m) {
    case WM_TIMER:
        KillTimer(h, w);
        if (w == TM_OCCLUDE)  { g.covered = desktop_covered(); set_running(FALSE); }
        if (w == TM_RELOAD)   check_reload();
        if (w == TM_REATTACH) { if (attach()) set_running(FALSE); else SetTimer(h, TM_REATTACH, 2000, NULL); }
        return 0;
    case WM_WTSSESSION_CHANGE:
        if (w == WTS_SESSION_LOCK)   g.locked = TRUE;
        if (w == WTS_SESSION_UNLOCK) g.locked = FALSE;
        set_running(FALSE);
        return 0;
    case WM_POWERBROADCAST:
        if (w == PBT_POWERSETTINGCHANGE) {
            const POWERBROADCAST_SETTING *s = (const POWERBROADCAST_SETTING *)l;
            DWORD v = *(const DWORD *)s->Data;
            if (!memcmp(&s->PowerSetting, &kDisplayState, sizeof(GUID))) g.display_off = (v == 0);
            if (!memcmp(&s->PowerSetting, &kAcDc, sizeof(GUID)))         g.on_battery  = (v != 0);
            set_running(TRUE);
        }
        return TRUE;
    case WM_DISPLAYCHANGE: relayout(); return 0;
    case WM_SETTINGCHANGE: if (w == SPI_SETWORKAREA) relayout(); return 0;
    case WM_CLOSE:   DestroyWindow(h); return 0;
    case WM_DESTROY:
        if (g.tray_added) Shell_NotifyIconW(NIM_DELETE, &g.tray);
        g.tray_added = FALSE;
        settings_ui_close();
        PostQuitMessage(0);
        return 0;
    }
    if (!g.screen_saver && g.msg_taskbar && m == g.msg_taskbar) { /* Explorer перезапустився */
        if (g.wall && IsWindow(g.wall)) DestroyWindow(g.wall);
        g.wall = NULL; set_running(FALSE);
        if (!add_tray_icon())
            MessageBoxW(NULL, L"Could not restore the AsciiWall tray icon.", L"AsciiWall",
                        MB_OK | MB_ICONERROR);
        SetTimer(h, TM_REATTACH, 1000, NULL);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ───────────── автозапуск і точка входу ───────────── */

static BOOL autostart_set(BOOL on) {
    WCHAR exe[MAX_PATH], val[MAX_PATH + 4];
    HKEY k;
    LSTATUS r;
    if (on) {
        r = RegCreateKeyExW(HKEY_CURRENT_USER,
                            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                            0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL);
        if (r != ERROR_SUCCESS) return FALSE;
        DWORD length = GetModuleFileNameW(NULL, exe, ARRAYSIZE(exe));
        if (!length || length >= ARRAYSIZE(exe)) {
            RegCloseKey(k);
            return FALSE;
        }
        wcscpy(val, L"\""); wcscat(val, exe); wcscat(val, L"\"");
        r = RegSetValueExW(k, L"AsciiWall", 0, REG_SZ, (const BYTE *)val, (DWORD)((wcslen(val) + 1) * sizeof(WCHAR)));
    } else {
        r = RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_SET_VALUE, &k);
        if (r == ERROR_FILE_NOT_FOUND) return TRUE;
        if (r != ERROR_SUCCESS) return FALSE;
        r = RegDeleteValueW(k, L"AsciiWall");
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    }
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static BOOL autostart_get(BOOL *on) {
    HKEY k;
    DWORD type = 0, size = 0;
    *on = FALSE;
    LSTATUS r = RegOpenKeyExW(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                              0, KEY_QUERY_VALUE, &k);
    if (r == ERROR_FILE_NOT_FOUND) return TRUE;
    if (r != ERROR_SUCCESS) return FALSE;
    r = RegQueryValueExW(k, L"AsciiWall", NULL, &type, NULL, &size);
    RegCloseKey(k);
    if (r == ERROR_FILE_NOT_FOUND) return TRUE;
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ))
        return FALSE;
    *on = size > sizeof(WCHAR);
    return TRUE;
}

static BOOL registry_read_value(HKEY key, const WCHAR *name, RegistryValue *value) {
    memset(value, 0, sizeof(*value));
    DWORD size = 0, type = 0;
    LSTATUS result = RegQueryValueExW(key, name, NULL, &type, NULL, &size);
    if (result == ERROR_FILE_NOT_FOUND) return TRUE;
    if (result != ERROR_SUCCESS) return FALSE;

    BYTE *data = malloc(size ? size : 1);
    if (!data) return FALSE;
    result = RegQueryValueExW(key, name, NULL, &type, data, &size);
    if (result != ERROR_SUCCESS) {
        free(data);
        return FALSE;
    }
    value->type = type;
    value->size = size;
    value->data = data;
    value->present = TRUE;
    return TRUE;
}

static void registry_value_free(RegistryValue *value) {
    free(value->data);
    memset(value, 0, sizeof(*value));
}

static BOOL screensaver_path(WCHAR *path, size_t capacity) {
    WCHAR module[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, module, ARRAYSIZE(module));
    if (!length || length >= ARRAYSIZE(module)) return FALSE;
    WCHAR *slash = wcsrchr(module, L'\\');
    if (!slash) return FALSE;
    *slash = 0;
    return join_path(path, capacity, module, L"asciiwall.scr");
}

static BOOL open_desktop_key(DWORD access, HKEY *key) {
    return RegOpenKeyExW(HKEY_CURRENT_USER, DESKTOP_KEY, 0, access, key) == ERROR_SUCCESS;
}

static BOOL read_screensaver_selection(HKEY desktop, WCHAR *path, size_t capacity,
                                       RegistryValue *raw_value) {
    if (!registry_read_value(desktop, L"SCRNSAVE.EXE", raw_value)) return FALSE;
    path[0] = 0;
    if (!raw_value->present) return TRUE;
    if ((raw_value->type != REG_SZ && raw_value->type != REG_EXPAND_SZ) ||
        !raw_value->size || raw_value->size % sizeof(WCHAR) ||
        raw_value->size > capacity * sizeof(WCHAR) ||
        !wmemchr((const WCHAR *)raw_value->data, L'\0',
                 raw_value->size / sizeof(WCHAR)))
        return FALSE;
    memcpy(path, raw_value->data, raw_value->size);
    path[capacity - 1] = 0;
    return TRUE;
}

static BOOL path_equal(const WCHAR *left, const WCHAR *right) {
    return CompareStringOrdinal(left, -1, right, -1, TRUE) == CSTR_EQUAL;
}

static void notify_screensaver_change(void) {
    DWORD_PTR result;
    /* A hung notification recipient does not invalidate the registry selection. */
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)DESKTOP_KEY, SMTO_ABORTIFHUNG, 1000, &result);
}

static BOOL screensaver_backup_read(BOOL *valid, RegistryValue *original) {
    *valid = FALSE;
    memset(original, 0, sizeof(*original));
    HKEY key;
    LSTATUS result = RegOpenKeyExW(HKEY_CURRENT_USER, SAVER_STATE_KEY, 0,
                                   KEY_QUERY_VALUE, &key);
    if (result == ERROR_FILE_NOT_FOUND) return TRUE;
    if (result != ERROR_SUCCESS) return FALSE;

    RegistryValue marker = { 0 };
    BOOL ok = registry_read_value(key, L"BackupValid", &marker);
    if (!ok) { RegCloseKey(key); return FALSE; }
    if (!marker.present) {
        registry_value_free(&marker);
        RegCloseKey(key);
        return TRUE;
    }
    if (marker.type != REG_DWORD || marker.size != sizeof(DWORD)) {
        registry_value_free(&marker);
        RegCloseKey(key);
        return FALSE;
    }
    DWORD is_valid = *(DWORD *)marker.data;
    registry_value_free(&marker);
    if (is_valid == 0) {
        RegCloseKey(key);
        return TRUE;
    }
    if (is_valid != 1) {
        RegCloseKey(key);
        return FALSE;
    }

    RegistryValue present = { 0 }, type = { 0 };
    ok = registry_read_value(key, L"OriginalPresent", &present);
    if (!ok || !present.present || present.type != REG_DWORD ||
        present.size != sizeof(DWORD)) {
        registry_value_free(&present);
        RegCloseKey(key);
        return FALSE;
    }
    DWORD has_original = *(DWORD *)present.data;
    registry_value_free(&present);
    if (has_original > 1) {
        RegCloseKey(key);
        return FALSE;
    }
    if (has_original) {
        ok = registry_read_value(key, L"OriginalType", &type);
        if (!ok || !type.present || type.type != REG_DWORD ||
            type.size != sizeof(DWORD)) {
            registry_value_free(&type);
            RegCloseKey(key);
            return FALSE;
        }
        DWORD original_type = *(DWORD *)type.data;
        registry_value_free(&type);
        if (original_type != REG_SZ && original_type != REG_EXPAND_SZ) {
            RegCloseKey(key);
            return FALSE;
        }
        RegistryValue data = { 0 };
        ok = registry_read_value(key, L"OriginalData", &data);
        if (!ok || !data.present || data.type != REG_BINARY ||
            !data.size || data.size % sizeof(WCHAR) ||
            !wmemchr((const WCHAR *)data.data, L'\0', data.size / sizeof(WCHAR))) {
            registry_value_free(&data);
            registry_value_free(original);
            RegCloseKey(key);
            return FALSE;
        }
        data.type = original_type;
        *original = data;
    }
    *valid = TRUE;
    RegCloseKey(key);
    return TRUE;
}

static BOOL screensaver_backup_save(const RegistryValue *original) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SAVER_STATE_KEY, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
        return FALSE;

    DWORD invalid = 0, present = original->present ? 1 : 0;
    LSTATUS result = RegSetValueExW(key, L"BackupValid", 0, REG_DWORD,
                                    (const BYTE *)&invalid, sizeof(invalid));
    if (result == ERROR_SUCCESS)
        result = RegSetValueExW(key, L"OriginalPresent", 0, REG_DWORD,
                                (const BYTE *)&present, sizeof(present));
    if (result == ERROR_SUCCESS && original->present)
        result = RegSetValueExW(key, L"OriginalType", 0, REG_DWORD,
                                (const BYTE *)&original->type, sizeof(original->type));
    if (result == ERROR_SUCCESS && original->present)
        result = RegSetValueExW(key, L"OriginalData", 0, REG_BINARY,
                                original->data, original->size);
    DWORD valid = 1;
    if (result == ERROR_SUCCESS)
        result = RegSetValueExW(key, L"BackupValid", 0, REG_DWORD,
                                (const BYTE *)&valid, sizeof(valid));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

static BOOL screensaver_backup_clear(void) {
    HKEY key;
    LSTATUS result = RegOpenKeyExW(HKEY_CURRENT_USER, SAVER_STATE_KEY, 0,
                                   KEY_SET_VALUE, &key);
    if (result == ERROR_FILE_NOT_FOUND) return TRUE;
    if (result != ERROR_SUCCESS) return FALSE;
    const WCHAR *names[] = { L"BackupValid", L"OriginalPresent", L"OriginalType", L"OriginalData" };
    BOOL ok = TRUE;
    for (int i = 0; i < ARRAYSIZE(names); i++) {
        result = RegDeleteValueW(key, names[i]);
        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) ok = FALSE;
    }
    RegCloseKey(key);
    return ok;
}

static BOOL screensaver_get(BOOL *on) {
    WCHAR current[MAX_PATH], target[MAX_PATH];
    RegistryValue raw = { 0 };
    HKEY desktop;
    *on = FALSE;
    if (!open_desktop_key(KEY_QUERY_VALUE, &desktop)) return FALSE;
    BOOL ok = read_screensaver_selection(desktop, current, ARRAYSIZE(current), &raw);
    RegCloseKey(desktop);
    registry_value_free(&raw);
    if (!ok || !screensaver_path(target, ARRAYSIZE(target))) return FALSE;
    *on = current[0] && path_equal(current, target);
    return TRUE;
}

static BOOL screensaver_set(BOOL on) {
    WCHAR current[MAX_PATH], target[MAX_PATH];
    RegistryValue raw = { 0 }, original = { 0 };
    HKEY desktop;
    if (!screensaver_path(target, ARRAYSIZE(target)) ||
        !open_desktop_key(KEY_QUERY_VALUE | KEY_SET_VALUE, &desktop))
        return FALSE;
    if (!read_screensaver_selection(desktop, current, ARRAYSIZE(current), &raw)) {
        registry_value_free(&raw);
        RegCloseKey(desktop);
        return FALSE;
    }

    BOOL ok = TRUE;
    if (on) {
        if (current[0] && path_equal(current, target)) {
            registry_value_free(&raw);
            RegCloseKey(desktop);
            return TRUE;
        }
        BOOL backup_valid = FALSE;
        RegistryValue old_backup = { 0 };
        if (!screensaver_backup_read(&backup_valid, &old_backup)) ok = FALSE;
        registry_value_free(&old_backup);
        if (ok && backup_valid) ok = screensaver_backup_clear();
        if (ok) ok = screensaver_backup_save(&raw);
        if (ok) {
            DWORD size = (DWORD)((wcslen(target) + 1) * sizeof(WCHAR));
            ok = RegSetValueExW(desktop, L"SCRNSAVE.EXE", 0, REG_SZ,
                                (const BYTE *)target, size) == ERROR_SUCCESS;
            if (!ok) screensaver_backup_clear();
            else notify_screensaver_change();
        }
    } else if (current[0] && path_equal(current, target)) {
        BOOL backup_valid = FALSE;
        if (!screensaver_backup_read(&backup_valid, &original)) ok = FALSE;
        if (ok && backup_valid && original.present) {
            ok = RegSetValueExW(desktop, L"SCRNSAVE.EXE", 0, original.type,
                                original.data, original.size) == ERROR_SUCCESS;
        } else if (ok) {
            LSTATUS result = RegDeleteValueW(desktop, L"SCRNSAVE.EXE");
            ok = result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
        }
        if (ok) {
            notify_screensaver_change();
            ok = screensaver_backup_clear();
        }
    } else {
        ok = screensaver_backup_clear();
    }
    registry_value_free(&original);
    registry_value_free(&raw);
    RegCloseKey(desktop);
    return ok;
}

static BOOL screensaver_start_now(void) {
    WCHAR path[MAX_PATH], directory[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, directory, ARRAYSIZE(directory));
    if (!length || length >= ARRAYSIZE(directory)) return FALSE;
    WCHAR *slash = wcsrchr(directory, L'\\');
    if (!slash) return FALSE;
    *slash = 0;
    if (!join_path(path, ARRAYSIZE(path), directory, L"asciiwall.scr"))
        return FALSE;
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return FALSE;
    return (INT_PTR)ShellExecuteW(NULL, L"open", path, L"/s", directory, SW_SHOWNORMAL) > 32;
}

static BOOL command_has_argument(const WCHAR *expected) {
    int count = 0;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return FALSE;
    BOOL found = FALSE;
    for (int i = 1; i < count; i++) {
        if (lstrcmpiW(arguments[i], expected) == 0) {
            found = TRUE;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, PWSTR cmd, int show) {
    BOOL open_settings = command_has_argument(L"--settings");
    BOOL screen_saver = command_has_argument(L"/s") || command_has_argument(L"--screensaver");
    if (command_has_argument(L"--quit")) { HWND h = FindWindowW(CLS_CTL, NULL); if (h) PostMessageW(h, WM_CLOSE, 0, 0); return 0; }
    if (command_has_argument(L"--screensaver-uninstall")) return screensaver_set(FALSE) ? 0 : 1;
    if (command_has_argument(L"--install"))   return autostart_set(TRUE) ? 0 : 1;
    if (command_has_argument(L"--uninstall")) return autostart_set(FALSE) ? 0 : 1;
    if (open_settings) {
        HWND existing = FindWindowW(CLS_CTL, NULL);
        if (existing) { PostMessageW(existing, WM_APP_SETTINGS, 0, 0); return 0; }
    }

    CreateMutexW(NULL, FALSE, screen_saver ? L"Local\\AsciiWall.ScreenSaver" : L"Local\\AsciiWall");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;            /* один екземпляр */

    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#ifdef PROCESS_POWER_THROTTLING_CURRENT_VERSION                       /* EcoQoS / «режим ефективності» */
    PROCESS_POWER_THROTTLING_STATE pt = { PROCESS_POWER_THROTTLING_CURRENT_VERSION,
        PROCESS_POWER_THROTTLING_EXECUTION_SPEED, PROCESS_POWER_THROTTLING_EXECUTION_SPEED };
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pt, sizeof pt);
#endif

    GetModuleFileNameW(NULL, g.dir, MAX_PATH);
    *wcsrchr(g.dir, L'\\') = 0;
    if (!initialize_settings_path()) {
        MessageBoxW(NULL,
                    L"Could not initialize %APPDATA%\\AsciiWall\\settings.ini. "
                    L"Make sure the app's settings.ini is beside the executable and that your user profile is writable.",
                    L"AsciiWall", MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSW wc = { 0 };
    wc.hInstance = hi;
    wc.lpfnWndProc = ctl_proc;  wc.lpszClassName = CLS_CTL;  RegisterClassW(&wc);
    if (screen_saver) {
        wc.lpszClassName = CLS_SAVER_CTL;
        RegisterClassW(&wc);
    }
    wc.lpfnWndProc = wall_proc; wc.lpszClassName = CLS_WALL; wc.style = CS_OWNDC; RegisterClassW(&wc);
    if (!screen_saver && !settings_ui_init(hi)) {
        MessageBoxW(NULL, L"Could not initialize the settings controls.", L"AsciiWall",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    /* Приховане керуюче вікно: ніколи не показується, тож у панелі задач нічого немає */
    g.ctl = CreateWindowExW(WS_EX_TOOLWINDOW, screen_saver ? CLS_SAVER_CTL : CLS_CTL,
                            L"AsciiWall", WS_POPUP, 0, 0, 0, 0, NULL, NULL, hi, NULL);
    if (!screen_saver) g.msg_taskbar = RegisterWindowMessageW(L"TaskbarCreated");
    if (!screen_saver && !add_tray_icon())
        MessageBoxW(NULL, L"Could not create the AsciiWall tray icon. You can still open settings with --settings.",
                    L"AsciiWall", MB_OK | MB_ICONERROR);

    QueryPerformanceFrequency(&g.qpf); QueryPerformanceCounter(&g.t0); g.tprev = g.t0;
    g.frame_timer = CreateWaitableTimerW(NULL, FALSE, NULL);

    update_monitor();
    read_ini();
    if (screen_saver) {
        g.screen_saver = TRUE;
        g.enabled = TRUE;
        g.covered = FALSE;
        if (!create_screensaver_window()) {
            MessageBoxW(NULL, L"Could not create the AsciiWall screensaver window.",
                        L"AsciiWall", MB_OK | MB_ICONERROR);
            DestroyWindow(g.ctl);
            return 1;
        }
    } else if (!attach()) SetTimer(g.ctl, TM_REATTACH, 2000, NULL);
    update_mouse_hook();
    load_scene();
    if (screen_saver) {
        ShowWindow(g.wall, SW_SHOW);
        UpdateWindow(g.wall);
        SetForegroundWindow(g.wall);
        SetFocus(g.wall);
        SetCursor(NULL);
    }

    if (!screen_saver) {
        WTSRegisterSessionNotification(g.ctl, NOTIFY_FOR_THIS_SESSION);
        RegisterPowerSettingNotification(g.ctl, &kDisplayState, DEVICE_NOTIFY_WINDOW_HANDLE);
        RegisterPowerSettingNotification(g.ctl, &kAcDc, DEVICE_NOTIFY_WINDOW_HANDLE);
        static const DWORD ev[][2] = {
            { EVENT_SYSTEM_FOREGROUND,     EVENT_SYSTEM_FOREGROUND },
            { EVENT_SYSTEM_MOVESIZEEND,    EVENT_SYSTEM_MOVESIZEEND },
            { EVENT_SYSTEM_MINIMIZESTART,  EVENT_SYSTEM_MINIMIZEEND },
            { EVENT_OBJECT_SHOW,           EVENT_OBJECT_HIDE },
            { EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE },
            { EVENT_OBJECT_CLOAKED,        EVENT_OBJECT_UNCLOAKED },
        };
        for (int i = 0; i < 6; i++)
            SetWinEventHook(ev[i][0], ev[i][1], NULL, on_win_event, 0, 0,
                            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }
    g.dir_watch = FindFirstChangeNotificationW(g.dir, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
    g.ini_watch = FindFirstChangeNotificationW(g.config_dir, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
    if (!screen_saver) g.covered = desktop_covered();
    set_running(TRUE);
    if (open_settings) PostMessageW(g.ctl, WM_APP_SETTINGS, 0, 0);

    /* Головний цикл: без опитування. У паузі таймер скасовано — потік спить. */
    MSG msg;
    for (;;) {
        HANDLE hs[3];
        DWORD n = 0, dir_watch_index = MAXDWORD, ini_watch_index = MAXDWORD;
        hs[n++] = g.frame_timer;
        if (g.dir_watch != INVALID_HANDLE_VALUE) {
            dir_watch_index = n;
            hs[n++] = g.dir_watch;
        }
        if (g.ini_watch != INVALID_HANDLE_VALUE) {
            ini_watch_index = n;
            hs[n++] = g.ini_watch;
        }
        DWORD r = MsgWaitForMultipleObjectsEx(n, hs, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (r == WAIT_FAILED) break;
        if (r == WAIT_OBJECT_0) tick();
        else if (dir_watch_index != MAXDWORD && r == WAIT_OBJECT_0 + dir_watch_index) {
            FindNextChangeNotification(g.dir_watch);
            SetTimer(g.ctl, TM_RELOAD, 300, NULL);          /* редактори пишуть файл кількома кроками */
        }
        else if (ini_watch_index != MAXDWORD && r == WAIT_OBJECT_0 + ini_watch_index) {
            FindNextChangeNotification(g.ini_watch);
            SetTimer(g.ctl, TM_RELOAD, 300, NULL);
        }
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
    }
done:
    CancelWaitableTimer(g.frame_timer);
    if (g.mouse_hook) UnhookWindowsHookEx(g.mouse_hook);
    if (g.dir_watch != INVALID_HANDLE_VALUE) FindCloseChangeNotification(g.dir_watch);
    if (g.ini_watch != INVALID_HANDLE_VALUE) FindCloseChangeNotification(g.ini_watch);
    if (g.wall) {                                          /* повернути звичайні шпалери */
        HWND p = GetParent(g.wall);
        BOOL was_screensaver = g.screen_saver;
        if (was_screensaver) SetCursor(LoadCursorW(NULL, IDC_ARROW));
        g.screen_saver = FALSE;
        DestroyWindow(g.wall);
        if (!was_screensaver)
            RedrawWindow(p, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    if (g.L) lua_close(g.L);
    return 0;
}