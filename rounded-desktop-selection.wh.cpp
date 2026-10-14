// ==WindhawkMod==
// @id              rounded-desktop-selection
// @name            Rounded Desktop Selection
// @name:pt         Seletor arredondado do desktop
// @description     Red translucent desktop drag selection with rounded corners
// @version         0.1.2
// @author          delta
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lgdiplus -lcomctl32 -lgdi32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Rounded Desktop Selection — MVP

Changes only the appearance of the desktop drag-selection rectangle.
Windows retains its native rectangular hit testing and selection behavior.
File Explorer's DirectUI file view is outside this MVP.

Defaults: #E81123, 20% fill opacity, 80% border opacity, 8 DIP radius,
1 DIP border. Settings work with Windhawk 1.7.3.

Disable the mod to restore native drawing. If the required Windows symbols
cannot be resolved, initialization fails and native drawing remains in use.
Do not combine with other mods that replace desktop marquee painting.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Color: "#E81123"
  $name: Color (#RRGGBB)
- FillOpacity: 20
  $name: Fill opacity (0-100%)
- BorderOpacity: 80
  $name: Border opacity (0-100%)
- CornerRadius: 8
  $name: Corner radius (DIP, 0-64)
- BorderWidth: 1
  $name: Border width (DIP, 0-8)
*/
// ==/WindhawkModSettings==

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cwchar>
#ifndef MVP_RENDER_TEST
#include <windhawk_utils.h>
#endif

struct Appearance {
    DWORD rgb = 0xE81123;
    int fillOpacity = 20;
    int borderOpacity = 80;
    int radius = 8;
    int borderWidth = 1;
};

static bool ParseRgb(const wchar_t* text, DWORD& rgb) {
    if (!text) return false;
    if (*text == L'#') ++text;
    if (wcslen(text) != 6) return false;
    DWORD result = 0;
    for (int i = 0; i < 6; ++i) {
        wchar_t c = text[i];
        int digit = c >= L'0' && c <= L'9' ? c - L'0' :
                    c >= L'a' && c <= L'f' ? c - L'a' + 10 :
                    c >= L'A' && c <= L'F' ? c - L'A' + 10 : -1;
        if (digit < 0) return false;
        result = (result << 4) | digit;
    }
    rgb = result;
    return true;
}

static Gdiplus::Color MakeColor(DWORD rgb, int opacity) {
    BYTE alpha = static_cast<BYTE>((std::clamp(opacity, 0, 100) * 255 + 50) / 100);
    return Gdiplus::Color(alpha, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

// Both the live hook and the rendering tests use this exact drawing function.
static bool DrawMarquee(Gdiplus::Graphics& graphics, const RECT& rect,
                        const Appearance& style, float dpiScale) {
    float width = static_cast<float>(rect.right - rect.left);
    float height = static_cast<float>(rect.bottom - rect.top);
    if (width <= 0 || height <= 0) return true;
    float penWidth = std::min(std::clamp(style.borderWidth, 0, 8) * dpiScale,
                              std::min(width, height));
    float inset = penWidth / 2.0f;
    float x = rect.left + inset, y = rect.top + inset;
    float w = width - penWidth, h = height - penWidth;
    // Very thin drag rectangles must remain valid even with a wide border.
    if (w <= 0 || h <= 0) {
        Gdiplus::SolidBrush brush(MakeColor(style.rgb,
            std::max(style.fillOpacity, style.borderOpacity)));
        return graphics.FillRectangle(&brush, static_cast<float>(rect.left),
            static_cast<float>(rect.top), width, height) == Gdiplus::Ok;
    }
    float radius = std::min(std::clamp(style.radius, 0, 64) * dpiScale,
                            std::min(w, h) / 2.0f);
    Gdiplus::GraphicsPath path;
    if (radius > 0) {
        float diameter = radius * 2;
        path.AddArc(x, y, diameter, diameter, 180, 90);
        path.AddArc(x + w - diameter, y, diameter, diameter, 270, 90);
        path.AddArc(x + w - diameter, y + h - diameter, diameter, diameter, 0, 90);
        path.AddArc(x, y + h - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
    } else {
        path.AddRectangle(Gdiplus::RectF(x, y, w, h));
    }
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    Gdiplus::SolidBrush fill(MakeColor(style.rgb, style.fillOpacity));
    if (graphics.FillPath(&fill, &path) != Gdiplus::Ok) return false;
    if (penWidth > 0 && style.borderOpacity > 0) {
        Gdiplus::Pen border(MakeColor(style.rgb, style.borderOpacity), penWidth);
        if (graphics.DrawPath(&border, &path) != Gdiplus::Ok) return false;
    }
    return true;
}

static bool DrawClippedMarquee(Gdiplus::Graphics& graphics, const RECT& selection,
                               const RECT& repaint, const Appearance& style,
                               float dpiScale) {
    auto state = graphics.Save();
    graphics.SetClip(Gdiplus::Rect(repaint.left, repaint.top,
        std::max(0L, repaint.right - repaint.left),
        std::max(0L, repaint.bottom - repaint.top)), Gdiplus::CombineModeIntersect);
    bool result = DrawMarquee(graphics, selection, style, dpiScale);
    graphics.Restore(state);
    return result;
}

#ifndef MVP_RENDER_TEST
static std::atomic<DWORD> g_rgb{0xE81123};
static std::atomic<int> g_fill{20}, g_border{80}, g_radius{8}, g_width{1};
static ULONG_PTR g_gdiplusToken = 0;
static HMODULE g_controls = nullptr;
static thread_local HWND g_currentList = nullptr;
static size_t g_ownerOffset = 0, g_managerOffset = 0, g_rectOffset = 0;
static std::atomic<bool> g_recordedPaint{false};

using PaintMarquee_t = void(__cdecl*)(void*, HDC, void*, const RECT*);
using ListWndProc_t = LRESULT(__cdecl*)(void*, HWND, UINT, WPARAM, LPARAM);
static PaintMarquee_t PaintMarquee_Original;
static PaintMarquee_t PaintMarquee_Target;
static ListWndProc_t ListWndProc_Original;

static bool IsDesktopList(HWND hwnd) {
    if (!hwnd) return false;
    wchar_t className[64]{};
    if (!GetClassNameW(hwnd, className, ARRAYSIZE(className)) ||
        wcscmp(className, L"SysListView32") != 0) return false;
    HWND parent = GetParent(hwnd);
    if (!GetClassNameW(parent, className, ARRAYSIZE(className)) ||
        wcscmp(className, L"SHELLDLL_DefView") != 0) return false;
    if (!GetClassNameW(GetAncestor(hwnd, GA_ROOT), className, ARRAYSIZE(className)))
        return false;
    return wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0;
}

static LRESULT __cdecl ListWndProc_Hook(void* self, HWND hwnd, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
    HWND previous = g_currentList;
    g_currentList = hwnd;
    LRESULT result = ListWndProc_Original(self, hwnd, message, wParam, lParam);
    g_currentList = previous;
    return result;
}

// Derive the exact member offsets that this Windows binary itself uses to
// intersect the dirty rect with its native marquee rect. Never hardcode a
// private layout or follow pointers when this instruction pattern is absent.
static bool ResolveGeometryLayout() {
    const BYTE* code = reinterpret_cast<const BYTE*>(PaintMarquee_Target);
    int ownerAt = -1, constantAt = -1, managerAt = -1, addAt = -1;
    for (int i = 0; i < 112; ++i) {
        if (code[i] == 0x48 && code[i+1] == 0x8B && code[i+2] == 0x41) {
            ownerAt = i; g_ownerOffset = code[i+3];
        }
        if (code[i] == 0x41 && code[i+1] == 0xBE) {
            DWORD offset; memcpy(&offset, code+i+2, sizeof(offset));
            constantAt = i; g_rectOffset = offset;
        }
        if (code[i] == 0x4C && code[i+1] == 0x8B && code[i+2] == 0x80) {
            DWORD offset; memcpy(&offset, code+i+3, sizeof(offset));
            managerAt = i; g_managerOffset = offset;
        }
        if (code[i] == 0x4D && code[i+1] == 0x03 && code[i+2] == 0xC6) addAt = i;
    }
    return ownerAt >= 0 && ownerAt < constantAt && constantAt < managerAt &&
        managerAt < addAt && addAt - managerAt < 24 &&
        g_ownerOffset > 0 && g_ownerOffset <= 128 && g_ownerOffset % 8 == 0 &&
        g_managerOffset > 0 && g_managerOffset < 4096 && g_managerOffset % 8 == 0 &&
        g_rectOffset > 0 && g_rectOffset < 1024 && g_rectOffset % 4 == 0;
}

static bool GetNativeSelectionRect(void* self, RECT& rect) {
    void* owner = nullptr;
    void* manager = nullptr;
    SIZE_T read = 0;
    HANDLE process = GetCurrentProcess();
    if (!ReadProcessMemory(process, static_cast<BYTE*>(self)+g_ownerOffset,
                           &owner, sizeof(owner), &read) || !owner) return false;
    if (!ReadProcessMemory(process, static_cast<BYTE*>(owner)+g_managerOffset,
                           &manager, sizeof(manager), &read) || !manager) return false;
    if (!ReadProcessMemory(process, static_cast<BYTE*>(manager)+g_rectOffset,
                           &rect, sizeof(rect), &read)) return false;
    return rect.right >= rect.left && rect.bottom >= rect.top &&
        static_cast<int64_t>(rect.right) - rect.left < 100000 &&
        static_cast<int64_t>(rect.bottom) - rect.top < 100000;
}

static void __cdecl PaintMarquee_Hook(void* self, HDC dc, void* theme,
                                     const RECT* rect) {
    RECT selection{};
    if (!dc || !rect || !IsDesktopList(g_currentList) ||
        !GetNativeSelectionRect(self, selection)) {
        PaintMarquee_Original(self, dc, theme, rect);
        return;
    }
    Appearance style{g_rgb.load(), g_fill.load(), g_border.load(),
                     g_radius.load(), g_width.load()};
    UINT dpi = GetDpiForWindow(g_currentList);
    Gdiplus::Graphics graphics(dc);
    if (graphics.GetLastStatus() != Gdiplus::Ok ||
        !DrawClippedMarquee(graphics, selection, *rect, style, (dpi ? dpi : 96) / 96.0f)) {
        PaintMarquee_Original(self, dc, theme, rect);
        return;
    }
    if (!g_recordedPaint.exchange(true)) {
        Wh_SetIntValue(L"DesktopPaintObserved", 1);
        Wh_Log(L"Custom desktop marquee painted: %ld,%ld,%ld,%ld, DPI=%u",
            rect->left, rect->top, rect->right, rect->bottom, dpi);
    }
}

static void LoadSettings() {
    WindhawkUtils::StringSetting color(Wh_GetStringSetting(L"Color"));
    DWORD rgb = 0xE81123;
    ParseRgb(color, rgb);
    g_rgb = rgb;
    g_fill = std::clamp(Wh_GetIntSetting(L"FillOpacity"), 0, 100);
    g_border = std::clamp(Wh_GetIntSetting(L"BorderOpacity"), 0, 100);
    g_radius = std::clamp(Wh_GetIntSetting(L"CornerRadius"), 0, 64);
    g_width = std::clamp(Wh_GetIntSetting(L"BorderWidth"), 0, 8);
}

static HMODULE LoadThemedControls() {
    HMODULE shell = LoadLibraryExW(L"shell32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!shell) return nullptr;
    ACTCTXW context{};
    context.cbSize = sizeof(context);
    context.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
    context.hModule = shell;
    context.lpResourceName = MAKEINTRESOURCEW(124);
    HANDLE activation = CreateActCtxW(&context);
    ULONG_PTR cookie = 0;
    HMODULE controls = nullptr;
    if (activation != INVALID_HANDLE_VALUE && ActivateActCtx(activation, &cookie)) {
        controls = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        DeactivateActCtx(0, cookie);
    }
    if (activation != INVALID_HANDLE_VALUE) ReleaseActCtx(activation);
    FreeLibrary(shell);
    return controls;
}

BOOL Wh_ModInit() {
    HWND desktop = GetShellWindow();
    DWORD owner = 0;
    if (desktop) GetWindowThreadProcessId(desktop, &owner);
    if (!owner || owner != GetCurrentProcessId()) return FALSE;
    Wh_SetIntValue(L"DesktopPaintObserved", 0);
    Wh_SetIntValue(L"Initialized", 0);
    LoadSettings();
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) != Gdiplus::Ok)
        return FALSE;
    g_controls = LoadThemedControls();
    const WindhawkUtils::SYMBOL_HOOK resolve[] = {
        {{L"private: void __cdecl CLVDrawManager::_PaintMarquee(struct HDC__ *,void *,struct tagRECT const *)"},
         &PaintMarquee_Target, nullptr},
    };
    const WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {{L"private: void __cdecl CLVDrawManager::_PaintMarquee(struct HDC__ *,void *,struct tagRECT const *)"},
         &PaintMarquee_Original, PaintMarquee_Hook},
        {{L"public: __int64 __cdecl CListView::WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64)"},
         &ListWndProc_Original, ListWndProc_Hook},
    };
    if (!g_controls ||
        !WindhawkUtils::HookSymbols(g_controls, resolve, ARRAYSIZE(resolve)) ||
        !ResolveGeometryLayout() ||
        !WindhawkUtils::HookSymbols(g_controls, hooks, ARRAYSIZE(hooks))) {
        if (g_controls) FreeLibrary(g_controls);
        g_controls = nullptr;
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
        return FALSE;
    }
    Wh_SetIntValue(L"Initialized", 1);
    Wh_Log(L"Rounded desktop selection initialized");
    return TRUE;
}

void Wh_ModSettingsChanged() { LoadSettings(); }

void Wh_ModUninit() {
    Wh_SetIntValue(L"Initialized", 0);
    if (g_controls) FreeLibrary(g_controls);
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
}
#endif
