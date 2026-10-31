# Othello-Fix-Mods

**Eine kleine DLL, mit der sich die Gothic-1-Mod [Othello](https://www.worldofgothic.de/) zusammen mit beliebigen anderen
Union-Plugins (Mods) spielen lässt.** Othello 4.4.2.1 startet von sich aus nur mit den Plugins, die es selbst kennt – jedes
fremde Plugin führt entweder zu einer Meldung und sofortigem Beenden oder zu stillen Abstürzen später im Spiel. Diese DLL
schaltet beide Sperren beim Start im Arbeitsspeicher ab. **An den Dateien von Othello ändert sie nichts.**

> *English summary: a small Union plugin that lets the Gothic 1 total conversion "Othello" run together with any other Union
> plugins. Othello 4.4.2.1 refuses to start when it finds a plugin it does not know ("Remove the following unsupported
> plugins") and silently patches five engine functions so the game crashes later. This DLL disables both locks in memory at
> startup; Othello's own files stay untouched. Drop `OthelloFixMods.dll` into `<Othello>\system\Autorun\`. AGPL-3.0.*

**Inhalt:** [Wofür](#wofür) · [Installation](#installation) · [Was passiert](#was-die-dll-genau-macht) ·
[Protokoll](#protokoll) · [Sicherheit](#sicherheit-und-grenzen) · [Selbst bauen](#selbst-bauen) · [Test](#test) ·
[Herkunft](#herkunft-und-lizenz)

## Wofür

Othello bringt Union mit und bringt eigene Plugins mit. Legt man ein weiteres Union-Plugin nach `system\Autorun\` – etwa
eine Quality-of-Life-Mod, einen Renderer-Aufsatz, ein Hilfsmittel oder einen Koop –, dann geschieht eines von beidem:

1. **Othello beendet sich sofort** mit der Meldung *„Remove the following unsupported plugins“*.
2. **Othello startet, sabotiert aber die Engine:** es schreibt `0x5D` in den Epilog von fünf Engine-Funktionen. Das Spiel
   läuft zunächst, stürzt dann aber an scheinbar zufälliger Stelle ab – ein Fehlerbild, das sich kaum zuordnen lässt.

Mit dieser DLL im `Autorun`-Ordner entfallen beide Sperren, und Othello läuft mit fremden Plugins wie jede andere Mod.

**Brauche ich sie?** Nur, wenn Sie Othello mit mindestens einem Plugin spielen wollen, das Othello nicht kennt. Wer Othello
pur spielt, braucht sie nicht. Wer den
[Gothic-I-II-Universal-Coop](https://github.com/corshaegar/Gothic-I-II-Universal-Coop) benutzt, braucht sie ebenfalls nicht –
dort steckt dieselbe Freischaltung schon in der Koop-DLL.

## Installation

1. [Neueste Fassung herunterladen](../../releases) (`OthelloFixMods.dll`).
2. Die Datei nach `<Othello-Ordner>\system\Autorun\` legen – dorthin, wo auch die anderen Union-Plugins liegen.
3. Othello wie gewohnt starten.

Entfernen: Datei wieder löschen. Es bleibt nichts zurück – keine Einträge in der Registry, keine veränderte Spieldatei.

**Voraussetzungen:** Gothic 1 mit Othello **4.4.2.1** und Union (bringt Othello mit), Windows Vista oder neuer.
Andere Othello-Fassungen: siehe [Sicherheit und Grenzen](#sicherheit-und-grenzen).

## Was die DLL genau macht

Othello hängt seine Prüfung an drei Stellen in die Engine (die Zielangaben sind verschleiert: Adresse = `~ecx ^ edx`).
Alle Eingriffe geschehen **im Arbeitsspeicher**, bevor Othello prüft – in `DllMain`, falls `OTHELLO_ABI.DLL` schon geladen
ist, sonst über `LdrRegisterDllNotification`, sobald sie geladen wird.

| Stelle in `OTHELLO_ABI.DLL` | Was dort steht | Was die DLL daraus macht |
|---|---|---|
| `+0x4C5F9` | `call eax` – der Verteiler wählt per Zufall eine von 100 gleichen Prüf-Varianten; unbekanntes Plugin → `MessageBoxW` und `quick_exit` | `xor eax, eax` (`31 C0`) – trifft alle 100 Varianten auf einmal |
| `+0x553CD` | `jne` der ersten Whitelist-Abfrage | `jmp` auf dasselbe Ziel – das Plugin gilt als erlaubt |
| `+0x558B5` | `jne` der zweiten Whitelist-Abfrage | `jmp` auf dasselbe Ziel |

Warum `xor eax, eax` und nicht einfach `nop nop`: an der Aufrufstelle `HandledWinMain+0x206` stand ursprünglich Gothics
`call [CreateMenu]`, und die Engine legt `eax` gleich danach als Menü-Griff nach `0x86F4DC` ab (für `DeleteMenu` und
`DestroyMenu` beim Beenden). `0` heißt „kein Menü“ und wird übersprungen; mit `nop nop` stünde dort die Adresse der
Prüf-Variante.

**Rückhalt gegen die stille Sabotage:** Sollte eine abweichende Othello-Fassung die Prüfung doch durchlaufen, überwacht ein
Faden die fünf betroffenen Engine-Bytes (`0x432EE1`, `0x5F36E4`, `0x5F7C2A`, `0x64025A`, `0x640995` in `GothicMod.exe`) und
stellt sie wieder her, sobald dort `0x5D` auftaucht – die erste Minute alle 0,5 s, danach alle 5 s.

## Protokoll

Erkennt die DLL Othello, legt sie neben sich `system\Autorun\OthelloFixMods.log` an (bei jedem Start neu). Darin steht,
was gefunden und geändert wurde:

```
Othello-Fix-Mods 1.0.0 - Start 2026-10-31 11:24:07
Othello erkannt (beim Laden von OTHELLO_ABI.DLL)
OTHELLO_ABI.DLL bei 70670000, 3448320 Byte gross
  4C5F9 Pruef-Verteiler (Meldung 'unsupported plugins', 100 Varianten): abgeschaltet
  553CD Sabotage-Pruefung 1: abgeschaltet
  558B5 Sabotage-Pruefung 2: abgeschaltet
Fertig: Othello startet jetzt auch mit fremden Plugins.
```

Ohne Othello passiert nichts, und es entsteht keine Datei.

## Sicherheit und Grenzen

- **Jede Stelle wird einzeln geprüft.** Geändert wird nur, wenn der Befehl davor mitsamt seinem (vom Lader verschobenen)
  Adress-Operanden genau passt. Eine andere Othello-Fassung bleibt unberührt; im Protokoll steht dann „Muster passt nicht“.
  Getestet ist **Othello 4.4.2.1**.
- **Keine Datei wird verändert** – weder die Archive (`.vdf`) noch `OTHELLO_ABI.DLL` auf der Platte. Ältere Wege
  (ein Patch direkt in `ScriptsOthello4.0_de.vdf`) sind damit überflüssig. Eine bereits gepatchte Installation erkennt die
  DLL und lässt sie in Ruhe („schon abgeschaltet“).
- **Die DLL ändert nichts am Spiel selbst** – keine Werte, keine Skripte, keine Rechte. Sie nimmt Othello nur die Weigerung,
  neben anderen Plugins zu laufen.
- **Was andere Plugins tun, prüft sie nicht.** Ob eine fremde Mod mit Othello verträglich ist, bleibt Ihre Sache; Othellos
  Sperre war nie ein Verträglichkeits-Nachweis, sondern eine Liste bekannter Namen.
- **Mehrspieler/Online:** die DLL schaltet nichts frei, was mit anderen Spielern zu tun hat.

## Selbst bauen

Gebraucht werden die **Visual Studio Build Tools** mit C++ (x86; getestet mit 2019, Toolset v142). Dann:

```powershell
.\Werkzeuge\bauen.ps1
```

Ergebnis: `Bau\OthelloFixMods.dll` (32 Bit, statisch gebundene Laufzeit – die DLL hängt an keiner `msvcr`-Datei des
Spielordners). Die DLL besteht aus einer einzigen Quelldatei und benutzt nur Windows-Aufrufe; das Union-SDK wird **nicht**
gebraucht.

## Test

Der Test lässt genau den Code, der später im Spiel läuft, auf ein Speicherabbild der echten `OTHELLO_ABI.DLL` los – ohne
Gothic. `Werkzeuge\abbild.py` legt die DLL dafür wie der Lader auf ihre Ladeadresse (braucht
[pefile](https://pypi.org/project/pefile/)), der Test reserviert genau diese Adresse und prüft danach jedes Byte:

```powershell
.\Werkzeuge\bauen.ps1 -Test -AbiOriginal "<Pfad>\OTHELLO_ABI.DLL"
```

Geprüft wird: alle drei Stellen werden abgeschaltet, die Sprungziele der beiden Sprünge bleiben gleich, ein zweiter Lauf
ändert nichts mehr, ein verfälschtes Muster (andere Fassung) bleibt unberührt – und, wenn man mit `-AbiGepatcht` eine aus
einer gepatchten VDF entpackte DLL dazugibt, dass nur noch der Prüf-Verteiler abgeschaltet wird.

`OTHELLO_ABI.DLL` liegt im Spiel unter `Data\$Templates$\` (versteckt); Union entpackt sie beim Start aus dem Archiv.
Die Datei gehört Othello und liegt diesem Projekt **nicht** bei.

## Herkunft und Lizenz

Die Freischaltung entstand 2026 als Teil des
[Gothic-I-II-Universal-Coop](https://github.com/corshaegar/Gothic-I-II-Universal-Coop) (dort `OthelloUnlock.cpp`), weil der
Koop als fremdes Union-Plugin sonst nicht neben Othello starten konnte. Dieses Projekt löst sie heraus, damit sie auch ohne
Koop nutzbar ist. Vorbild für den Weg „im Speicher statt in der Datei“ war die Mod `ModRestrictRemove`
(`zNonToxicUnfaker`), die für Othello 4.3.5 gebaut war und bei 4.4.2.1 nicht mehr greift.

Lizenz: **AGPL-3.0-or-later** (siehe [LICENSE](LICENSE)) – dieselbe wie beim Koop-Projekt, aus dem der Code stammt.

Diese Mod steht in keiner Verbindung zu den Autoren von Othello, zu Union oder zu Piranha Bytes.
