# River Raid en la Xbox (UWP)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | Español | [中文](README.zh.md) | [日本語](README.ja.md)

El juego (sin el visor) como app UWP: funciona en Windows 10/11 y en una Xbox One o Series en Dev Mode. Se compila con SDL2, ya que SDL3 dejó de admitir UWP. **Aún sin probar**: se escribió sin Windows a mano; si algún paso no funciona como se describe, por favor cuente qué pasó.

## Qué hace falta
- Windows 10/11 con Visual Studio 2022: las cargas de trabajo «Desarrollo para el escritorio con C++» y «Desarrollo de la Plataforma universal de Windows» (con las herramientas de C++ para UWP, v143).
- CMake 3.16 o posterior (sirve el de Visual Studio) y git: SDL2 se descarga y se compila con el juego.
- Para la Xbox: una cuenta de desarrollador y la consola en Dev Mode (la app «Dev Mode Activation» en la Xbox).
- Su volcado del cartucho como `rom/River_Raid.rom` (8192 bytes): se mete **en el paquete**, así que el paquete es solo para usted; no lo comparta.

## Compilación
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Abra `build-uwp/river_raid_port.sln`, elija Release y x64 y ponga `river_raid` como proyecto de inicio. Ejecútelo primero en el equipo local (Windows): la ventana debería mostrar el juego.

El paquete se firma con el certificado de prueba que trae CMake (`CN=CMake Test Cert`). Uno propio: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (el sujeto del certificado). Otra ruta de la ROM: `-DRR_UWP_ROM=...`.

## A la Xbox
Sirve cualquiera de las dos formas:
- **Desde Visual Studio:** destino de depuración «Máquina remota», la dirección IP de la Xbox (la muestra Dev Home), autenticación «Universal (Unencrypted Protocol)»; emparejar con el PIN que muestra Dev Home.
- **Un paquete por Device Portal:** en Visual Studio Proyecto → Publicar → Crear paquetes de la aplicación → Instalación de prueba, x64; luego, en un navegador del PC, abra `https://<ip de la xbox>:11443` (Device Portal, se activa en Dev Home), Add, y elija el `.msix` con las dependencias de su carpeta (el paquete VCLibs).

Después, en Dev Home, seleccione la app, View details, y ponga App type en **Game**: si no, una app recibe menos de la consola.

El mando funciona como en el PC (ver [../README.es.md](../README.es.md)), X pausa. Sin teclado, los demás ajustes quedan en sus valores predeterminados: colores PAL, Altirra, solo la imagen del juego, nítido. B es también el botón Atrás del sistema; el juego lo intercepta, así que no cierra la app. El botón Xbox sale del juego.

## Qué puede quedar por hacer
- La zona segura del televisor: las apps UWP en la Xbox pueden dibujarse con un margen respecto a los bordes de la pantalla. La imagen ya está rodeada de negro; si el margen molesta, la app puede pedir toda la pantalla (`ApplicationView::SetDesiredBoundsMode`).
- Cómo se juntan los archivos (`uwp/uwp.cmake`, `uwp/Package.appxmanifest.in`): el soporte de CMake para proyectos UWP y el punto de entrada WinRT de SDL2 (`SDL_winrt_main_NonXAML.cpp`, compilado como C++/CX) se usan como dice la documentación, pero aquí no se han compilado.
