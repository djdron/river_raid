# River Raid на Xbox (UWP)

[English](README.md) | [Русский](README.ru.md) | Українська | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

Гра (без переглядача) як застосунок UWP: працює на Windows 10/11 і на Xbox One або Series у Dev Mode. Збирається на SDL2, бо SDL3 більше не підтримує UWP. Перевірено на Windows 11 і на Xbox Series S, зібрано у Visual Studio 2026.

## Що потрібно
- Windows 10/11 і Visual Studio 2022 або 2026 з інструментами C++ для UWP. У 2022: навантаження «Розробка класичних застосунків на C++» і «Розробка застосунків для універсальної платформи Windows» (з інструментами C++ для UWP). У 2026: «Розробка класичних застосунків на C++» і в навантаженні «Розробка застосунків WinUI» компонент «C++ Universal Windows Platform tools».
- CMake 3.16 або новіший (підійде той, що у Visual Studio) і git: SDL2 завантажується й збирається разом із грою.
- Для Xbox: обліковий запис розробника й консоль у Dev Mode (застосунок «Dev Mode Activation» на Xbox).
- Ваш дамп картриджа як `rom/River_Raid.rom` (8192 байти): він кладеться **в пакет**, тож пакет лише для вас; не поширюйте його.

## Збирання
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Для Visual Studio 2026 генератор `-G "Visual Studio 18 2026"` (CMake 4.2 або новіший, наприклад той, що йде з нею).

Відкрийте `build-uwp/river_raid_port.sln`, виберіть Release і x64 і зробіть `river_raid` проєктом запуску. Спершу запустіть на локальній машині (Windows): у вікні має з'явитися гра.

Пакет підписується тестовим сертифікатом, що йде з CMake (`CN=CMake Test Cert`). Свій: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (суб'єкт сертифіката). Інший шлях до ROM: `-DRR_UWP_ROM=...`.

## На Xbox
Підходить будь-який спосіб:
- **З Visual Studio:** ціль налагодження «Віддалений комп'ютер», IP-адреса Xbox (її показує Dev Home), автентифікація «Universal (Unencrypted Protocol)»; спарювання за PIN, який показує Dev Home.
- **Пакетом через Device Portal:** у Visual Studio Проєкт → Опублікувати → Створити пакети застосунку → Неопубліковане завантаження, x64; потім у браузері на ПК відкрийте `https://<ip xbox>:11443` (Device Portal, вмикається в Dev Home), Add і виберіть `.msix` разом із залежностями з його теки (пакет VCLibs).

Потім у Dev Home виберіть застосунок, View details і встановіть App type = **Game**: інакше застосунку дістається менше ресурсів консолі. На Xbox має бути виконано вхід в обліковий запис: інакше застосунок не запускається (Visual Studio: «Unable to activate Windows Store app … 0x8004090a»).

Геймпад працює так само, як на ПК (див. [../README.uk.md](../README.uk.md)), X — пауза. Клавіатури немає, тож решта налаштувань — за замовчуванням: кольори PAL, Altirra, лише картинка гри, чітке масштабування. B — це ще й системна кнопка «Назад»; гра її перехоплює, тож застосунок не закривається. Кнопка Xbox виходить із гри.
