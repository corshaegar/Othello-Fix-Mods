# Changes

> *Deutsche Fassung: [CHANGELOG.de.md](CHANGELOG.de.md).*

## 1.0.0 (2026-10-31)

First release, taken out of [Gothic-I-II-Universal-Coop](https://github.com/corshaegar/Gothic-I-II-Universal-Coop), where the
same code (`OthelloUnlock.cpp`) has been in use since Coop version v79 (2026-10-21).

- Disables Othello's plugin lock in memory: the check dispatcher behind the message "Remove the following unsupported
  plugins" and the two whitelist checks that trigger the silent sabotage.
- A background thread restores sabotaged engine bytes in case the patterns of a different Othello version don't match.
- Standalone: Windows API calls only, no Union SDK, statically linked runtime.
- Writes the log file `OthelloFixMods.log` next to the DLL, but only when Othello is detected.
- Tested against a memory image of the real `OTHELLO_ABI.DLL` (18 checks).
