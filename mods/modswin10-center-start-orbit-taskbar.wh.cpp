// ==WindhawkMod==
// @id win10-center-start-orbit-taskbar
// @name Windows 10 Center Start Orbit Taskbar
// @description Center Start exactly, place Task View to its right, and let task buttons extend around them.
// @version 0.1
// @author B
// @github bnk1
// @include explorer.exe
// @architecture x86-64
// ==/WindhawkMod==

#include <windows.h>

UINT_PTR g_timer = 1;

// Tune these only.
const int StartGap = 6;
const int LeftMinimum = 20;
const int VisibleTaskWidthLeftOfStart = 800;

HWND g_start = nullptr;
HWND g_taskView = nullptr;
HWND g_task = nullptr;

RECT g_startRect = {};
RECT g_taskViewRect = {};
RECT g_taskRect = {};

bool g_saved = false;
bool g_applying = false;

struct FindData
{
    const wchar_t* className;
    HWND hwnd;
};

HWND FindDeepChild(HWND parent, const wchar_t* className)
{
    FindData data = { className, nullptr };

    EnumChildWindows(parent, [](HWND hwnd, LPARAM lParam) -> BOOL
    {
        FindData* data = reinterpret_cast<FindData*>(lParam);

        wchar_t cls[128] = {};
        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));

        if (_wcsicmp(cls, data->className) == 0)
        {
            data->hwnd = hwnd;
            return FALSE;
        }

        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));

    return data.hwnd;
}

int Width(HWND hwnd)
{
    RECT r = {};
    GetWindowRect(hwnd, &r);
    return r.right - r.left;
}

void MoveWindowInParent(HWND hwnd, HWND reference, int x, int y, int w, int h)
{
    HWND parent = GetParent(hwnd);

    if (!parent)
        return;

    POINT pt = { x, y };
    MapWindowPoints(reference, parent, &pt, 1);

    SetWindowPos(hwnd, nullptr, pt.x, pt.y, w, h,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
}

void RestoreWindow(HWND hwnd, const RECT& rect)
{
    if (!hwnd)
        return;

    HWND parent = GetParent(hwnd);

    if (!parent)
        return;

    POINT pt = { rect.left, rect.top };
    MapWindowPoints(nullptr, parent, &pt, 1);

    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    SetWindowPos(hwnd, nullptr, pt.x, pt.y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

HWND FindTaskViewButton(HWND taskbar)
{
    struct Data
    {
        HWND hwnd;
    };

    Data data = {};

    EnumChildWindows(taskbar, [](HWND hwnd, LPARAM lParam) -> BOOL
    {
        Data* data = reinterpret_cast<Data*>(lParam);

        wchar_t cls[128] = {};
        wchar_t text[128] = {};

        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
        GetWindowTextW(hwnd, text, ARRAYSIZE(text));

        if (_wcsicmp(cls, L"TrayButton") == 0 && wcsstr(text, L"Task View"))
        {
            data->hwnd = hwnd;
            return FALSE;
        }

        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));

    return data.hwnd;
}

void CenterTaskbar()
{
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);

    if (!taskbar)
        return;

    HWND start = FindDeepChild(taskbar, L"Start");
    HWND taskView = FindTaskViewButton(taskbar);
    HWND taskSw = FindDeepChild(taskbar, L"MSTaskSwWClass");
    HWND tray = FindDeepChild(taskbar, L"TrayNotifyWnd");

    if (!start || !taskSw)
    {
        Wh_Log(L"Missing windows: start=%p taskSw=%p", start, taskSw);
        return;
    }

    RECT tb = {};
    RECT st = {};
    RECT tv = {};

    GetWindowRect(taskbar, &tb);
    GetWindowRect(start, &st);

    int taskbarWidth = tb.right - tb.left;
    int taskbarHeight = tb.bottom - tb.top;

    int startWidth = st.right - st.left;
    int startHeight = st.bottom - st.top;

    int taskViewWidth = 0;
    int taskViewHeight = taskbarHeight;

    if (taskView)
    {
        GetWindowRect(taskView, &tv);
        taskViewWidth = tv.right - tv.left;
        taskViewHeight = tv.bottom - tv.top;
    }

    int trayWidth = tray ? Width(tray) : 450;
    int usableRight = taskbarWidth - trayWidth;

    // Start button center exactly at the visual usable center.
    int centerX = usableRight / 2;

    int startX = centerX - (startWidth / 2);
    int taskViewX = startX + startWidth + StartGap;

    // Task buttons begin to the left of Start, so they appear to wrap around the centered Start/Task View area.
    int taskX = startX - VisibleTaskWidthLeftOfStart;

    if (taskX < LeftMinimum)
        taskX = LeftMinimum;

    int taskWidth = usableRight - taskX;

    if (taskWidth < 600)
        taskWidth = 600;

    if (!g_saved)
    {
        g_start = start;
        g_taskView = taskView;
        g_task = taskSw;

        GetWindowRect(start, &g_startRect);

        if (taskView)
            GetWindowRect(taskView, &g_taskViewRect);

        GetWindowRect(taskSw, &g_taskRect);

        g_saved = true;

        Wh_Log(L"Saved original positions");
    }

	if (g_applying)
		return;

	g_applying = true;
	
    MoveWindowInParent(taskSw, taskbar, taskX, 0, taskWidth, taskbarHeight);
    MoveWindowInParent(start, taskbar, startX, 0, startWidth, startHeight);

	// existing MoveWindowInParent calls here
	g_applying = false;
	
    if (taskView)
        MoveWindowInParent(taskView, taskbar, taskViewX, 0, taskViewWidth, taskViewHeight);

    Wh_Log(L"Centered orbit: taskX=%d taskWidth=%d startX=%d taskViewX=%d tray=%d", taskX, taskWidth, startX, taskViewX, trayWidth);
}

void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD)
{
    CenterTaskbar();
}

BOOL Wh_ModInit()
{
    Wh_Log(L"Init");

	SetTimer(nullptr, g_timer, 50, TimerProc);
    CenterTaskbar();

    return TRUE;
}

void Wh_ModUninit()
{
    KillTimer(nullptr, g_timer);

    if (g_saved)
    {
        RestoreWindow(g_task, g_taskRect);
        RestoreWindow(g_start, g_startRect);
        RestoreWindow(g_taskView, g_taskViewRect);

        Wh_Log(L"Restored original positions");
    }

    g_saved = false;
    g_start = nullptr;
    g_taskView = nullptr;
    g_task = nullptr;

    Wh_Log(L"Uninit done");
}