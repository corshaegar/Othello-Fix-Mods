// Othello-Mod-Unlock - Othello mit beliebigen Union-Plugins (Mods) spielen
// Copyright (C) 2026 corshaegar - AGPL-3.0-or-later, siehe LICENSE
//
// Othello 4.4.2.1 laesst sich nur mit den Plugins starten, die es selbst kennt. Liegt ein anderes Union-Plugin in
// system\Autorun, geschieht eines von beidem:
//   1. Meldung "Remove the following unsupported plugins" und sofortiges Beenden, oder
//   2. stille Sabotage - Othello schreibt 0x5D in den Epilog von fuenf Engine-Funktionen; das Spiel startet und stuerzt
//      spaeter an scheinbar zufaelliger Stelle ab.
// Diese DLL schaltet beide Sperren zur Laufzeit im Speicher ab. Die Dateien von Othello bleiben unveraendert.
//
// --- Engine-Befunde (Othello 4.4.2.1, OTHELLO_ABI.DLL, Wunschbasis 0x10000000; Gothic 1, GothicMod.exe) ----------------
// Othello haengt seine Pruefung in drei Engine-Aufrufe (verschleierte Ziele: Adresse = ~ecx ^ edx, Funktion 0x1008A6F0):
//  - HandledWinMain +0x206 (0x004F4166) und oCGame::Init +0x2B8 (0x00637208, ueber 0x100B0430 -> 0x1000BD90):
//    Verteiler 0x1004C570 waehlt per Zufall eine von 100 gleichen Pruef-Varianten (Tabelle 0x100DB278, nur von hier
//    erreichbar) und ruft sie mit "call eax" (0x1004C5F9) - unbekanntes Plugin -> MessageBoxW + quick_exit.
//    Abschalten: FF D0 -> 31 C0 (xor eax,eax; trifft alle 100 Varianten auf einmal). Nicht 90 90: an 0x004F4166 stand
//    Gothics "call [CreateMenu]" (Othello schreibt dort "call 0x1004C570; nop"), und 0x004F416C legt eax als Menue-Griff
//    nach 0x86F4DC (DeleteMenu-Schleife, DestroyMenu beim Beenden). 0 = "kein Menue", DestroyMenu wird uebersprungen;
//    mit 90 90 stuende dort die Adresse der Pruef-Variante. Der Init-Weg (0x100B0BF1) wertet eax nicht aus.
//  - CGameManager::GameInit +0x191 (0x00425A91 -> 0x10054F30): die stille Sabotage. Abschalten: die beiden
//    Whitelist-Abfragen 0x100553CD / 0x100558B5 "0F 85 rel32" -> "E9 rel32+1, 90" (Plugin gilt als erlaubt), gleiches
//    Sprungziel wie vorher.
// Jede Stelle wird nur geaendert, wenn der Befehl davor mit der (verschobenen) Adresse uebereinstimmt - eine andere
// Othello-Fassung bleibt unberuehrt. Steht schon die Ersetzung da (jemand hat die VDF gepatcht), bleibt es dabei.
// Eingegriffen wird in DllMain (ist OTHELLO_ABI.DLL schon geladen) und sonst per LdrRegisterDllNotification beim Laden -
// beides vor HandledWinMain +0x206, wo Othello die geladenen Plugins prueft. Als Rueckhalt prueft ein Faden die fuenf
// Engine-Bytes und stellt sabotierte wieder her (fuer den Fall, dass die Muster einer anderen Fassung nicht passen).
//
// Herkunft: entstanden als Teil des Gothic-I-II-Universal-Coop (dort OthelloUnlock.cpp, Fassung v79 vom 21.10.2026),
// hier eigenstaendig ohne Union-SDK - nur Windows-Aufrufe, laeuft im Lader.

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define OMU_VERSION "1.0.0"

namespace {

    // --- Stellen in OTHELLO_ABI.DLL -------------------------------------------------------------------------------
    struct UnlockSite {
        DWORD rva;               // Patchstelle
        BYTE size;
        BYTE expect[6];
        BYTE repl[6];
        DWORD sigRva;            // Befehl davor mit Adress-Operand
        BYTE sig[5];
        BYTE sigLen;
        DWORD sigTarget;         // Adresse im Operanden minus Wunschbasis 0x10000000
        const char* what;
    };

    const UnlockSite UnlockSites[] = {
        { 0x4C5F9, 2, { 0xFF, 0xD0 }, { 0x31, 0xC0 }, 0x4C5F2, { 0x8B, 0x04, 0x95 }, 3, 0xDB278,
          "Pruef-Verteiler (Meldung 'unsupported plugins', 100 Varianten)" },
        { 0x553CD, 6, { 0x0F, 0x85, 0xCA, 0x00, 0x00, 0x00 }, { 0xE9, 0xCB, 0x00, 0x00, 0x00, 0x90 },
          0x553C4, { 0x8B, 0x45, 0xE0, 0x3B, 0x05 }, 5, 0x14FDE8, "Sabotage-Pruefung 1" },
        { 0x558B5, 6, { 0x0F, 0x85, 0xCD, 0x00, 0x00, 0x00 }, { 0xE9, 0xCE, 0x00, 0x00, 0x00, 0x90 },
          0x558AC, { 0x8B, 0x45, 0xE0, 0x3B, 0x05 }, 5, 0x14FDE8, "Sabotage-Pruefung 2" },
    };

    // --- GothicMod.exe: Epilog-Bytes, die Othellos Sabotage auf 0x5D (pop ebp) setzt -------------------------------
    struct EpilogByte {
        DWORD va;
        BYTE orig;
    };
    const EpilogByte SabotageBytes[] = {
        { 0x00432EE1, 0x5B }, { 0x005F36E4, 0x5B }, { 0x005F7C2A, 0x5B }, { 0x0064025A, 0x5E }, { 0x00640995, 0x5E },
    };

    // --- Protokoll ------------------------------------------------------------------------------------------------
    // Die Zeilen werden gesammelt und erst geschrieben, wenn Othello wirklich erkannt ist - in einem anderen Spiel
    // legt die DLL keine Datei an und tut nichts.
    CRITICAL_SECTION LogLock;
    bool LogReady = false;
    char LogBuffer[4096] = "";
    char LogPath[MAX_PATH] = "";
    bool LogToFile = false;
    HMODULE SelfModule = NULL;

    void LogWriteFile(const char* text) {
        if (!LogToFile || !LogPath[0])
            return;
        if (FILE* f = fopen(LogPath, "ab")) {
            fwrite(text, 1, strlen(text), f);
            fclose(f);
        }
    }

    // Ab hier wird jede Zeile sofort in die Datei geschrieben; das Gesammelte kommt zuerst hinein.
    void LogStartFile() {
        if (LogToFile || !SelfModule)
            return;
        if (!GetModuleFileNameA(SelfModule, LogPath, MAX_PATH)) {
            LogPath[0] = 0;
            return;
        }
        char* dot = strrchr(LogPath, '.');
        char* slash = strrchr(LogPath, '\\');
        if (!dot || (slash && dot < slash)) {
            LogPath[0] = 0;
            return;
        }
        strcpy(dot, ".log");
        SYSTEMTIME st;
        GetLocalTime(&st);
        char head[200];
        _snprintf(head, sizeof(head), "Othello-Mod-Unlock %s - Start %04d-%02d-%02d %02d:%02d:%02d\r\n",
            OMU_VERSION, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        head[sizeof(head) - 1] = 0;
        if (FILE* f = fopen(LogPath, "wb")) {   // je Start neu
            fwrite(head, 1, strlen(head), f);
            fclose(f);
        }
        LogToFile = true;
        LogWriteFile(LogBuffer);
        LogBuffer[0] = 0;
    }

    void Log(const char* fmt, ...) {
        char line[400];
        va_list ap;
        va_start(ap, fmt);
        _vsnprintf(line, sizeof(line) - 3, fmt, ap);
        va_end(ap);
        line[sizeof(line) - 3] = 0;
        strcat(line, "\r\n");
        if (LogReady)
            EnterCriticalSection(&LogLock);
        if (LogToFile)
            LogWriteFile(line);
        else if (strlen(LogBuffer) + strlen(line) + 1 < sizeof(LogBuffer))
            strcat(LogBuffer, line);
        if (LogReady)
            LeaveCriticalSection(&LogLock);
    }

    // --- Sperren abschalten ---------------------------------------------------------------------------------------
    volatile LONG UnlockDone = 0;
    PVOID NotifyCookie = NULL;
    HANDLE WatchThread = NULL;
    volatile LONG WatchStop = 0;

    DWORD ImageSizeOf(BYTE* base) {
        const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
        if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;
        const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        return nt->Signature == IMAGE_NT_SIGNATURE ? nt->OptionalHeader.SizeOfImage : 0;
    }

    // Rueckgabe: wie viele Stellen diese DLL geaendert hat
    int ApplySites(BYTE* base, DWORD imageSize) {
        int changed = 0;
        Log("OTHELLO_ABI.DLL bei %08X, %u Byte gross", (unsigned)(UINT_PTR)base, (unsigned)imageSize);
        for (int i = 0; i < (int)(sizeof(UnlockSites) / sizeof(UnlockSites[0])); i++) {
            const UnlockSite& s = UnlockSites[i];
            if (s.rva + s.size > imageSize || s.sigRva + s.sigLen + 4 > imageSize) {
                Log("  %05X %s: ausserhalb der DLL - andere Othello-Fassung, nicht geaendert", (unsigned)s.rva, s.what);
                continue;
            }
            BYTE* p = base + s.rva;
            const BYTE* sig = base + s.sigRva;
            const DWORD want = (DWORD)(UINT_PTR)base + s.sigTarget;
            if (memcmp(sig, s.sig, s.sigLen) != 0 || *(const DWORD*)(sig + s.sigLen) != want) {
                Log("  %05X %s: Muster passt nicht - andere Othello-Fassung, nicht geaendert", (unsigned)s.rva, s.what);
                continue;
            }
            if (memcmp(p, s.repl, s.size) == 0) {
                Log("  %05X %s: schon abgeschaltet (gepatchte VDF?)", (unsigned)s.rva, s.what);
                continue;
            }
            if (memcmp(p, s.expect, s.size) != 0) {
                Log("  %05X %s: unerwartete Bytes %02X %02X - nicht geaendert", (unsigned)s.rva, s.what, p[0], p[1]);
                continue;
            }
            DWORD old = 0;
            if (!VirtualProtect(p, s.size, PAGE_EXECUTE_READWRITE, &old)) {
                Log("  %05X %s: VirtualProtect fehlgeschlagen (Fehler %lu)", (unsigned)s.rva, s.what, GetLastError());
                continue;
            }
            memcpy(p, s.repl, s.size);
            VirtualProtect(p, s.size, old, &old);
            FlushInstructionCache(GetCurrentProcess(), p, s.size);
            Log("  %05X %s: abgeschaltet", (unsigned)s.rva, s.what);
            changed++;
        }
        return changed;
    }

    // Rueckhalt: sabotierte Engine-Bytes (0x5D) wieder herstellen. Rueckgabe: Anzahl.
    int RepairSabotage(bool quiet) {
        int fixed = 0;
        for (int i = 0; i < (int)(sizeof(SabotageBytes) / sizeof(SabotageBytes[0])); i++) {
            const EpilogByte& e = SabotageBytes[i];
            BYTE* p = (BYTE*)(UINT_PTR)e.va;
            if (IsBadReadPtr(p, 1) || *p == e.orig)
                continue;
            if (*p != 0x5D) {
                if (!quiet)
                    Log("Engine-Byte %08X: %02X (erwartet %02X oder 5D) - nicht angefasst", (unsigned)e.va, *p, e.orig);
                continue;
            }
            DWORD old = 0;
            if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) {
                if (!quiet)
                    Log("Engine-Byte %08X: VirtualProtect fehlgeschlagen (Fehler %lu)", (unsigned)e.va, GetLastError());
                continue;
            }
            *p = e.orig;
            VirtualProtect(p, 1, old, &old);
            FlushInstructionCache(GetCurrentProcess(), p, 1);
            fixed++;
            Log("Sabotage: Engine-Byte %08X 5D -> %02X wiederhergestellt", (unsigned)e.va, e.orig);
        }
        return fixed;
    }

    // Prueft die fuenf Engine-Bytes: die erste Minute oft, danach selten. Nur noetig, wenn die Muster oben nicht
    // passten (andere Othello-Fassung) - sonst sabotiert Othello gar nicht erst.
    DWORD WINAPI WatchProc(LPVOID) {
        for (int round = 0; !WatchStop; round++) {
            Sleep(round < 120 ? 500 : 5000);
            if (WatchStop)
                break;
            RepairSabotage(true);
        }
        return 0;
    }

    void StartWatch() {
        if (WatchThread)
            return;
        WatchThread = CreateThread(NULL, 0, WatchProc, NULL, 0, NULL);
        if (!WatchThread)
            Log("Rueckhalt-Faden nicht gestartet (Fehler %lu)", GetLastError());
    }

    void UnlockApply(BYTE* base, DWORD imageSize, const char* how) {
        if (InterlockedCompareExchange(&UnlockDone, 1, 0) != 0 || !base)
            return;
        LogStartFile();   // ab jetzt steht fest: das hier ist Othello
        Log("Othello erkannt (%s)", how);
        if (!imageSize)
            imageSize = ImageSizeOf(base);
        const int changed = ApplySites(base, imageSize);
        RepairSabotage(false);
        StartWatch();
        Log(changed > 0 ? "Fertig: Othello startet jetzt auch mit fremden Plugins."
            : "Fertig: nichts geaendert (siehe oben) - Othello kann fremde Plugins weiter ablehnen.");
    }

    // --- ntdll: LdrRegisterDllNotification (ab Windows Vista, nicht dokumentiert, Aufbau wie in ntdll) -------------
    struct UStr {
        USHORT Length, MaximumLength;
        PWSTR Buffer;
    };
    struct NotifyData {
        ULONG Flags;
        const UStr* FullDllName;
        const UStr* BaseDllName;
        PVOID DllBase;
        ULONG SizeOfImage;
    };
    typedef VOID(NTAPI* NotifyFn)(ULONG reason, const NotifyData* data, PVOID ctx);
    typedef LONG(NTAPI* RegisterFn)(ULONG flags, NotifyFn fn, PVOID ctx, PVOID* cookie);
    typedef LONG(NTAPI* UnregisterFn)(PVOID cookie);

    VOID NTAPI OnDllLoaded(ULONG reason, const NotifyData* data, PVOID) {
        if (reason != 1 || !data || !data->BaseDllName || !data->BaseDllName->Buffer || UnlockDone)
            return;   // 1 = geladen
        wchar_t name[64] = { 0 };
        const size_t n = data->BaseDllName->Length / sizeof(wchar_t);
        if (n == 0 || n >= 64)
            return;
        memcpy(name, data->BaseDllName->Buffer, n * sizeof(wchar_t));
        if (_wcsicmp(name, L"OTHELLO_ABI.DLL") == 0)
            UnlockApply((BYTE*)data->DllBase, data->SizeOfImage, "beim Laden von OTHELLO_ABI.DLL");
    }

    void Install() {
        if (HMODULE m = GetModuleHandleW(L"OTHELLO_ABI.DLL")) {
            UnlockApply((BYTE*)m, ImageSizeOf((BYTE*)m), "OTHELLO_ABI.DLL war schon geladen");
            return;
        }
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        RegisterFn reg = ntdll ? (RegisterFn)GetProcAddress(ntdll, "LdrRegisterDllNotification") : NULL;
        if (!reg || reg(0, OnDllLoaded, NULL, &NotifyCookie) != 0) {
            NotifyCookie = NULL;
            Log("LdrRegisterDllNotification nicht verfuegbar - Sperre nicht abgeschaltet");
        }
    }

    void Uninstall() {
        WatchStop = 1;
        if (NotifyCookie) {
            HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            if (UnregisterFn unreg = ntdll ? (UnregisterFn)GetProcAddress(ntdll, "LdrUnregisterDllNotification") : NULL)
                unreg(NotifyCookie);
            NotifyCookie = NULL;
        }
        if (WatchThread) {
            CloseHandle(WatchThread);   // nicht warten: DllMain haelt die Ladersperre
            WatchThread = NULL;
        }
        if (LogReady) {
            DeleteCriticalSection(&LogLock);
            LogReady = false;
        }
    }
}

#ifdef OMU_TEST
// Fuer den Test (Test\unlock_test.cpp bindet diese Datei ein): dieselben Funktionen ohne DllMain
extern "C" int OmuTestApply(unsigned char* base, unsigned long imageSize) { return ApplySites(base, imageSize); }
extern "C" const char* OmuTestLog() { return LogBuffer; }
extern "C" void OmuTestReset() { LogBuffer[0] = 0; UnlockDone = 0; }
#else
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        SelfModule = hinst;
        DisableThreadLibraryCalls(hinst);
        InitializeCriticalSection(&LogLock);
        LogReady = true;
        Install();   // vor HandledWinMain +0x206, wo Othello die geladenen Plugins prueft
    }
    else if (reason == DLL_PROCESS_DETACH) {
        Uninstall();
    }
    return TRUE;
}
#endif
