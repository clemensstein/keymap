// Trackpad-/Maus-Messung fuer Windows.
//
// Zeichnet jede Bewegungsmeldung aller Zeigegeraete ueber die Raw-Input-API mit
// hochaufloesendem Zeitstempel auf und wertet pro Geraet aus, wie gleichmaessig
// die Meldungen ankommen. Stottern zeigt sich als Luecken (lange Pausen) oder
// Doppelpakete (zwei Meldungen fast gleichzeitig nach einem Stau).
//
// Bauen:
//   MSVC:  cl /O2 /EHsc trackpad_messung.cpp user32.lib hid.lib
//   MinGW: g++ -O2 -static -o trackpad_messung.exe trackpad_messung.cpp -luser32 -lhid
//
// Aufruf:
//   trackpad_messung.exe [Sekunden=20] [ausgabe.csv]

#define __USE_MINGW_ANSI_STDIO 1 // %zu auch mit aelteren MinGW-Laufzeiten
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
extern "C" {
#include <hidsdi.h>
}

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

struct Sample {
    double t_ms;
    HANDLE device;
    LONG dx;
    LONG dy;
};

static std::vector<Sample> g_samples;
static LARGE_INTEGER g_freq;
static LARGE_INTEGER g_start;

static double now_ms() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - g_start.QuadPart) * 1000.0 / (double)g_freq.QuadPart;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_INPUT: {
        double t = now_ms();
        RAWINPUT raw;
        UINT size = sizeof(raw);
        if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) !=
                (UINT)-1 &&
            raw.header.dwType == RIM_TYPEMOUSE) {
            const RAWMOUSE &m = raw.data.mouse;
            // Nur relative Bewegungen (Maus, Trackpad als Maus)
            if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE) && (m.lLastX != 0 || m.lLastY != 0)) {
                g_samples.push_back({t, raw.header.hDevice, m.lLastX, m.lLastY});
            }
        }
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    case WM_TIMER:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static std::wstring device_path(HANDLE dev) {
    UINT len = 0;
    GetRawInputDeviceInfoW(dev, RIDI_DEVICENAME, NULL, &len);
    if (len == 0) {
        return L"";
    }
    std::wstring name(len, L'\0');
    if (GetRawInputDeviceInfoW(dev, RIDI_DEVICENAME, &name[0], &len) == (UINT)-1) {
        return L"";
    }
    name.resize(wcslen(name.c_str()));
    return name;
}

static std::wstring product_name(const std::wstring &path) {
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return L"";
    }
    wchar_t buf[128] = {0};
    BOOLEAN ok = HidD_GetProductString(h, buf, sizeof(buf));
    CloseHandle(h);
    return ok ? std::wstring(buf) : L"";
}

static std::string narrow(const std::wstring &w) {
    std::string s;
    for (wchar_t c : w) {
        s += (c >= 32 && c < 127) ? (char)c : '?';
    }
    return s;
}

static double percentile(std::vector<double> v, double p) {
    if (v.empty()) {
        return 0;
    }
    std::sort(v.begin(), v.end());
    size_t i = (size_t)std::min<double>(v.size() - 1, std::floor(p / 100.0 * (v.size() - 1) + 0.5));
    return v[i];
}

int main(int argc, char **argv) {
    int seconds = argc > 1 ? atoi(argv[1]) : 20;
    if (seconds <= 0) {
        seconds = 20;
    }
    std::string out = argc > 2 ? argv[2] : "trackpad_messung.csv";

    QueryPerformanceFrequency(&g_freq);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"TrackpadMessung";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL,
                              wc.hInstance, NULL);

    // Alle Mausgeraete, auch wenn ein anderes Fenster den Fokus hat
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        printf("Fehler: RegisterRawInputDevices (%lu)\n", GetLastError());
        return 1;
    }

    printf("Trackpad-Messung: %d Sekunden.\n", seconds);
    printf("In 3 Sekunden geht es los. Dann den Cursor in langen, schnellen Zuegen\n");
    printf("hin und her bewegen (so, wie das Stottern auftritt).\n\n");
    for (int i = 3; i > 0; i--) {
        printf("  %d...\n", i);
        Sleep(1000);
    }
    printf("  Los! Bewegen...\n");

    // Waehrend des Countdowns aufgelaufene Meldungen verwerfen
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    }
    g_samples.clear();
    g_samples.reserve(200000);
    QueryPerformanceCounter(&g_start);
    SetTimer(hwnd, 1, seconds * 1000, NULL);
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        DispatchMessage(&msg);
    }
    printf("  Fertig.\n\n");

    // Geraete einsammeln
    std::map<HANDLE, int> index;
    std::vector<HANDLE> devices;
    for (const Sample &s : g_samples) {
        if (index.find(s.device) == index.end()) {
            index[s.device] = (int)devices.size();
            devices.push_back(s.device);
        }
    }

    FILE *csv = fopen(out.c_str(), "w");
    if (csv) {
        fprintf(csv, "# Trackpad-Messung, %d s, %zu Meldungen\n", seconds, g_samples.size());
    }

    if (devices.empty()) {
        printf("Keine Bewegung erkannt.\n");
    }

    for (size_t d = 0; d < devices.size(); d++) {
        std::wstring path = device_path(devices[d]);
        std::wstring product = product_name(path);

        std::vector<const Sample *> s;
        for (const Sample &x : g_samples) {
            if (x.device == devices[d]) {
                s.push_back(&x);
            }
        }

        // Abstaende nur innerhalb zusammenhaengender Bewegungen (Pause > 100 ms trennt)
        std::vector<double> iv;
        double move_ms = 0;
        for (size_t i = 1; i < s.size(); i++) {
            double dt = s[i]->t_ms - s[i - 1]->t_ms;
            if (dt <= 100.0) {
                iv.push_back(dt);
                move_ms += dt;
            }
        }

        printf("==================================================================\n");
        printf("Geraet %zu: %s\n", d + 1, product.empty() ? "(Name unbekannt)" : narrow(product).c_str());
        printf("  %s\n", narrow(path).c_str());
        printf("  Meldungen: %zu   davon in Bewegung ausgewertet: %zu Abstaende\n", s.size(),
               iv.size());

        if (csv) {
            fprintf(csv, "# Geraet %zu: %s | %s\n", d + 1, narrow(product).c_str(),
                    narrow(path).c_str());
        }

        if (iv.size() < 20) {
            printf("  Zu wenig Bewegung fuer eine Auswertung.\n\n");
            continue;
        }

        double med = percentile(iv, 50);
        double mean = move_ms / iv.size();
        size_t gaps = 0, doubles = 0;
        for (double v : iv) {
            if (v > 1.8 * med) {
                gaps++;
            }
            if (v < 1.5) {
                doubles++;
            }
        }

        printf("  Melderate:      %.0f Hz (typischer Abstand %.2f ms, Mittel %.2f ms)\n",
               1000.0 / med, med, mean);
        printf("  Abstaende:      p90 %.2f ms   p99 %.2f ms   max %.2f ms\n", percentile(iv, 90),
               percentile(iv, 99), percentile(iv, 100));
        printf("  Luecken:        %zu (%.1f %%)  = Abstand > 1,8x typisch -> sichtbares Stocken\n",
               gaps, 100.0 * gaps / iv.size());
        printf("  Doppelpakete:   %zu (%.1f %%)  = Abstand < 1,5 ms -> Stau wurde nachgeholt\n",
               doubles, 100.0 * doubles / iv.size());

        // Histogramm
        const double edges[] = {0, 2, 4, 6, 8, 10, 12, 15, 20, 30, 50, 100.01};
        const int n_bins = (int)(sizeof(edges) / sizeof(edges[0])) - 1;
        std::vector<size_t> bins(n_bins, 0);
        for (double v : iv) {
            for (int b = 0; b < n_bins; b++) {
                if (v >= edges[b] && v < edges[b + 1]) {
                    bins[b]++;
                    break;
                }
            }
        }
        size_t max_bin = *std::max_element(bins.begin(), bins.end());
        printf("  Verteilung der Abstaende:\n");
        for (int b = 0; b < n_bins; b++) {
            int bar = max_bin ? (int)(40.0 * bins[b] / max_bin + 0.5) : 0;
            printf("   %5.0f-%-5.0f ms %6zu  %s\n", edges[b], edges[b + 1], bins[b],
                   std::string(bar, '#').c_str());
        }
        printf("\n");
    }

    if (csv) {
        fprintf(csv, "t_ms,geraet,dx,dy\n");
        for (const Sample &s : g_samples) {
            fprintf(csv, "%.3f,%d,%ld,%ld\n", s.t_ms, index[s.device] + 1, s.dx, s.dy);
        }
        fclose(csv);
        printf("Rohdaten gespeichert: %s\n", out.c_str());
    }

    printf("\nTipp: Einmal kabellos und einmal mit der linken Haelfte am USB-Kabel messen.\n");
    return 0;
}
