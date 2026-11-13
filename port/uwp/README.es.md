# River Raid en la Xbox (UWP)

[English](README.md) | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | Español | [中文](README.zh.md) | [日本語](README.ja.md)

El juego (sin el visor) como app UWP: funciona en Windows 10/11 y en una Xbox One o Series en Dev Mode. Se compila con SDL2, ya que SDL3 dejó de admitir UWP. Probado en Windows 11 y en una Xbox Series S, compilado con Visual Studio 2026.

## Qué hace falta
- Windows 10/11 con Visual Studio 2022 o 2026 y las herramientas de C++ para UWP. En 2022: las cargas de trabajo «Desarrollo para el escritorio con C++» y «Desarrollo de la Plataforma universal de Windows» (con las herramientas de C++ para UWP). En 2026: «Desarrollo para el escritorio con C++» y, en la carga «Desarrollo de aplicaciones WinUI», el componente «C++ Universal Windows Platform tools».
- CMake 3.16 o posterior (sirve el de Visual Studio) y git: SDL2 se descarga y se compila con el juego.
- Para la Xbox: una cuenta de desarrollador y la consola en Dev Mode (la app «Dev Mode Activation» en la Xbox).
- Su volcado del cartucho como `rom/River_Raid.rom` (8192 bytes): se mete **en el paquete**, así que el paquete es solo para usted; no lo comparta.

## Compilación
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Con Visual Studio 2026 el generador es `-G "Visual Studio 18 2026"` (CMake 4.2 o posterior, como el que trae).

Abra `build-uwp/river_raid_port.sln`, elija Release y x64 y ponga `river_raid` como proyecto de inicio. Ejecútelo primero en el equipo local (Windows): la ventana debería mostrar el juego.

El paquete se firma con el certificado de prueba que trae CMake (`CN=CMake Test Cert`). Uno propio: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (el sujeto del certificado). Otra ruta de la ROM: `-DRR_UWP_ROM=...`.

## A la Xbox
Sirve cualquiera de las dos formas:
- **Desde Visual Studio:** destino de depuración «Máquina remota», la dirección IP de la Xbox (la muestra Dev Home), autenticación «Universal (Unencrypted Protocol)»; emparejar con el PIN que muestra Dev Home.
- **Un paquete por Device Portal:** en Visual Studio Proyecto → Publicar → Crear paquetes de la aplicación → Instalación de prueba, x64; luego, en un navegador del PC, abra `https://<ip de la xbox>:11443` (Device Portal, se activa en Dev Home), Add, y elija el `.msix` con las dependencias de su carpeta (el paquete VCLibs).

Después, en Dev Home, seleccione la app, View details, y ponga App type en **Game**: si no, una app recibe menos de la consola. En la Xbox tiene que haber alguien con la sesión iniciada; si no, la app no arranca (Visual Studio: «Unable to activate Windows Store app … 0x8004090a»).

El mando funciona como en el PC (ver [../README.es.md](../README.es.md)), X pausa. Sin teclado, los demás ajustes quedan en sus valores predeterminados: colores PAL, Altirra, solo la imagen del juego, escalado nítido (LT lo cambia). B es también el botón Atrás del sistema; el juego lo intercepta, así que no cierra la app. El botón Xbox sale del juego.

Si mantiene LT pulsado mientras arranca el juego, este funciona como una máquina PAL (como `-pal`: 50 fotogramas por segundo, un 17 % más lento) y pone el televisor a 50 Hz, si la consola lo permite (Settings → TV & display options → Video modes → Allow 50 Hz), y lo devuelve como estaba al salir. Sin 50 Hz funciona igualmente como PAL, solo que sin ir al ritmo del televisor.

Las opciones como en el PC (ver [../README.es.md](../README.es.md)) también se pueden dar al arrancar: en Dev Home seleccione el juego, Launch with parameters, marque Use custom protocol e introduzca, por ejemplo, `riverraid:-pal -bridge 500`. La app declara el protocolo `riverraid`; lo que sigue es la línea de órdenes.
