# Othello-Fix-Mods

**A tiny DLL that lets you play the Gothic 1 mod [Othello](https://www.worldofgothic.de/) together with any other Union
plugins.** Out of the box, Othello 4.4.2.1 only starts with the plugins it knows. Any other plugin either makes it quit with
an error message or, worse, leads to silent crashes later in the game. This DLL switches off both locks in memory at startup.
**It does not modify any of Othello's files.**

> *Deutsche Fassung: [weiter unten auf dieser Seite](#deutsch).*

**Contents:** [The problem](#the-problem) · [Installation](#installation) · [How it works](#how-it-works) ·
[Log file](#log-file) · [Safety and limits](#safety-and-limits) · [Building](#building-it-yourself) · [Testing](#testing) ·
[Origin and license](#origin-and-license)

## The problem

Othello ships with Union and its own set of plugins. Add another Union plugin to `system\Autorun\` – a quality-of-life mod,
a renderer add-on, a modding tool or a co-op mod, for example – and one of two things happens:

1. **Othello quits immediately** with the message *"Remove the following unsupported plugins"*.
2. **Othello starts, but sabotages the engine:** it writes `0x5D` into the epilogue of five engine functions. The game runs
   fine at first, then crashes at seemingly random points – the kind of bug that is almost impossible to trace back to its
   cause.

With this DLL in your `Autorun` folder, both locks are gone and Othello runs with third-party plugins just like any other
mod.

**Do I need it?** Only if you want to play Othello with at least one plugin that Othello doesn't know. If you play Othello on
its own, you don't. If you use [Gothic-I-II-Universal-Coop](https://github.com/corshaegar/Gothic-I-II-Universal-Coop), you
don't need it either – the Coop DLL already contains the same unlock.

## Installation

1. [Download the latest release](../../releases) (`OthelloFixMods.dll`).
2. Copy the file to `<Othello folder>\system\Autorun\` – the folder where your other Union plugins live.
3. Launch Othello as usual.

To remove it, just delete the file. Nothing is left behind: no registry entries, no modified game files.

**Requirements:** Gothic 1 with Othello **4.4.2.1** and Union (comes with Othello), Windows Vista or later. Other Othello
versions: see [Safety and limits](#safety-and-limits).

## How it works

Othello hooks its checks into three engine calls, and it hides where those calls lead (the target address is computed as
`~ecx ^ edx`). The DLL disables the checks at three points inside `OTHELLO_ABI.DLL`. Every change is made **in memory**,
before Othello gets to check anything: in `DllMain` if `OTHELLO_ABI.DLL` is already loaded, otherwise via
`LdrRegisterDllNotification` the moment it gets loaded.

| Location in `OTHELLO_ABI.DLL` | Original code | Patched to |
|---|---|---|
| `+0x4C5F9` | `call eax` – a dispatcher randomly picks one of 100 identical check variants; an unknown plugin leads to `MessageBoxW` and `quick_exit` | `xor eax, eax` (`31 C0`) – takes out all 100 variants at once |
| `+0x553CD` | `jne` of the first whitelist check | `jmp` to the same target – the plugin counts as allowed |
| `+0x558B5` | `jne` of the second whitelist check | `jmp` to the same target |

Why `xor eax, eax` rather than a simple `nop nop`? The call site at `HandledWinMain+0x206` originally held Gothic's
`call [CreateMenu]`, and right afterwards the engine stores `eax` at `0x86F4DC` as a menu handle (used by `DeleteMenu` and
`DestroyMenu` on exit). `0` means "no menu" and is skipped. With `nop nop`, the engine would store the address of the check
variant there and later treat it as a menu handle.

**Safety net against the silent sabotage:** should a different Othello version still run its checks because the patterns
don't match, a background thread keeps an eye on the five affected engine bytes (`0x432EE1`, `0x5F36E4`, `0x5F7C2A`,
`0x64025A`, `0x640995` in `GothicMod.exe`) and puts the original value back as soon as a `0x5D` shows up – every 0.5 seconds
during the first minute, every 5 seconds after that.

## Log file

Once the DLL has detected Othello, it writes `system\Autorun\OthelloFixMods.log` next to itself (a fresh one on every
start) and records what it found and changed. The log is in German; a typical one looks like this:

```
Othello-Fix-Mods 1.0.0 - Start 2026-10-31 11:24:07
Othello erkannt (beim Laden von OTHELLO_ABI.DLL)
OTHELLO_ABI.DLL bei 70670000, 3448320 Byte gross
  4C5F9 Pruef-Verteiler (Meldung 'unsupported plugins', 100 Varianten): abgeschaltet
  553CD Sabotage-Pruefung 1: abgeschaltet
  558B5 Sabotage-Pruefung 2: abgeschaltet
Fertig: Othello startet jetzt auch mit fremden Plugins.
```

What the important lines mean:

| Log entry | Meaning |
|---|---|
| `Othello erkannt` | Othello detected – here while `OTHELLO_ABI.DLL` was being loaded |
| `abgeschaltet` | this check is now disabled |
| `schon abgeschaltet` | already disabled (for example by a patched VDF) and left as it is |
| `Muster passt nicht` | the code doesn't match – a different Othello version, nothing changed |
| `Sabotage: Engine-Byte … wiederhergestellt` | the safety net restored a sabotaged engine byte |
| `Fertig: Othello startet jetzt auch mit fremden Plugins.` | done – Othello now starts with other plugins, too |
| `Fertig: nichts geaendert` | done, but nothing was changed – Othello may still reject other plugins |

Without Othello, the DLL does nothing and creates no file.

## Safety and limits

- **Every location is checked on its own.** The DLL only patches a location if the instruction right before it, including
  its address operand as relocated by the loader, matches exactly. A different Othello version is left alone; the log then
  says `Muster passt nicht` (pattern doesn't match). Tested with **Othello 4.4.2.1**.
- **No file is ever modified** – neither the archives (`.vdf`) nor `OTHELLO_ABI.DLL` on disk. Older workarounds such as
  patching `ScriptsOthello4.0_de.vdf` directly are no longer needed. If your installation has already been patched that way,
  the DLL notices: it leaves the patched locations as they are (`schon abgeschaltet`) and only disables what is still active.
- **It leaves the game itself alone** – no changed values, no changed scripts, nothing else unlocked. All it does is stop
  Othello from refusing to run next to other plugins.
- **It doesn't check what other plugins do.** Whether a particular mod works well with Othello is up to you. Othello's lock
  was never a compatibility check anyway, just a list of known names.
- **Multiplayer/online:** the DLL doesn't unlock anything that involves other players.

## Building it yourself

The folder and parameter names in this repository are German: `Quelltext` is the source code, `Werkzeuge` the tools, `Bau`
the build output, and `Gepatcht` means patched.

You need the **Visual Studio Build Tools** with C++ (x86; tested with 2019, toolset v142). Then run:

```powershell
.\Werkzeuge\bauen.ps1
```

The result is `Bau\OthelloFixMods.dll` (32-bit, with a statically linked runtime, so the DLL doesn't depend on any `msvcr`
file in the game folder). The DLL is built from a single source file and only uses Windows API calls; the Union SDK is
**not** needed.

## Testing

The test runs exactly the code that later runs in the game against a memory image of the real `OTHELLO_ABI.DLL` – without
Gothic. `Werkzeuge\abbild.py` maps the DLL to its load address just like the Windows loader would (requires
[pefile](https://pypi.org/project/pefile/)); the test reserves exactly that address and checks every byte afterwards:

```powershell
.\Werkzeuge\bauen.ps1 -Test -AbiOriginal "<path>\OTHELLO_ABI.DLL"
```

It verifies that all three locations get disabled, that both jumps still lead to the same target, that a second run changes
nothing, and that a tampered pattern (standing in for a different version) is left untouched. If you also pass
`-AbiGepatcht` with a DLL extracted from a patched VDF, it checks that only the dispatcher still gets disabled.

In the game, `OTHELLO_ABI.DLL` lives in the hidden folder `Data\$Templates$\`; Union extracts it from the archive at
startup. The file belongs to Othello and is **not** included in this project.

## Origin and license

The unlock was written in 2026 as part of
[Gothic-I-II-Universal-Coop](https://github.com/corshaegar/Gothic-I-II-Universal-Coop) (as `OthelloUnlock.cpp`): being a
third-party Union plugin itself, the Coop couldn't start next to Othello without it. This project takes it out of the Coop
so it can be used on its own. The idea of patching in memory instead of on disk came from the mod `ModRestrictRemove`
(`zNonToxicUnfaker`), which was made for Othello 4.3.5 and no longer works with 4.4.2.1.

License: **AGPL-3.0-or-later** (see [LICENSE](LICENSE)) – the same as the Coop project the code comes from.

This mod is not affiliated with the authors of Othello, with Union or with Piranha Bytes.

---

# Deutsch

**Eine kleine DLL, mit der sich die Gothic-1-Mod [Othello](https://www.worldofgothic.de/) zusammen mit beliebigen anderen
Union-Plugins (Mods) spielen lässt.** Othello 4.4.2.1 startet von sich aus nur mit den Plugins, die es selbst kennt – jedes
fremde Plugin führt entweder zu einer Meldung und sofortigem Beenden oder zu stillen Abstürzen später im Spiel. Diese DLL
schaltet beide Sperren beim Start im Arbeitsspeicher ab. **An den Dateien von Othello ändert sie nichts.**

**Inhalt:** [Wofür](#wofür) · [Installation](#installation-1) · [Was passiert](#was-die-dll-genau-macht) ·
[Protokoll](#protokoll) · [Sicherheit](#sicherheit-und-grenzen) · [Selbst bauen](#selbst-bauen) · [Test](#test) ·
[Herkunft](#herkunft-und-lizenz)

## Wofür

Othello bringt Union mit und bringt eigene Plugins mit. Legt man ein weiteres Union-Plugin nach `system\Autorun\` – etwa
eine Quality-of-Life-Mod, einen Renderer-Aufsatz, ein Hilfsmittel oder eine Coop-Mod –, dann geschieht eines von beidem:

1. **Othello beendet sich sofort** mit der Meldung *„Remove the following unsupported plugins“*.
2. **Othello startet, sabotiert aber die Engine:** es schreibt `0x5D` in den Epilog von fünf Engine-Funktionen. Das Spiel
   läuft zunächst, stürzt dann aber an scheinbar zufälliger Stelle ab – ein Fehlerbild, das sich kaum zuordnen lässt.

Mit dieser DLL im `Autorun`-Ordner entfallen beide Sperren, und Othello läuft mit fremden Plugins wie jede andere Mod.

**Brauche ich sie?** Nur, wenn Sie Othello mit mindestens einem Plugin spielen wollen, das Othello nicht kennt. Wer Othello
pur spielt, braucht sie nicht. Wer den
[Gothic-I-II-Universal-Coop](https://github.com/corshaegar/Gothic-I-II-Universal-Coop) benutzt, braucht sie ebenfalls nicht –
dort steckt dieselbe Freischaltung schon in der Coop-DLL.

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
Thread die fünf betroffenen Engine-Bytes (`0x432EE1`, `0x5F36E4`, `0x5F7C2A`, `0x64025A`, `0x640995` in `GothicMod.exe`) und
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
  (ein Patch direkt in `ScriptsOthello4.0_de.vdf`) sind damit überflüssig. Ist eine Installation schon auf diesem Weg
  gepatcht, erkennt die DLL das: Die geänderten Stellen lässt sie, wie sie sind („schon abgeschaltet“), und schaltet nur
  ab, was noch aktiv ist.
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
Coop als fremdes Union-Plugin sonst nicht neben Othello starten konnte. Dieses Projekt löst sie heraus, damit sie auch ohne
den Coop nutzbar ist. Vorbild für den Weg „im Speicher statt in der Datei“ war die Mod `ModRestrictRemove`
(`zNonToxicUnfaker`), die für Othello 4.3.5 gebaut war und bei 4.4.2.1 nicht mehr greift.

Lizenz: **AGPL-3.0-or-later** (siehe [LICENSE](LICENSE)) – dieselbe wie beim Coop-Projekt, aus dem der Code stammt.

Diese Mod steht in keiner Verbindung zu den Autoren von Othello, zu Union oder zu Piranha Bytes.
