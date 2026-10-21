# Credits and licenses

UNO echo is a compact Windows translator developed by Arachna from Crow Translate.
It is an independently maintained derivative, not an official Crow Translate or KDE release.

## Crow Translate

The translation engine and selection code derive from Crow Translate by
Hennadii Chernyshchyk and Volk Milit. Original copyright notices remain in the
applicable files. Our compact interface, selection/insertion adaptations,
packaging and UNO styling are distributed with the application under
**GPL-3.0-or-later**. Changes to inherited files are identified in their headers.

Upstream: https://invent.kde.org/office/crow-translate

The license is provided in `LICENSE` and `LICENSES/GPL-3.0-or-later.txt`.

## Bundled source and artwork

- QHotkey 1.5.0 by Skycoder42: BSD-3-Clause. Source, upstream README and license
  are in `src/3rdparty/qhotkey`. Upstream: https://github.com/Skycoder42/QHotkey
- Inter font: SIL Open Font License 1.1; `data/echo/inter-LICENSE.txt`.
- Jura font: SIL Open Font License 1.1; `data/echo/jura-LICENSE.txt`.
- UNO echo icon and application-specific visual assets: Arachna, supplied under
  the project license unless an accompanying notice states otherwise.

## Binary distribution

Qt is dynamically linked. Its modules and their dependencies retain their own
licenses. The packaging script copies their installed copyright/license files
into the binary archive's `licenses` directory. Those notices, the application
license, and the corresponding application source archive must travel with the
portable release. The source archive does not contain a Qt SDK or compiler.
