# Сборка UNO echo из исходников

Нужны CMake, Ninja, Visual Studio C++ Build Tools с Windows SDK и **Qt 6 для MSVC x64**:
модули Widgets, Network, StateMachine, Svg и Test. QHotkey включён в исходники.
Текущая сборка проверена с Qt 6.11.1; другие минорные версии отдельно не проверялись.

Из x64 Developer Command Prompt:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.x\msvc2022_64" -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --parallel
```

Замените пример пути на свою установку Qt. Либо задайте `ECHO_QT` и запустите
`tools\build-echo.cmd`: он найдёт Visual Studio через `vswhere`. Если Ninja/CMake
не в PATH, задайте `ECHO_NINJA` / `ECHO_CMAKE`. Дерево старого Crow Translate
для сборки не требуется. Результат: `build\UNO echo.exe`.

Проверка движка перевода на местном HTTP-сервере (без внешнего сервиса
и без работы с буфером обмена), из того же окружения:

```bat
set "PATH=C:\Qt\6.x\msvc2022_64\bin;%PATH%"
set "QT_PLUGIN_PATH=C:\Qt\6.x\msvc2022_64\plugins"
ctest --test-dir build -R "^translator$" --output-on-failure
```

Отдельный тест захвата: `ctest --test-dir build -R "^selection$" --output-on-failure`.
Он на несколько секунд активирует своё текстовое окно, использует буфер
обмена и затем восстанавливает его содержимое. Не вводите текст в других
приложениях во время проверки. В нашей среде автоматический захват пока не
проходит; реальный захват, перевод и вставка проверялись вручную.
Эти тесты не проверяют доступность внешнего сервиса перевода.

## Создание portable-пакета

Нужен Python 3 с пакетом `pefile`:

```bat
python -m pip install pefile
python tools\package-echo.py --qt-dir "C:\Qt\6.x\msvc2022_64"
```

Скрипт собирает приложение, Qt-плагины, найденные DLL и лицензии в
`dist\UNO echo`, затем создаёт ZIP рядом. Для DLL Visual C++ используется
установка Build Tools; можно передать `--runtime-dir` явно. Он не загружает
зависимости из интернета и сообщает о ненайденной DLL. Для повторной упаковки
укажите новый пустой каталог через `--output`, чтобы не смешивать разные сборки.

`source.zip` внутри пакета содержит соответствующие исходники приложения,
QHotkey, ресурсы и инструкции сборки. SDK Qt, компилятор, настройки пользователя,
рабочие архивы и Git-история в него не входят.

Изменять иконку необязательно. Для её перерисовки из `data/echo/echo.svg`
используется `tools/render-icon.py`; ему нужен Python с PySide6.
