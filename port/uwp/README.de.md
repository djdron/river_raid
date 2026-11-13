# River Raid auf der Xbox (UWP)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | Deutsch | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Das Spiel (ohne den Betrachter) als UWP-App: Es läuft unter Windows 10/11 und auf einer Xbox One oder Series im Dev Mode. Es wird mit SDL2 gebaut, da SDL3 UWP nicht mehr unterstützt. Ausprobiert unter Windows 11 und auf einer Xbox Series S, gebaut mit Visual Studio 2026.

## Was man braucht
- Windows 10/11 mit Visual Studio 2022 oder 2026 und den C++-Tools für UWP. In 2022: die Workloads „Desktopentwicklung mit C++“ und „Entwicklung für die universelle Windows-Plattform“ (mit den C++-UWP-Tools). In 2026: „Desktopentwicklung mit C++“ und im Workload „WinUI-Anwendungsentwicklung“ die Komponente „C++ Universal Windows Platform tools“.
- CMake 3.16 oder neuer (das aus Visual Studio genügt) und git: SDL2 wird mit dem Spiel heruntergeladen und gebaut.
- Für die Xbox: ein Entwicklerkonto und die Konsole im Dev Mode (die App „Dev Mode Activation“ auf der Xbox).
- Ihr Abbild des Moduls als `rom/River_Raid.rom` (8192 Bytes): Es kommt **in das Paket**, das Paket ist also nur für Sie; geben Sie es nicht weiter.

## Bauen
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Mit Visual Studio 2026 ist der Generator `-G "Visual Studio 18 2026"` (CMake 4.2 oder neuer, etwa das mitgelieferte).

Öffnen Sie `build-uwp/river_raid_port.sln`, wählen Sie Release und x64 und machen Sie `river_raid` zum Startprojekt. Starten Sie es zuerst auf dem lokalen Computer (Windows): Das Fenster sollte das Spiel zeigen.

Das Paket wird mit dem Testzertifikat signiert, das CMake mitbringt (`CN=CMake Test Cert`). Ein eigenes: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (der Antragsteller des Zertifikats). Ein anderer ROM-Pfad: `-DRR_UWP_ROM=...`.

## Auf die Xbox
Beide Wege funktionieren:
- **Aus Visual Studio:** Debug-Ziel „Remotecomputer“, die IP-Adresse der Xbox (in Dev Home angezeigt), Authentifizierung „Universal (Unencrypted Protocol)“; koppeln mit der PIN, die Dev Home anzeigt.
- **Als Paket über das Device Portal:** in Visual Studio Projekt → Veröffentlichen → App-Pakete erstellen → Querladen, x64; dann im Browser auf dem PC `https://<xbox-ip>:11443` öffnen (Device Portal, in Dev Home einzuschalten), Add, und die `.msix` mit den Abhängigkeiten aus ihrem Ordner wählen (das VCLibs-Paket).

Danach in Dev Home die App auswählen, View details, und App type auf **Game** setzen: Eine App bekommt sonst weniger von der Konsole. Auf der Xbox muss jemand angemeldet sein, sonst startet die App nicht (Visual Studio: „Unable to activate Windows Store app … 0x8004090a“).

Das Gamepad funktioniert wie am PC (siehe [../README.de.md](../README.de.md)), X pausiert. Ohne Tastatur bleiben die übrigen Einstellungen bei ihren Voreinstellungen: PAL-Farben, Altirra, nur das Spielbild, scharfe Skalierung (LT schaltet sie um). B ist auch die Zurück-Taste des Systems; das Spiel fängt sie ab, damit sie die App nicht schließt. Die Xbox-Taste verlässt das Spiel.

Hält man beim Start des Spiels LT gedrückt, läuft es als PAL-Gerät (wie `-pal`: 50 Bilder pro Sekunde, etwa 17 % langsamer) und schaltet den Fernseher auf 50 Hz, wenn die Konsole es erlaubt (Settings → TV & display options → Video modes → Allow 50 Hz), und am Ende zurück. Ohne 50 Hz läuft es trotzdem als PAL, nur nicht im Takt des Fernsehers.
