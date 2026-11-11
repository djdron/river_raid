# River Raid auf der Xbox (UWP)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | Deutsch | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Das Spiel (ohne den Betrachter) als UWP-App: Es läuft unter Windows 10/11 und auf einer Xbox One oder Series im Dev Mode. Es wird mit SDL2 gebaut, da SDL3 UWP nicht mehr unterstützt. **Noch nicht ausprobiert**: Das wurde ohne Windows zur Hand geschrieben; wenn ein Schritt nicht wie beschrieben funktioniert, melden Sie bitte, was passiert ist.

## Was man braucht
- Windows 10/11 mit Visual Studio 2022: die Workloads „Desktopentwicklung mit C++“ und „Entwicklung für die universelle Windows-Plattform“ (mit den C++-UWP-Tools, v143).
- CMake 3.16 oder neuer (das aus Visual Studio genügt) und git: SDL2 wird mit dem Spiel heruntergeladen und gebaut.
- Für die Xbox: ein Entwicklerkonto und die Konsole im Dev Mode (die App „Dev Mode Activation“ auf der Xbox).
- Ihr Abbild des Moduls als `rom/River_Raid.rom` (8192 Bytes): Es kommt **in das Paket**, das Paket ist also nur für Sie; geben Sie es nicht weiter.

## Bauen
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Öffnen Sie `build-uwp/river_raid_port.sln`, wählen Sie Release und x64 und machen Sie `river_raid` zum Startprojekt. Starten Sie es zuerst auf dem lokalen Computer (Windows): Das Fenster sollte das Spiel zeigen.

Das Paket wird mit dem Testzertifikat signiert, das CMake mitbringt (`CN=CMake Test Cert`). Ein eigenes: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (der Antragsteller des Zertifikats). Ein anderer ROM-Pfad: `-DRR_UWP_ROM=...`.

## Auf die Xbox
Beide Wege funktionieren:
- **Aus Visual Studio:** Debug-Ziel „Remotecomputer“, die IP-Adresse der Xbox (in Dev Home angezeigt), Authentifizierung „Universal (Unencrypted Protocol)“; koppeln mit der PIN, die Dev Home anzeigt.
- **Als Paket über das Device Portal:** in Visual Studio Projekt → Veröffentlichen → App-Pakete erstellen → Querladen, x64; dann im Browser auf dem PC `https://<xbox-ip>:11443` öffnen (Device Portal, in Dev Home einzuschalten), Add, und die `.msix` mit den Abhängigkeiten aus ihrem Ordner wählen (das VCLibs-Paket).

Danach in Dev Home die App auswählen, View details, und App type auf **Game** setzen: Eine App bekommt sonst weniger von der Konsole.

Das Gamepad funktioniert wie am PC (siehe [../README.de.md](../README.de.md)), X pausiert. Ohne Tastatur bleiben die übrigen Einstellungen bei ihren Voreinstellungen: PAL-Farben, Altirra, nur das Spielbild, scharf. B ist auch die Zurück-Taste des Systems; das Spiel fängt sie ab, damit sie die App nicht schließt. Die Xbox-Taste verlässt das Spiel.

## Was noch zu tun sein kann
- Der sichere Bereich des Fernsehers: UWP-Apps auf der Xbox werden eventuell mit einem Rand zu den Bildschirmkanten gezeichnet. Das Bild ist ohnehin von Schwarz umgeben; stört der Rand, kann die App den ganzen Bildschirm anfordern (`ApplicationView::SetDesiredBoundsMode`).
- Wie die Dateien zusammengesetzt sind (`uwp/uwp.cmake`, `uwp/Package.appxmanifest.in`): Die UWP-Projektunterstützung von CMake und der WinRT-Einstiegspunkt von SDL2 (`SDL_winrt_main_NonXAML.cpp`, als C++/CX kompiliert) sind wie dokumentiert verwendet, wurden hier aber nicht gebaut.
