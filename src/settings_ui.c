#define WIN32_LEAN_AND_MEAN
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include "settings_ui.h"

#ifndef ASCIIWALL_VERSION
#define ASCIIWALL_VERSION "dev"
#endif

#define WIDEN_LITERAL_INNER(value) L##value
#define WIDEN_LITERAL(value) WIDEN_LITERAL_INNER(value)

#define CLS_SETTINGS L"AsciiWall.Settings"
#define IDI_APP_ICON 1
#define ID_TAB 100
#define ID_SAVE 101
#define ID_RANDOMIZE 102
#define ID_DEFAULTS 103
#define ID_CLOSE 104
#define ID_TREE_LIGHTS 105
#define ID_FPS_DEFAULTS 106
#define ID_WALLPAPER_ENABLED 107
#define ID_RUN_AT_STARTUP 108
#define ID_SCREENSAVER_ENABLED 109
#define ID_START_SCREENSAVER 110
#define ID_MOUSE_INTERACTIONS 111
#define ID_GROUND_TRACE_PERMANENT 112
#define ID_SUBTAB 113
#define ID_SEARCH_EDIT 114
#define ID_SEARCH_PREVIOUS 115
#define ID_SIZE_COMBO 116
#define ID_SEARCH_NEXT 117
#define ID_SEARCH_PAGE_LABEL 118
#define ID_SEARCH_RESULT_BASE 120
#define ID_TRACK_BASE 200
#define ID_VALUE_BASE 300
#define SETTINGS_WIDTH 760
#define SETTINGS_HEIGHT 510
#define SEARCH_PAGE_SIZE 7

enum {
    PAGE_SCREEN, PAGE_SCREENSAVER, PAGE_SIZE, PAGE_WEATHER, PAGE_FIRE,
    PAGE_TREES, PAGE_GROUND, PAGE_INTERACTION, PAGE_COUNT
};

enum { CATEGORY_GENERAL, CATEGORY_SCENE, CATEGORY_INTERACTION, CATEGORY_SEARCH, CATEGORY_COUNT };

typedef struct {
    const WCHAR *key, *label, *section;
    int page, decimals;
    double min, max, step, def;
    HWND label_hwnd, track, value_hwnd;
} SettingControl;

typedef struct {
    const WCHAR *key, *label;
    int page, control_index, command_id;
} SearchTarget;

static SettingControl s_controls[] = {
    {L"fps", L"Plugged-in FPS", L"engine", PAGE_SCREEN, 0, 1, 60, 1, 15},
    {L"fps_battery", L"Battery FPS", L"engine", PAGE_SCREEN, 0, 1, 60, 1, 8},
    {L"stars_density", L"Star amount", L"scene", PAGE_WEATHER, 1, 0, 5, 0.1, 1},
    {L"stars_speed", L"Star twinkle speed", L"scene", PAGE_WEATHER, 1, 0, 10, 0.1, 1},
    {L"stars_variety", L"Star color variety", L"scene", PAGE_WEATHER, 2, 0, 1, 0.01, 1},
    {L"snow_density", L"Snow amount", L"scene", PAGE_WEATHER, 1, 0, 5, 0.1, 1},
    {L"snow_speed", L"Snow fall speed", L"scene", PAGE_WEATHER, 1, 0, 10, 0.1, 1},
    {L"snow_drift", L"Snow sway", L"scene", PAGE_WEATHER, 1, 0, 5, 0.1, 1},
    {L"wind", L"Wind (left - / right +)", L"scene", PAGE_FIRE, 1, -20, 20, 0.1, 0},
    {L"fire_power", L"Fire power", L"scene", PAGE_FIRE, 1, 0, 3, 0.1, 1},
    {L"fire_speed", L"Fire speed", L"scene", PAGE_FIRE, 1, 0, 5, 0.1, 1},
    {L"fire_width", L"Fire width", L"scene", PAGE_FIRE, 2, 0.25, 3, 0.05, 1},
    {L"tree_count", L"Number of trees", L"scene", PAGE_TREES, 0, 0, 20, 1, 10},
    {L"tree_jitter", L"Tree position variation", L"scene", PAGE_TREES, 2, 0, 0.45, 0.01, 0.25},
    {L"tree_min_scale", L"Smallest tree size", L"scene", PAGE_TREES, 2, 0.35, 3, 0.05, 0.75},
    {L"tree_max_scale", L"Largest tree size", L"scene", PAGE_TREES, 2, 0.35, 4, 0.05, 1.9},
    {L"tree_snow", L"Tree-top snow coverage", L"scene", PAGE_TREES, 2, 0, 1, 0.01, 0.15},
    {L"tree_light_count", L"Trees with lights", L"scene", PAGE_TREES, 0, 0, 20, 1, 4},
    {L"tree_light_interval", L"Light blink interval (seconds)", L"scene", PAGE_TREES, 2, 0.25, 30, 0.25, 1.5},
    {L"coal_rate", L"Coal emission chance", L"scene", PAGE_GROUND, 2, 0, 1, 0.01, 0.35},
    {L"glow_power", L"Snow glow", L"scene", PAGE_GROUND, 1, 0, 3, 0.1, 1},
    {L"ground_trace_seconds", L"Ground trail duration (seconds)", L"scene", PAGE_INTERACTION, 0, 1, 60, 1, 10},
    {L"mouse_fire_power", L"Fire gust power", L"scene", PAGE_INTERACTION, 1, 0, 3, 0.1, 1},
    {L"mouse_snow_radius", L"Snow attraction range (cells)", L"scene", PAGE_INTERACTION, 0, 1, 12, 1, 7},
    {L"mouse_snow_pull", L"Snow pull strength", L"scene", PAGE_INTERACTION, 1, 0.1, 3, 0.1, 1},
    {L"mouse_snow_limit", L"Maximum held snowflakes", L"scene", PAGE_INTERACTION, 0, 1, 72, 1, 36},
    {L"tree_shake_power", L"Tree snow shower", L"scene", PAGE_INTERACTION, 1, 0, 3, 0.1, 1}
};

static const WCHAR *s_pages[PAGE_COUNT] = {
    L"Screen", L"Screensaver", L"Size", L"Weather", L"Fire",
    L"Trees and Lights", L"Ground and Effects", L"Interaction"
};
static const WCHAR *s_categories[CATEGORY_COUNT] = {
    L"General", L"Scene", L"Interaction", L"Search"
};
static const WCHAR *s_page_hints[PAGE_COUNT] = {
    L"Set the frame rate for plugged-in and battery operation.",
    L"Choose AsciiWall for this Windows account or start the screensaver now.",
    L"Choosing a size applies its scene profile immediately. Save to keep the changes.",
    L"Adjust stars and snow. Typed values are clamped to their allowed range.",
    L"Adjust flame and wind. Type values or drag sliders; out-of-range values are clamped.",
    L"Set tree spacing, size, and snow coverage. Turn on lights to edit their settings.",
    L"Control embers and snow glow. Type values or drag sliders; values are clamped.",
    L"Hold left mouse to gather snow; click snowy trees to shake them. Tune effects below."
};
static HWND s_window, s_tab, s_subtab, s_tree_lights, s_fps_defaults, s_size_combo;
static HWND s_size_label;
static HWND s_wallpaper_enabled, s_run_at_startup, s_screensaver_enabled;
static HWND s_start_screensaver, s_screensaver_hint, s_hint;
static HWND s_mouse_interactions, s_ground_trace_permanent;
static HWND s_search_edit, s_search_previous, s_search_next, s_search_page_label;
static HWND s_search_result_buttons[SEARCH_PAGE_SIZE];
static HINSTANCE s_instance;
static const WCHAR *s_ini;
static SettingsReloadFn s_reload;
static SettingsStartupFn s_set_startup;
static SettingsScreensaverFn s_set_screensaver;
static SettingsScreensaverStartFn s_start_screensaver_now;
static BOOL s_startup_enabled, s_screensaver_initial;
static int s_scale = 100;
static int s_current_page, s_current_category;
static BOOL s_build_failed;
static HFONT s_font, s_value_font, s_search_font, s_header_font, s_subtitle_font;
static HBRUSH s_background_brush, s_header_brush, s_accent_brush;
static HICON s_app_icon;
static const COLORREF s_background_color = RGB(246, 248, 252);
static const WCHAR s_title[] = L"Winter Campfire v" WIDEN_LITERAL(ASCIIWALL_VERSION);
enum { SEARCH_EXTRA_COUNT = 7 };
static SearchTarget s_search_targets[ARRAYSIZE(s_controls) + SEARCH_EXTRA_COUNT];
static int s_search_target_count;
static int s_search_matches[ARRAYSIZE(s_controls) + SEARCH_EXTRA_COUNT];
static int s_search_match_count, s_search_page;

static int px(int v) { return MulDiv(v, s_scale, 100); }

static double clamp_value(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static int control_steps(const SettingControl *c) {
    return (int)floor((c->max - c->min) / c->step + 0.5);
}

static double snap_value(const SettingControl *c, double v) {
    double n = floor((clamp_value(v, c->min, c->max) - c->min) / c->step + 0.5);
    return c->min + n * c->step;
}

static void format_value(const SettingControl *c, double value, WCHAR *out, size_t count) {
    swprintf(out, count, c->decimals == 0 ? L"%.0f" :
             (c->decimals == 1 ? L"%.1f" : L"%.2f"), value);
}

static double read_setting(const SettingControl *c) {
    WCHAR text[64], *end = NULL;
    GetPrivateProfileStringW(c->section, c->key, L"", text, ARRAYSIZE(text), s_ini);
    if (!text[0]) return c->def;
    double v = wcstod(text, &end);
    return end && *end == 0 && isfinite(v) ? snap_value(c, v) : c->def;
}

static double control_value(const SettingControl *c) {
    int pos = (int)SendMessageW(c->track, TBM_GETPOS, 0, 0);
    return c->min + pos * c->step;
}

static void update_tree_dependencies(void);

static void set_control_value(SettingControl *c, double value) {
    value = snap_value(c, value);
    int pos = (int)floor((value - c->min) / c->step + 0.5);
    SendMessageW(c->track, TBM_SETPOS, TRUE, pos);
    WCHAR text[32];
    format_value(c, value, text, ARRAYSIZE(text));
    SetWindowTextW(c->value_hwnd, text);
}

static void show_page_hint(const WCHAR *text) {
    if (s_hint) SetWindowTextW(s_hint, text);
}

static void commit_value_edit(int index) {
    SettingControl *c = &s_controls[index];
    WCHAR text[64], *end = NULL;
    GetWindowTextW(c->value_hwnd, text, ARRAYSIZE(text));
    double value = wcstod(text, &end);
    BOOL parsed = end && end != text;
    while (end && iswspace(*end)) end++;
    if (!parsed || *end != 0 || !isfinite(value)) {
        set_control_value(c, control_value(c));
        show_page_hint(L"Enter a valid number; the previous value was kept.");
        return;
    }

    set_control_value(c, clamp_value(value, c->min, c->max));
    if (!wcscmp(c->key, L"tree_count") || !wcscmp(c->key, L"tree_light_count"))
        update_tree_dependencies();
    show_page_hint(s_page_hints[s_current_page]);
}

static LRESULT CALLBACK value_edit_proc(HWND edit, UINT message, WPARAM w, LPARAM l,
                                        UINT_PTR subclass_id, DWORD_PTR ref_data) {
    if (message == WM_KEYDOWN && w == VK_RETURN) {
        commit_value_edit((int)ref_data);
        return 0;
    }
    return DefSubclassProc(edit, message, w, l);
}

static int find_control(const WCHAR *key) {
    for (int i = 0; i < ARRAYSIZE(s_controls); i++)
        if (!wcscmp(s_controls[i].key, key)) return i;
    return -1;
}

static int category_for_page(int page) {
    if (page == PAGE_SCREEN || page == PAGE_SCREENSAVER) return CATEGORY_GENERAL;
    if (page == PAGE_SIZE) return CATEGORY_SCENE;
    if (page >= PAGE_WEATHER && page <= PAGE_GROUND) return CATEGORY_SCENE;
    return CATEGORY_INTERACTION;
}

static int page_for_category_index(int category, int index) {
    for (int page = 0, found = 0; page < PAGE_COUNT; page++) {
        if (category_for_page(page) == category && found++ == index) return page;
    }
    return -1;
}

static int category_index_for_page(int page) {
    int category = category_for_page(page), index = 0;
    for (int candidate = 0; candidate < page; candidate++)
        if (category_for_page(candidate) == category) index++;
    return index;
}

static BOOL contains_text_ci(const WCHAR *text, const WCHAR *query) {
    if (!query[0]) return TRUE;
    for (const WCHAR *start = text; *start; start++) {
        const WCHAR *left = start, *right = query;
        while (*left && *right && towlower(*left) == towlower(*right)) {
            left++;
            right++;
        }
        if (!*right) return TRUE;
    }
    return FALSE;
}

static void update_tree_dependencies(void) {
    int count_index = find_control(L"tree_count");
    int lights_index = find_control(L"tree_light_count");
    double count = control_value(&s_controls[count_index]);
    double lights = control_value(&s_controls[lights_index]);
    if (lights > count) set_control_value(&s_controls[lights_index], count);
    BOOL enabled = SendMessageW(s_tree_lights, BM_GETCHECK, 0, 0) == BST_CHECKED && count > 0;
    for (int i = 0; i < ARRAYSIZE(s_controls); i++) {
        if (!wcscmp(s_controls[i].key, L"tree_light_count") ||
            !wcscmp(s_controls[i].key, L"tree_light_interval")) {
            EnableWindow(s_controls[i].track, enabled);
            EnableWindow(s_controls[i].label_hwnd, enabled);
            EnableWindow(s_controls[i].value_hwnd, enabled);
        }
    }
}

static void update_interaction_dependencies(void) {
    int trace_index = find_control(L"ground_trace_seconds");
    BOOL enabled = SendMessageW(s_mouse_interactions, BM_GETCHECK, 0, 0) == BST_CHECKED;
    BOOL timed = SendMessageW(s_ground_trace_permanent, BM_GETCHECK, 0, 0) != BST_CHECKED;
    EnableWindow(s_ground_trace_permanent, enabled);
    for (int i = 0; i < ARRAYSIZE(s_controls); i++) {
        if (s_controls[i].page != PAGE_INTERACTION) continue;
        BOOL control_enabled = enabled && (i != trace_index || timed);
        EnableWindow(s_controls[i].track, control_enabled);
        EnableWindow(s_controls[i].label_hwnd, control_enabled);
        EnableWindow(s_controls[i].value_hwnd, control_enabled);
    }
}

static void hide_search_controls(void) {
    ShowWindow(s_search_edit, SW_HIDE);
    ShowWindow(s_search_previous, SW_HIDE);
    ShowWindow(s_search_next, SW_HIDE);
    ShowWindow(s_search_page_label, SW_HIDE);
    for (int i = 0; i < SEARCH_PAGE_SIZE; i++)
        ShowWindow(s_search_result_buttons[i], SW_HIDE);
}

static void show_page(int page) {
    s_current_page = page;
    s_current_category = category_for_page(page);
    if (s_subtab) TabCtrl_SetCurSel(s_subtab, category_index_for_page(page));
    for (int i = 0; i < ARRAYSIZE(s_controls); i++) {
        int cmd = s_controls[i].page == page ? SW_SHOW : SW_HIDE;
        ShowWindow(s_controls[i].label_hwnd, cmd);
        ShowWindow(s_controls[i].track, cmd);
        ShowWindow(s_controls[i].value_hwnd, cmd);
    }
    ShowWindow(s_tree_lights, page == PAGE_TREES ? SW_SHOW : SW_HIDE);
    ShowWindow(s_fps_defaults, page == PAGE_SCREEN ? SW_SHOW : SW_HIDE);
    ShowWindow(s_wallpaper_enabled, page == PAGE_SCREEN ? SW_SHOW : SW_HIDE);
    ShowWindow(s_run_at_startup, page == PAGE_SCREEN ? SW_SHOW : SW_HIDE);
    ShowWindow(s_screensaver_enabled, page == PAGE_SCREENSAVER ? SW_SHOW : SW_HIDE);
    ShowWindow(s_start_screensaver, page == PAGE_SCREENSAVER ? SW_SHOW : SW_HIDE);
    ShowWindow(s_screensaver_hint, page == PAGE_SCREENSAVER ? SW_SHOW : SW_HIDE);
    ShowWindow(s_mouse_interactions, page == PAGE_INTERACTION ? SW_SHOW : SW_HIDE);
    ShowWindow(s_ground_trace_permanent, page == PAGE_INTERACTION ? SW_SHOW : SW_HIDE);
    ShowWindow(s_size_label, page == PAGE_SIZE ? SW_SHOW : SW_HIDE);
    ShowWindow(s_size_combo, page == PAGE_SIZE ? SW_SHOW : SW_HIDE);
    hide_search_controls();
    SetWindowTextW(s_hint, s_page_hints[page]);
    update_tree_dependencies();
    update_interaction_dependencies();
}

static void add_search_target(const WCHAR *key, const WCHAR *label, int page,
                              int control_index, int command_id) {
    if (s_search_target_count >= ARRAYSIZE(s_search_targets)) return;
    s_search_targets[s_search_target_count++] =
        (SearchTarget){ key, label, page, control_index, command_id };
}

static void initialize_search_targets(void) {
    static const struct {
        const WCHAR *key, *label;
        int page, command_id;
    } extra[] = {
        {L"tree_lights", L"Enable tree lights", PAGE_TREES, ID_TREE_LIGHTS},
        {L"enabled", L"Enable animated wallpaper", PAGE_SCREEN, ID_WALLPAPER_ENABLED},
        {L"run_at_startup", L"Run at Windows startup", PAGE_SCREEN, ID_RUN_AT_STARTUP},
        {L"screensaver", L"Use AsciiWall as this account's screensaver", PAGE_SCREENSAVER, ID_SCREENSAVER_ENABLED},
        {L"mouse_interactions", L"Enable mouse interactions", PAGE_INTERACTION, ID_MOUSE_INTERACTIONS},
        {L"ground_trace_permanent", L"Keep ground trails until the scene reloads", PAGE_INTERACTION, ID_GROUND_TRACE_PERMANENT},
        {L"size", L"Scene size preset (Small, Medium, Large)", PAGE_SIZE, ID_SIZE_COMBO}
    };
    s_search_target_count = 0;
    for (int i = 0; i < ARRAYSIZE(s_controls); i++)
        add_search_target(s_controls[i].key, s_controls[i].label,
                          s_controls[i].page, i, 0);
    for (int i = 0; i < ARRAYSIZE(extra); i++)
        add_search_target(extra[i].key, extra[i].label, extra[i].page, -1,
                          extra[i].command_id);
}

static void render_search_page(void) {
    int page_count = (s_search_match_count + SEARCH_PAGE_SIZE - 1) / SEARCH_PAGE_SIZE;
    for (int row = 0; row < SEARCH_PAGE_SIZE; row++) {
        int match = s_search_page * SEARCH_PAGE_SIZE + row;
        if (match >= s_search_match_count) {
            ShowWindow(s_search_result_buttons[row], SW_HIDE);
            continue;
        }

        SearchTarget *target = &s_search_targets[s_search_matches[match]];
        int category = category_for_page(target->page);
        WCHAR display[256];
        swprintf(display, ARRAYSIZE(display), L"%ls  -  %ls > %ls",
                 target->label, s_categories[category], s_pages[target->page]);
        SetWindowTextW(s_search_result_buttons[row], display);
        ShowWindow(s_search_result_buttons[row], SW_SHOW);
    }

    BOOL has_results = s_search_match_count > 0;
    ShowWindow(s_search_previous, has_results ? SW_SHOW : SW_HIDE);
    ShowWindow(s_search_next, has_results ? SW_SHOW : SW_HIDE);
    ShowWindow(s_search_page_label, has_results ? SW_SHOW : SW_HIDE);
    if (has_results) {
        WCHAR page_text[32];
        swprintf(page_text, ARRAYSIZE(page_text), L"Page %d of %d",
                 s_search_page + 1, page_count);
        SetWindowTextW(s_search_page_label, page_text);
        EnableWindow(s_search_previous, s_search_page > 0);
        EnableWindow(s_search_next, s_search_page + 1 < page_count);
        SetWindowTextW(s_hint, L"Select a result to open its setting.");
    }
}

static void update_search_results(void) {
    if (!s_search_edit) return;
    WCHAR query[128];
    GetWindowTextW(s_search_edit, query, ARRAYSIZE(query));
    s_search_match_count = 0;
    s_search_page = 0;

    if (query[0]) {
        for (int i = 0; i < s_search_target_count; i++) {
            SearchTarget *target = &s_search_targets[i];
            int category = category_for_page(target->page);
            if (!contains_text_ci(target->label, query) &&
                !contains_text_ci(target->key, query) &&
                !contains_text_ci(s_pages[target->page], query) &&
                !contains_text_ci(s_categories[category], query))
                continue;

            int insert = s_search_match_count;
            while (insert > 0 &&
                   lstrcmpiW(s_search_targets[s_search_matches[insert - 1]].label,
                             target->label) > 0) {
                s_search_matches[insert] = s_search_matches[insert - 1];
                insert--;
            }
            s_search_matches[insert] = i;
            s_search_match_count++;
        }
    }

    render_search_page();
    if (!query[0])
        SetWindowTextW(s_hint, L"Type a setting name or keyword to see matching settings.");
    else if (s_search_match_count == 0)
        SetWindowTextW(s_hint, L"No settings match. Try another word.");
}

static BOOL populate_subtabs(int category) {
    TabCtrl_DeleteAllItems(s_subtab);
    int index = 0;
    for (int page = 0; page < PAGE_COUNT; page++) {
        if (category_for_page(page) != category) continue;
        TCITEMW item = { 0 };
        item.mask = TCIF_TEXT;
        item.pszText = (WCHAR *)s_pages[page];
        if (TabCtrl_InsertItem(s_subtab, index++, &item) == -1) return FALSE;
    }
    return index > 0;
}

static void show_category(int category) {
    s_current_category = category;
    TabCtrl_SetCurSel(s_tab, category);
    if (category == CATEGORY_SEARCH) {
        for (int i = 0; i < ARRAYSIZE(s_controls); i++) {
            ShowWindow(s_controls[i].label_hwnd, SW_HIDE);
            ShowWindow(s_controls[i].track, SW_HIDE);
            ShowWindow(s_controls[i].value_hwnd, SW_HIDE);
        }
        ShowWindow(s_tree_lights, SW_HIDE);
        ShowWindow(s_fps_defaults, SW_HIDE);
        ShowWindow(s_wallpaper_enabled, SW_HIDE);
        ShowWindow(s_run_at_startup, SW_HIDE);
        ShowWindow(s_screensaver_enabled, SW_HIDE);
        ShowWindow(s_start_screensaver, SW_HIDE);
        ShowWindow(s_screensaver_hint, SW_HIDE);
        ShowWindow(s_mouse_interactions, SW_HIDE);
        ShowWindow(s_ground_trace_permanent, SW_HIDE);
        ShowWindow(s_size_label, SW_HIDE);
        ShowWindow(s_size_combo, SW_HIDE);
        ShowWindow(s_subtab, SW_HIDE);
        ShowWindow(s_search_edit, SW_SHOW);
        update_search_results();
        SetFocus(s_search_edit);
        return;
    }

    hide_search_controls();
    ShowWindow(s_subtab, SW_SHOW);
    if (!populate_subtabs(category)) {
        SetWindowTextW(s_hint, L"Could not open this settings category.");
        return;
    }
    int page = page_for_category_index(category, 0);
    TabCtrl_SetCurSel(s_subtab, 0);
    show_page(page);
}

static void open_search_target(int target_index) {
    if (target_index < 0 || target_index >= s_search_target_count) return;
    SearchTarget *target = &s_search_targets[target_index];
    show_category(category_for_page(target->page));
    show_page(target->page);
    HWND focus = target->control_index >= 0
        ? s_controls[target->control_index].value_hwnd
        : GetDlgItem(s_window, target->command_id);
    if (focus) {
        SetFocus(focus);
        if (target->control_index >= 0)
            SendMessageW(focus, EM_SETSEL, 0, -1);
    }
}

static BOOL write_value(const WCHAR *section, const WCHAR *key, const WCHAR *value) {
    if (WritePrivateProfileStringW(section, key, value, s_ini)) return TRUE;
    WCHAR message[256];
    swprintf(message, ARRAYSIZE(message), L"Could not save setting \"%ls\" to settings.ini.", key);
    MessageBoxW(s_window, message, L"AsciiWall settings", MB_OK | MB_ICONERROR);
    return FALSE;
}

static int selected_scene_size(void) {
    int selection = (int)SendMessageW(s_size_combo, CB_GETCURSEL, 0, 0);
    return selection >= 0 && selection < 3 ? selection + 1 : 1;
}

static int random_int(int lo, int hi);

static void save_settings(void) {
    int count_index = find_control(L"tree_count");
    int lights_index = find_control(L"tree_light_count");
    double tree_count = control_value(&s_controls[count_index]);
    double lights = min((int)control_value(&s_controls[lights_index]), (int)tree_count);
    set_control_value(&s_controls[lights_index], lights);

    for (int i = 0; i < ARRAYSIZE(s_controls); i++) {
        SettingControl *c = &s_controls[i];
        double value = i == lights_index ? lights : control_value(c);
        WCHAR text[32];
        format_value(c, value, text, ARRAYSIZE(text));
        if (!write_value(c->section, c->key, text)) return;
    }
    WCHAR size_text[8];
    swprintf(size_text, ARRAYSIZE(size_text), L"%d", selected_scene_size());
    if (!write_value(L"scene", L"size", size_text)) return;
    if (!write_value(L"scene", L"tree_lights",
                     SendMessageW(s_tree_lights, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0"))
        return;
    if (!write_value(L"engine", L"enabled",
                     SendMessageW(s_wallpaper_enabled, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0"))
        return;
    if (!write_value(L"engine", L"mouse_interactions",
                     SendMessageW(s_mouse_interactions, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0"))
        return;
    if (!write_value(L"scene", L"ground_trace_permanent",
                     SendMessageW(s_ground_trace_permanent, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0"))
        return;
    /* A profile-cache flush returns zero even when it succeeds. */
    WritePrivateProfileStringW(NULL, NULL, NULL, s_ini);
    BOOL startup_saved = s_set_startup &&
        s_set_startup(SendMessageW(s_run_at_startup, BM_GETCHECK, 0, 0) == BST_CHECKED);
    BOOL screensaver_enabled =
        SendMessageW(s_screensaver_enabled, BM_GETCHECK, 0, 0) == BST_CHECKED;
    BOOL screensaver_saved = TRUE;
    if (screensaver_enabled != s_screensaver_initial) {
        screensaver_saved = s_set_screensaver && s_set_screensaver(screensaver_enabled);
        if (screensaver_saved) s_screensaver_initial = screensaver_enabled;
    }
    if (s_reload) s_reload();
    if (!startup_saved || !screensaver_saved) {
        const WCHAR *message = !startup_saved && !screensaver_saved
            ? L"Settings were saved, but the Windows startup and screensaver options could not both be updated."
            : (!startup_saved
                ? L"Settings were saved, but the Windows startup option could not be updated."
                : L"Settings were saved, but the Windows screensaver option could not be updated.");
        MessageBoxW(s_window, message, L"AsciiWall settings", MB_OK | MB_ICONERROR);
    }
}

static void apply_defaults(void) {
    for (int i = 0; i < ARRAYSIZE(s_controls); i++)
        if (s_controls[i].page != PAGE_SCREEN)
            set_control_value(&s_controls[i], s_controls[i].def);

    static const struct {
        double snow_density, snow_speed, fire_width, fire_power;
        double tree_count, tree_min_scale, tree_max_scale, tree_light_count;
        double mouse_fire_power, mouse_snow_radius, mouse_snow_pull;
        double mouse_snow_limit, tree_shake_power;
        int wind_min_tenths, wind_max_tenths;
    } presets[] = {
        {0.8, 0.9, 1.0, 1.0, 10, 0.6, 1.5, 4, 1.0, 7, 1.0, 36, 1.0, 10, 25},
        {1.1, 1.1, 1.25, 1.35, 8, 0.9, 2.1, 3, 1.4, 8, 1.3, 45, 1.4, 25, 45},
        {1.4, 1.3, 1.55, 2.5, 6, 2.5, 3.8, 2, 1.8, 9, 1.6, 54, 1.8, 40, 65}
    };
    const int size = selected_scene_size();
    const int preset_index = size - 1;
    const double values[] = {
        presets[preset_index].snow_density,
        presets[preset_index].snow_speed,
        presets[preset_index].fire_width,
        presets[preset_index].fire_power,
        presets[preset_index].tree_count,
        presets[preset_index].tree_min_scale,
        presets[preset_index].tree_max_scale,
        presets[preset_index].tree_light_count,
        presets[preset_index].mouse_fire_power,
        presets[preset_index].mouse_snow_radius,
        presets[preset_index].mouse_snow_pull,
        presets[preset_index].mouse_snow_limit,
        presets[preset_index].tree_shake_power
    };
    static const WCHAR *keys[] = {
        L"snow_density", L"snow_speed", L"fire_width", L"fire_power",
        L"tree_count", L"tree_min_scale", L"tree_max_scale", L"tree_light_count",
        L"mouse_fire_power", L"mouse_snow_radius", L"mouse_snow_pull",
        L"mouse_snow_limit", L"tree_shake_power"
    };
    for (int i = 0; i < ARRAYSIZE(keys); i++) {
        int index = find_control(keys[i]);
        if (index >= 0) set_control_value(&s_controls[index], values[i]);
    }
    int wind_sign = rand() % 2 ? 1 : -1;
    double wind = wind_sign * random_int(presets[preset_index].wind_min_tenths,
                                          presets[preset_index].wind_max_tenths) / 10.0;
    int wind_index = find_control(L"wind");
    if (wind_index >= 0) set_control_value(&s_controls[wind_index], wind);

    SendMessageW(s_tree_lights, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(s_ground_trace_permanent, BM_SETCHECK, BST_UNCHECKED, 0);
    update_tree_dependencies();
    update_interaction_dependencies();
}

static void apply_fps_defaults(void) {
    for (int i = 0; i < ARRAYSIZE(s_controls); i++)
        if (s_controls[i].page == PAGE_SCREEN)
            set_control_value(&s_controls[i], s_controls[i].def);
}

static int random_int(int lo, int hi) {
    return lo + rand() % (hi - lo + 1);
}

static void randomize_settings(void) {
    static const struct { const WCHAR *key; double lo, hi; } ranges[] = {
        {L"stars_density", 0.3, 2.2}, {L"stars_speed", 0.5, 2.5},
        {L"stars_variety", 0.35, 1}, {L"snow_density", 0.3, 2.0},
        {L"snow_speed", 0.5, 2.5}, {L"snow_drift", 0.4, 1.8},
        {L"wind", -8, 8}, {L"fire_power", 0.6, 2.4},
        {L"fire_speed", 0.5, 3.0}, {L"fire_width", 0.6, 2.2},
        {L"tree_count", 4, 14}, {L"tree_jitter", 0.08, 0.4},
        {L"tree_min_scale", 0.5, 1.1}, {L"tree_max_scale", 1.3, 2.8},
        {L"tree_snow", 0, 0.4},
        {L"ground_trace_seconds", 5, 20},
        {L"mouse_fire_power", 0.4, 2.2},
        {L"mouse_snow_radius", 3, 10}, {L"mouse_snow_pull", 0.5, 2},
        {L"mouse_snow_limit", 12, 48}, {L"tree_shake_power", 0.4, 2.2},
        {L"tree_light_interval", 0.75, 4},
        {L"coal_rate", 0.1, 0.8}, {L"glow_power", 0.4, 1.8}
    };
    for (int i = 0; i < ARRAYSIZE(ranges); i++) {
        int index = find_control(ranges[i].key);
        double v = ranges[i].lo + (double)rand() / RAND_MAX * (ranges[i].hi - ranges[i].lo);
        set_control_value(&s_controls[index], v);
    }
    int tree_count_index = find_control(L"tree_count");
    int lights_index = find_control(L"tree_light_count");
    int tree_count = (int)control_value(&s_controls[tree_count_index]);
    set_control_value(&s_controls[lights_index], random_int(0, min(tree_count, 10)));
    SendMessageW(s_tree_lights, BM_SETCHECK, rand() % 5 == 0 ? BST_UNCHECKED : BST_CHECKED, 0);
    update_tree_dependencies();
}

static BOOL create_child(HWND parent, HWND *out, DWORD exstyle, const WCHAR *klass,
                         const WCHAR *text, DWORD style, int x, int y, int w, int h, int id) {
    *out = CreateWindowExW(exstyle, klass, text, style, px(x), px(y), px(w), px(h),
                           parent, (HMENU)(INT_PTR)id, s_instance, NULL);
    if (*out && s_font) SendMessageW(*out, WM_SETFONT, (WPARAM)s_font, TRUE);
    return *out != NULL;
}

static BOOL build_controls(HWND h) {
    s_tab = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        px(12), px(78), px(736), px(24), h, (HMENU)(INT_PTR)ID_TAB, s_instance, NULL);
    if (!s_tab) return FALSE;
    if (s_font) SendMessageW(s_tab, WM_SETFONT, (WPARAM)s_font, TRUE);
    TabCtrl_SetPadding(s_tab, px(12), 0);
    for (int i = 0; i < CATEGORY_COUNT; i++) {
        TCITEMW item = { 0 };
        item.mask = TCIF_TEXT;
        item.pszText = (WCHAR *)s_categories[i];
        if (TabCtrl_InsertItem(s_tab, i, &item) == -1) return FALSE;
    }

    s_subtab = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        px(12), px(120), px(736), px(24), h, (HMENU)(INT_PTR)ID_SUBTAB, s_instance, NULL);
    if (!s_subtab) return FALSE;
    if (s_font) SendMessageW(s_subtab, WM_SETFONT, (WPARAM)s_font, TRUE);
    TabCtrl_SetPadding(s_subtab, px(10), 0);

    if (!create_child(h, &s_hint, 0, L"STATIC", s_page_hints[PAGE_SCREEN],
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 24, 162, 712, 28, 0) ||
        !create_child(h, &s_search_edit, 0, L"EDIT", L"",
            WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
            24, 198, 712, 24, ID_SEARCH_EDIT))
        return FALSE;
    if (s_search_font)
        SendMessageW(s_search_edit, WM_SETFONT, (WPARAM)s_search_font, TRUE);
    for (int i = 0; i < SEARCH_PAGE_SIZE; i++) {
        int id = ID_SEARCH_RESULT_BASE + i;
        if (!create_child(h, &s_search_result_buttons[i], 0, L"BUTTON", L"",
                WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON | BS_FLAT | BS_LEFT | BS_VCENTER,
                24, 234 + i * 25, 712, 23, id))
            return FALSE;
    }
    if (!create_child(h, &s_search_previous, 0, L"BUTTON", L"Previous",
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 24, 416, 96, 28,
            ID_SEARCH_PREVIOUS) ||
        !create_child(h, &s_search_page_label, 0, L"STATIC", L"",
            WS_CHILD | SS_CENTER | SS_CENTERIMAGE, 330, 416, 136, 28,
            ID_SEARCH_PAGE_LABEL) ||
        !create_child(h, &s_search_next, 0, L"BUTTON", L"Next",
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 640, 416, 96, 28,
            ID_SEARCH_NEXT))
        return FALSE;

    for (int i = 0; i < ARRAYSIZE(s_controls); i++) {
        SettingControl *c = &s_controls[i];
        int row = 0;
        for (int j = 0; j < i; j++) if (s_controls[j].page == c->page) row++;
        int y = c->page == PAGE_INTERACTION ? 230 + row * 30
            : (c->page == PAGE_TREES ? 228 + row * 30 : 198 + row * 32);
        int id = ID_TRACK_BASE + i;
        WCHAR initial[32];
        format_value(c, read_setting(c), initial, ARRAYSIZE(initial));
        if (!create_child(h, &c->label_hwnd, 0, L"STATIC", c->label,
                          WS_CHILD | SS_LEFT | SS_CENTERIMAGE, 24, y + 3, 236, 28, 0) ||
            !create_child(h, &c->track, 0, TRACKBAR_CLASSW, L"",
                          WS_CHILD | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
                          280, y, 280, 30, id) ||
            !create_child(h, &c->value_hwnd, 0, L"EDIT", initial,
                          WS_CHILD | WS_TABSTOP | WS_BORDER | ES_CENTER | ES_AUTOHSCROLL,
                          580, y + 3, 84, 26, ID_VALUE_BASE + i))
            return FALSE;
        if (s_value_font)
            SendMessageW(c->value_hwnd, WM_SETFONT, (WPARAM)s_value_font, TRUE);
        if (!SetWindowSubclass(c->value_hwnd, value_edit_proc, 1, (DWORD_PTR)i))
            return FALSE;
        int steps = control_steps(c);
        SendMessageW(c->track, TBM_SETRANGE, TRUE, MAKELONG(0, steps));
        SendMessageW(c->track, TBM_SETPAGESIZE, 0, max(1, steps / 10));
        set_control_value(c, read_setting(c));
    }

    if (!create_child(h, &s_size_label, 0, L"STATIC", L"Scene size (observer distance)",
            WS_CHILD | SS_LEFT | SS_CENTERIMAGE, 24, 198, 236, 30, 0) ||
        !create_child(h, &s_size_combo, 0, WC_COMBOBOXW, L"",
            WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
            280, 198, 260, 120, ID_SIZE_COMBO))
        return FALSE;
    static const WCHAR *size_names[] = {L"1 - Small", L"2 - Medium", L"3 - Large"};
    for (int i = 0; i < ARRAYSIZE(size_names); i++)
        if (SendMessageW(s_size_combo, CB_ADDSTRING, 0, (LPARAM)size_names[i]) == CB_ERR)
            return FALSE;
    int configured_size = GetPrivateProfileIntW(L"scene", L"size", 1, s_ini);
    if (configured_size < 1 || configured_size > 3) configured_size = 1;
    SendMessageW(s_size_combo, CB_SETCURSEL, configured_size - 1, 0);

    if (!create_child(h, &s_tree_lights, 0, L"BUTTON", L"Enable tree lights",
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 24, 198, 300, 30, ID_TREE_LIGHTS))
        return FALSE;
    SendMessageW(s_tree_lights, BM_SETCHECK,
        GetPrivateProfileIntW(L"scene", L"tree_lights", 1, s_ini) ? BST_CHECKED : BST_UNCHECKED, 0);

    if (!create_child(h, &s_fps_defaults, 0, L"BUTTON", L"FPS Defaults",
            WS_CHILD | WS_TABSTOP, 24, 278, 120, 32, ID_FPS_DEFAULTS))
        return FALSE;
    if (!create_child(h, &s_wallpaper_enabled, 0, L"BUTTON", L"Enable animated wallpaper",
            WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 24, 322, 300, 30, ID_WALLPAPER_ENABLED) ||
        !create_child(h, &s_run_at_startup, 0, L"BUTTON", L"Run at Windows startup",
            WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 24, 356, 300, 30, ID_RUN_AT_STARTUP))
        return FALSE;
    if (!create_child(h, &s_screensaver_enabled, 0, L"BUTTON", L"Use AsciiWall as this account's screensaver",
            WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 24, 198, 390, 30, ID_SCREENSAVER_ENABLED) ||
        !create_child(h, &s_start_screensaver, 0, L"BUTTON", L"Start screensaver now",
            WS_CHILD | WS_TABSTOP, 430, 197, 190, 32, ID_START_SCREENSAVER) ||
        !create_child(h, &s_screensaver_hint, 0, L"STATIC",
            L"This per-user saver is not listed in Windows' picker. Windows' idle timeout and sign-in options stay unchanged.",
            WS_CHILD | SS_LEFT | SS_CENTERIMAGE, 24, 238, 712, 36, 0))
        return FALSE;
    SendMessageW(s_wallpaper_enabled, BM_SETCHECK,
        GetPrivateProfileIntW(L"engine", L"enabled", 1, s_ini) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(s_run_at_startup, BM_SETCHECK,
        s_startup_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(s_screensaver_enabled, BM_SETCHECK,
        s_screensaver_initial ? BST_CHECKED : BST_UNCHECKED, 0);

    if (!create_child(h, &s_mouse_interactions, 0, L"BUTTON",
            L"Enable mouse interactions (desktop icons remain clickable)",
            WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 24, 180, 640, 30,
            ID_MOUSE_INTERACTIONS) ||
        !create_child(h, &s_ground_trace_permanent, 0, L"BUTTON",
            L"Keep ground trails until the scene reloads",
            WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 24, 416, 500, 30,
            ID_GROUND_TRACE_PERMANENT))
        return FALSE;
    SendMessageW(s_mouse_interactions, BM_SETCHECK,
        GetPrivateProfileIntW(L"engine", L"mouse_interactions", 1, s_ini)
            ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(s_ground_trace_permanent, BM_SETCHECK,
        GetPrivateProfileIntW(L"scene", L"ground_trace_permanent", 0, s_ini)
            ? BST_CHECKED : BST_UNCHECKED, 0);
    update_interaction_dependencies();

    HWND button;
    if (!create_child(h, &button, 0, L"BUTTON", L"Randomize",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 20, 462, 100, 32, ID_RANDOMIZE) ||
        !create_child(h, &button, 0, L"BUTTON", L"Defaults",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 130, 462, 90, 32, ID_DEFAULTS) ||
        !create_child(h, &button, 0, L"BUTTON", L"Save",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 550, 462, 80, 32, ID_SAVE) ||
        !create_child(h, &button, 0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 640, 462, 100, 32, ID_CLOSE))
        return FALSE;

    initialize_search_targets();
    show_category(CATEGORY_GENERAL);
    return TRUE;
}

static LRESULT CALLBACK settings_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        if (!build_controls(h)) {
            s_build_failed = TRUE;
            return -1;
        }
        return 0;
    case WM_ERASEBKGND: {
        RECT client;
        GetClientRect(h, &client);
        FillRect((HDC)w, &client, s_background_brush);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT client, header;
        GetClientRect(h, &client);
        header.left = 0;
        header.top = 0;
        header.right = client.right;
        header.bottom = px(68);
        FillRect(dc, &header, s_header_brush);
        RECT accent = { 0, px(65), client.right, px(68) };
        FillRect(dc, &accent, s_accent_brush);
        if (s_app_icon)
            DrawIconEx(dc, px(18), px(12), s_app_icon, px(44), px(44), 0, NULL, DI_NORMAL);
        int saved = SaveDC(dc);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        if (s_header_font) SelectObject(dc, s_header_font);
        TextOutW(dc, px(76), px(13), s_title, ARRAYSIZE(s_title) - 1);
        SetTextColor(dc, RGB(199, 218, 237));
        if (s_subtitle_font) SelectObject(dc, s_subtitle_font);
        TextOutW(dc, px(76), px(37), L"Personalize your ASCII wallpaper", 32);
        RestoreDC(dc, saved);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)w, OPAQUE);
        SetBkColor((HDC)w, s_background_color);
        SetTextColor((HDC)w, RGB(36, 42, 50));
        return (LRESULT)s_background_brush;
    case WM_CTLCOLOREDIT:
        SetBkMode((HDC)w, OPAQUE);
        SetBkColor((HDC)w, RGB(255, 255, 255));
        SetTextColor((HDC)w, RGB(36, 42, 50));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_CTLCOLORBTN:
        SetBkMode((HDC)w, TRANSPARENT);
        return (LRESULT)s_background_brush;
    case WM_NOTIFY:
        if (((NMHDR *)l)->code == TCN_SELCHANGE) {
            if (((NMHDR *)l)->idFrom == ID_TAB) {
                show_category(TabCtrl_GetCurSel(s_tab));
                return 0;
            }
            if (((NMHDR *)l)->idFrom == ID_SUBTAB) {
                int page = page_for_category_index(s_current_category,
                                                   TabCtrl_GetCurSel(s_subtab));
                if (page >= 0) show_page(page);
                return 0;
            }
        }
        return 0;
    case WM_HSCROLL:
        for (int i = 0; i < ARRAYSIZE(s_controls); i++) {
            if ((HWND)l == s_controls[i].track) {
                set_control_value(&s_controls[i], control_value(&s_controls[i]));
                if (!wcscmp(s_controls[i].key, L"tree_count")) update_tree_dependencies();
                if (!wcscmp(s_controls[i].key, L"tree_light_count")) update_tree_dependencies();
                break;
            }
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == ID_SEARCH_EDIT && HIWORD(w) == EN_CHANGE) {
            update_search_results();
            return 0;
        }
        if (LOWORD(w) == ID_SIZE_COMBO && HIWORD(w) == CBN_SELCHANGE) {
            apply_defaults();
            return 0;
        }
        if (HIWORD(w) == BN_CLICKED && LOWORD(w) == ID_SEARCH_PREVIOUS) {
            if (s_search_page > 0) {
                s_search_page--;
                render_search_page();
            }
            return 0;
        }
        if (HIWORD(w) == BN_CLICKED && LOWORD(w) == ID_SEARCH_NEXT) {
            int page_count = (s_search_match_count + SEARCH_PAGE_SIZE - 1) / SEARCH_PAGE_SIZE;
            if (s_search_page + 1 < page_count) {
                s_search_page++;
                render_search_page();
            }
            return 0;
        }
        if (HIWORD(w) == BN_CLICKED &&
            LOWORD(w) >= ID_SEARCH_RESULT_BASE &&
            LOWORD(w) < ID_SEARCH_RESULT_BASE + SEARCH_PAGE_SIZE) {
            int row = LOWORD(w) - ID_SEARCH_RESULT_BASE;
            int match = s_search_page * SEARCH_PAGE_SIZE + row;
            if (match < s_search_match_count)
                open_search_target(s_search_matches[match]);
            return 0;
        }
        if (HIWORD(w) == EN_KILLFOCUS &&
            LOWORD(w) >= ID_VALUE_BASE &&
            LOWORD(w) < ID_VALUE_BASE + ARRAYSIZE(s_controls)) {
            commit_value_edit(LOWORD(w) - ID_VALUE_BASE);
            return 0;
        }
        switch (LOWORD(w)) {
        case ID_SAVE: save_settings(); return 0;
        case ID_RANDOMIZE: randomize_settings(); return 0;
        case ID_DEFAULTS: apply_defaults(); return 0;
        case ID_FPS_DEFAULTS: apply_fps_defaults(); return 0;
        case ID_START_SCREENSAVER:
            if (!s_start_screensaver_now || !s_start_screensaver_now())
                MessageBoxW(s_window, L"Could not start the AsciiWall screensaver. Make sure asciiwall.scr is beside asciiwall.exe.",
                            L"AsciiWall", MB_OK | MB_ICONERROR);
            return 0;
        case ID_CLOSE: DestroyWindow(h); return 0;
        case ID_TREE_LIGHTS: update_tree_dependencies(); return 0;
        case ID_MOUSE_INTERACTIONS:
        case ID_GROUND_TRACE_PERMANENT: update_interaction_dependencies(); return 0;
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        s_window = NULL;
        s_tab = NULL;
        s_subtab = NULL;
        s_tree_lights = NULL;
        s_size_combo = NULL;
        s_size_label = NULL;
        s_fps_defaults = NULL;
        s_wallpaper_enabled = NULL;
        s_run_at_startup = NULL;
        s_screensaver_enabled = NULL;
        s_start_screensaver = NULL;
        s_screensaver_hint = NULL;
        s_hint = NULL;
        s_search_edit = NULL;
        s_search_previous = NULL;
        s_search_next = NULL;
        s_search_page_label = NULL;
        for (int i = 0; i < SEARCH_PAGE_SIZE; i++)
            s_search_result_buttons[i] = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

BOOL settings_ui_init(HINSTANCE instance) {
    s_instance = instance;
    INITCOMMONCONTROLSEX controls = { sizeof controls, ICC_BAR_CLASSES | ICC_TAB_CLASSES };
    if (!InitCommonControlsEx(&controls)) return FALSE;
    s_background_brush = CreateSolidBrush(s_background_color);
    s_header_brush = CreateSolidBrush(RGB(20, 38, 61));
    s_accent_brush = CreateSolidBrush(RGB(0, 120, 215));
    s_app_icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!s_app_icon) s_app_icon = LoadIconW(NULL, IDI_APPLICATION);
    if (!s_background_brush || !s_header_brush || !s_accent_brush) return FALSE;

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = settings_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = s_app_icon;
    wc.hbrBackground = s_background_brush;
    wc.lpszClassName = CLS_SETTINGS;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return FALSE;
    return TRUE;
}

void settings_ui_show(HWND owner, const WCHAR *ini, SettingsReloadFn reload,
                      SettingsStartupFn set_startup, BOOL startup_enabled,
                      SettingsScreensaverFn set_screensaver, BOOL screensaver_enabled,
                      SettingsScreensaverStartFn start_screensaver) {
    if (s_window) {
        ShowWindow(s_window, SW_RESTORE);
        SetForegroundWindow(s_window);
        return;
    }
    s_ini = ini;
    s_reload = reload;
    s_set_startup = set_startup;
    s_set_screensaver = set_screensaver;
    s_start_screensaver_now = start_screensaver;
    s_startup_enabled = startup_enabled;
    s_screensaver_initial = screensaver_enabled;
    s_scale = GetDpiForSystem() * 100 / 96;
    if (!s_font) {
        s_font = CreateFontW(-px(10), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        s_value_font = CreateFontW(-px(10), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        s_search_font = CreateFontW(-px(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        s_header_font = CreateFontW(-px(15), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        s_subtitle_font = CreateFontW(-px(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }
    srand(GetTickCount() ^ GetCurrentProcessId());
    s_build_failed = FALSE;
    RECT r = { 0, 0, px(SETTINGS_WIDTH), px(SETTINGS_HEIGHT) };
    AdjustWindowRectEx(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                       FALSE, WS_EX_APPWINDOW);
    HWND h = CreateWindowExW(WS_EX_APPWINDOW, CLS_SETTINGS, L"AsciiWall Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        owner, NULL, s_instance, NULL);
    if (!h || s_build_failed) {
        MessageBoxW(owner, L"Could not create the settings window.", L"AsciiWall",
                    MB_OK | MB_ICONERROR);
        return;
    }
    s_window = h;
    ShowWindow(h, SW_SHOWNORMAL);
    UpdateWindow(h);
}

void settings_ui_close(void) {
    if (s_window) DestroyWindow(s_window);
    if (s_font) { DeleteObject(s_font); s_font = NULL; }
    if (s_value_font) { DeleteObject(s_value_font); s_value_font = NULL; }
    if (s_search_font) { DeleteObject(s_search_font); s_search_font = NULL; }
    if (s_header_font) { DeleteObject(s_header_font); s_header_font = NULL; }
    if (s_subtitle_font) { DeleteObject(s_subtitle_font); s_subtitle_font = NULL; }
    if (s_background_brush) { DeleteObject(s_background_brush); s_background_brush = NULL; }
    if (s_header_brush) { DeleteObject(s_header_brush); s_header_brush = NULL; }
    if (s_accent_brush) { DeleteObject(s_accent_brush); s_accent_brush = NULL; }
}
