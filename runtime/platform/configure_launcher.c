// Super Odyssey Configuration.exe (Windows): the game's settings without the game - it starts SuperOdysseyRecomp.exe
// next to it with --configure (runtime/main.c, runtime/ui/setup.mm configure_run) and exits. A program of its own so it
// has its own name and icon in the folder, the Start menu and Steam (a non-Steam shortcut).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show) {
    (void)inst, (void)prev, (void)args, (void)show;
    wchar_t dir[MAX_PATH], exe[MAX_PATH + 64], cmd[MAX_PATH + 96];
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    if (!n || n >= MAX_PATH) return 1;
    wchar_t *slash = wcsrchr(dir, 92);  // (92: a backslash)
    if (slash) *slash = 0;
    swprintf(exe, sizeof exe / sizeof *exe, L"%ls%lcSuperOdysseyRecomp.exe", dir, (wchar_t)92);
    swprintf(cmd, sizeof cmd / sizeof *cmd, L"\"%ls\" --configure", exe);
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        MessageBoxW(NULL, L"SuperOdysseyRecomp.exe wasn't found next to this program. Keep Super Odyssey Configuration.exe "
                          L"in the game's folder.", L"Super Odyssey Configuration", MB_OK | MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
