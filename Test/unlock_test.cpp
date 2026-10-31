// unlock_test.cpp - prueft den echten Patch-Code an einem Speicherabbild von OTHELLO_ABI.DLL, ohne Gothic.
//
// Werkzeuge\abbild.py legt die DLL wie der Lader auf eine Basis (Relocations angewendet) und schreibt sie als
// "<4 Byte Basis><Abbild>". Dieser Test reserviert genau diese Basis, kopiert das Abbild hinein und laesst
// ApplySites() aus Quelltext\OthelloFixMods.cpp darauf laufen - dieselbe Funktion, die spaeter im Spiel arbeitet.
//
// Aufruf: unlock_test.exe <original.img> [gepatcht.img]
// Prueft: (1) Original - alle drei Stellen abgeschaltet, Sprungziele der beiden Spruenge unveraendert;
//         (2) dasselbe Abbild ein zweites Mal - "schon abgeschaltet", nichts geaendert;
//         (3) ein verfaelschtes Muster - nichts geaendert (andere Othello-Fassung bleibt unberuehrt);
//         (4) falls angegeben: das mit gepatchter VDF ausgelieferte Abbild - "schon abgeschaltet".

#define OFM_TEST
#include "..\Quelltext\OthelloFixMods.cpp"

#include <stdio.h>
#include <stdlib.h>

namespace {

    int Fehler = 0;
    int Gut = 0;

    void Pruefe(bool ok, const char* was, const char* zusatz = "") {
        printf("  [%s] %s%s%s\n", ok ? "ok" : "FEHLER", was, zusatz[0] ? " - " : "", zusatz);
        if (ok)
            Gut++;
        else
            Fehler++;
    }

    // Abbild laden und an seine Basis legen
    BYTE* AbbildLaden(const char* datei, DWORD* size) {
        FILE* f = fopen(datei, "rb");
        if (!f) {
            printf("  [FEHLER] %s nicht lesbar\n", datei);
            Fehler++;
            return NULL;
        }
        fseek(f, 0, SEEK_END);
        const long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        DWORD basis = 0;
        if (n < 5 || fread(&basis, 1, 4, f) != 4) {
            fclose(f);
            printf("  [FEHLER] %s zu klein\n", datei);
            Fehler++;
            return NULL;
        }
        const DWORD bytes = (DWORD)(n - 4);
        BYTE* mem = (BYTE*)VirtualAlloc((LPVOID)(UINT_PTR)basis, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!mem) {
            fclose(f);
            printf("  [FEHLER] Speicher bei %08X nicht frei (Fehler %lu) - Test nicht moeglich\n",
                (unsigned)basis, GetLastError());
            Fehler++;
            return NULL;
        }
        const size_t got = fread(mem, 1, bytes, f);
        fclose(f);
        if (got != bytes) {
            printf("  [FEHLER] %s nur zum Teil gelesen\n", datei);
            Fehler++;
            return NULL;
        }
        *size = bytes;
        return mem;
    }

    void AbbildFrei(BYTE* mem) {
        if (mem)
            VirtualFree(mem, 0, MEM_RELEASE);
    }

    // Sprungziel von "0F 85 rel32" (6 Byte) bzw. "E9 rel32 90" (6 Byte) - beide muessen dieselbe Stelle treffen
    DWORD SprungZiel(const BYTE* p, DWORD rva) {
        if (p[0] == 0x0F && p[1] == 0x85)
            return rva + 6 + *(const int*)(p + 2);
        if (p[0] == 0xE9)
            return rva + 5 + *(const int*)(p + 1);
        return 0xFFFFFFFF;
    }

    void TestOriginal(const char* datei) {
        printf("1. Original %s\n", datei);
        DWORD size = 0;
        BYTE* mem = AbbildLaden(datei, &size);
        if (!mem)
            return;

        DWORD zielVorher[3] = { 0, 0, 0 };
        for (int i = 0; i < 3; i++)
            zielVorher[i] = SprungZiel(mem + UnlockSites[i].rva, UnlockSites[i].rva);

        OfmTestReset();
        const int changed = OfmTestApply(mem, size);
        Pruefe(changed == 3, "alle drei Stellen abgeschaltet",
            changed == 3 ? "" : "nur ein Teil - siehe Protokoll unten");

        for (int i = 0; i < 3; i++) {
            const UnlockSite& s = UnlockSites[i];
            char was[200];
            _snprintf(was, sizeof(was), "%05X %s steht auf der Ersetzung", (unsigned)s.rva, s.what);
            was[sizeof(was) - 1] = 0;
            Pruefe(memcmp(mem + s.rva, s.repl, s.size) == 0, was);
        }

        for (int i = 1; i <= 2; i++) {   // die beiden Spruenge
            const DWORD nachher = SprungZiel(mem + UnlockSites[i].rva, UnlockSites[i].rva);
            char was[200], zusatz[100];
            _snprintf(was, sizeof(was), "%05X Sprungziel unveraendert", (unsigned)UnlockSites[i].rva);
            _snprintf(zusatz, sizeof(zusatz), "vorher %05X, nachher %05X", (unsigned)zielVorher[i], (unsigned)nachher);
            was[sizeof(was) - 1] = 0;
            zusatz[sizeof(zusatz) - 1] = 0;
            Pruefe(nachher == zielVorher[i] && nachher != 0xFFFFFFFF, was, zusatz);
        }

        printf("2. Dasselbe Abbild noch einmal (schon abgeschaltet)\n");
        OfmTestReset();
        const int again = OfmTestApply(mem, size);
        Pruefe(again == 0, "nichts mehr geaendert");
        Pruefe(strstr(OfmTestLog(), "schon abgeschaltet") != NULL, "Protokoll meldet 'schon abgeschaltet'");

        AbbildFrei(mem);
    }

    void TestFremdeFassung(const char* datei) {
        printf("3. Verfaelschtes Muster (andere Othello-Fassung)\n");
        DWORD size = 0;
        BYTE* mem = AbbildLaden(datei, &size);
        if (!mem)
            return;
        // ein Byte der Signatur jeder Stelle verbiegen
        for (int i = 0; i < 3; i++)
            mem[UnlockSites[i].sigRva] ^= 0xFF;
        OfmTestReset();
        const int changed = OfmTestApply(mem, size);
        Pruefe(changed == 0, "nichts geaendert");
        for (int i = 0; i < 3; i++) {
            const UnlockSite& s = UnlockSites[i];
            char was[200];
            _snprintf(was, sizeof(was), "%05X steht unveraendert auf dem Original", (unsigned)s.rva);
            was[sizeof(was) - 1] = 0;
            Pruefe(memcmp(mem + s.rva, s.expect, s.size) == 0, was);
        }
        Pruefe(strstr(OfmTestLog(), "Muster passt nicht") != NULL, "Protokoll meldet 'Muster passt nicht'");
        AbbildFrei(mem);
    }

    // Der alte VDF-Patch (whitelistpatch.py) aendert nur die beiden Whitelist-Abfragen - der Pruef-Verteiler mit der
    // Meldung "unsupported plugins" bleibt dort stehen. Diese DLL erledigt auch ihn; die zwei anderen Stellen laesst
    // sie in Ruhe.
    void TestGepatchteVdf(const char* datei) {
        printf("4. Abbild aus einer gepatchten VDF %s\n", datei);
        DWORD size = 0;
        BYTE* mem = AbbildLaden(datei, &size);
        if (!mem)
            return;
        OfmTestReset();
        const int changed = OfmTestApply(mem, size);
        Pruefe(changed == 1, "nur der Pruef-Verteiler wurde noch abgeschaltet");
        Pruefe(memcmp(mem + UnlockSites[0].rva, UnlockSites[0].repl, UnlockSites[0].size) == 0,
            "4C5F9 Pruef-Verteiler steht auf der Ersetzung");
        for (int i = 1; i <= 2; i++) {
            char was[200];
            _snprintf(was, sizeof(was), "%05X war schon abgeschaltet und blieb unveraendert", (unsigned)UnlockSites[i].rva);
            was[sizeof(was) - 1] = 0;
            Pruefe(memcmp(mem + UnlockSites[i].rva, UnlockSites[i].repl, UnlockSites[i].size) == 0, was);
        }
        Pruefe(strstr(OfmTestLog(), "schon abgeschaltet") != NULL, "Protokoll meldet 'schon abgeschaltet'");
        AbbildFrei(mem);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Aufruf: unlock_test.exe <original.img> [gepatcht.img]\n");
        return 2;
    }
    printf("Othello-Fix-Mods %s - Test des Patch-Codes\n\n", OFM_VERSION);
    TestOriginal(argv[1]);
    printf("\n");
    TestFremdeFassung(argv[1]);
    if (argc > 2) {
        printf("\n");
        TestGepatchteVdf(argv[2]);
    }
    printf("\nProtokoll des letzten Laufs:\n%s", OfmTestLog());
    printf("\nErgebnis: %d gut, %d Fehler\n", Gut, Fehler);
    return Fehler ? 1 : 0;
}
