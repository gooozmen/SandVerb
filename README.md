# SandVerb

[![Build](https://github.com/gooozmen/SandVerb/actions/workflows/build.yml/badge.svg)](https://github.com/gooozmen/SandVerb/actions/workflows/build.yml)

Аудио-плагин (VST3) на JUCE 8.0.4 для Windows, Linux и macOS. Реверб, рядом с ним петля из прошлых кадров (FEEDBACK) и параллельная ей «песочная» ветка SAND: педаль Drive и биткрашер. Любой блок можно включать и выключать, настройки блока открываются по нажатию на него в схеме сигнала.

## Установка

Скачайте файл для своей системы со страницы [Releases](https://github.com/gooozmen/SandVerb/releases/latest). Рядом с каждым файлом лежит контрольная сумма `.sha256`.

После установки перезапустите DAW или пересканируйте плагины.

### Windows

1. Скачайте `SandVerb-<версия>-Windows-Setup.exe` и запустите его. Нужны права администратора.
2. Установщик скопирует плагин в `C:\Program Files\Common Files\VST3\SandVerb.vst3`. Папку можно сменить на странице выбора.
3. Удаление: «Параметры → Приложения → SandVerb» или `C:\Program Files\SandVerb\Uninstall.exe`.

Установщик не подписан сертификатом, поэтому Windows SmartScreen может показать предупреждение. Нажмите «Подробнее → Выполнить в любом случае».

Вручную: распакуйте `SandVerb-<версия>-windows-x64-vst3.zip` и скопируйте папку `SandVerb.vst3` в `C:\Program Files\Common Files\VST3`.

### Linux

```sh
tar -xzf SandVerb-<версия>-linux-x86_64.tar.gz
cd SandVerb-<версия>-linux-x86_64
./install.sh                 # для текущего пользователя: ~/.vst3
./install.sh --system        # для всех пользователей: /usr/lib/vst3 (запросит sudo)
./install.sh --uninstall     # удалить
```

Плагин динамически использует стандартные библиотеки рабочего стола (ALSA, X11, FreeType, Fontconfig). Собран на Ubuntu 22.04, поэтому запускается на дистрибутивах с glibc 2.35 и новее.

### macOS

```sh
unzip SandVerb-<версия>-macos-universal.zip
cd SandVerb-<версия>-macos-universal
./install.sh                 # для текущего пользователя: ~/Library/Audio/Plug-Ins/VST3
./install.sh --system        # для всех пользователей: /Library/Audio/Plug-Ins/VST3 (запросит sudo)
./install.sh --uninstall     # удалить
```

Сборка универсальная (Apple Silicon и Intel), macOS 10.13 и новее. Плагин подписан ad-hoc и не нотаризован. Скрипт сам снимает атрибут карантина. Если копируете плагин вручную, выполните `xattr -dr com.apple.quarantine SandVerb.vst3`.

## Сборка из исходников

Требования: CMake 3.22+, компилятор с поддержкой C++17, интернет при первой конфигурации (JUCE скачивается через `FetchContent`).

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

На Windows: `cmake -B build -A x64`, затем `cmake --build build --config Release`.

Результат:
- VST3: `build/SandVerb_artefacts/Release/VST3/SandVerb.vst3`.
- Standalone: `build/SandVerb_artefacts/Release/Standalone/SandVerb`.

По умолчанию собранный VST3 копируется в системную папку плагинов (удобно при разработке). Отключить: `-DSANDVERB_COPY_PLUGIN=OFF`.

Зависимости для Linux (Ubuntu / Debian):

```sh
sudo apt-get install ninja-build libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
```

Идентификаторы плагина заданы в `CMakeLists.txt` (`PLUGIN_MANUFACTURER_CODE Yrnm`, `PLUGIN_CODE Sdvb`). Менять их после выпуска нельзя: хосты будут считать плагин другим.

### Пакеты для установки

- Linux и macOS: `scripts/package.sh linux` или `scripts/package.sh macos` после сборки. Архив с плагином, `install.sh` и `INSTALL.txt` появится в `dist/`.
- Windows: `makensis /DSRC=<путь к SandVerb.vst3> /DVERSION=<версия> installers\windows\SandVerb.nsi` (нужен [NSIS](https://nsis.sourceforge.io/)).

### Выпуск релиза

Workflow `.github/workflows/build.yml` собирает плагин на Linux, Windows и macOS при каждом push и pull request. По тегу `v*` он ещё и публикует все пакеты в Releases:

```sh
git tag v0.1.0
git push origin v0.1.0
```

Версия в именах файлов берётся из `project(SandVerb VERSION ...)` в `CMakeLists.txt`, поэтому меняйте её вместе с тегом.

## Структура

```
.github/workflows/    сборка на трёх системах и публикация релиза
installers/           install.sh (Linux, macOS), windows/SandVerb.nsi (установщик Windows)
scripts/              package.sh: архив релиза для Linux и macOS
Source/
  PluginProcessor.*   APVTS с параметрами, перенос параметров в ядро, состояние (save/restore)
  PluginEditor.*      окно: схема сверху, настройки выбранного блока снизу
  FlowGraph.*         интерактивная схема сигнала: выбор блока, светодиоды включателей, уровни на проводах
  Inspector.*         страницы настроек по блокам (секции со своими включателями)
  Widgets.h           цвета, список блоков (Node), тема JUCE
  SandVerbCore.h      граф сигнала: хранилище, SAND, компрессоры, реверб, модули, включатели
  FeedbackStore.h     кольцевой буфер до 32 с, чтение взвешенной суммы отводов на кратных D
  Sand.h              SAND = Drive -> Crusher, у каждого свой включатель
  Drive.h             педаль: Overdrive / Tube / Fuzz, Tone, Level, компенсация громкости
  Crusher.h           биткрашер: Bits, Rate, Companding, Noise, полки, Mix
  DspUtil.h           XorShift, Biquad (ФНЧ, ФВЧ, полки)
  WarmVerb.h          реверб: 8-линейный FDN, матрица Хаусхолдера, два голоса (Fat, Tape)
  Compressor.h        компрессор прямой связи, стерео-линкованный, мягкое колено
LICENSE               GNU AGPL-3.0
```

## Сигнал

```
IN ─┬───────────────────────────────────────────────────────► dry ──┐
    │                                                                │
    └─► REVERB ─┬──────────────────────────────────────────► (Mix) ─┤
                │                                                    │
                └─► COMP1 ─┬─► FEEDBACK (≤ N кадров, модули) ─┐      ├─► OUT
                           │          ▲                       ▼      │
                           └─► SAND (DRIVE → CRUSHER) ──► COMP2 ─────┤ (Fb Out)
                                      │                     │
                                      └────── SEND ◄────────┘
```

На каждый сэмпл:

1. **REVERB** получает только сухой вход. Его выход идёт в Mix и в COMP1.
2. **COMP1** сжимает выход реверба. Дальше сигнал идёт в две параллельные ветки.
3. **FEEDBACK** читает прошлые кадры: отводы на 1·D, 2·D, … N·D назад, где D — длина кадра, а N — глубина (дробная, до 16). Вес отвода `k` равен `falloff^k`.
4. **SAND** красит выход COMP1: Drive, затем Crusher. Ветка стоит вне петли: через SAND проходит только то, что идёт от реверба, а повторы заново не крашатся.
5. **COMP2** сжимает сумму `Feedback + Sand·Sand Level`, затем `tanh`. Результат идёт на OUT (Fb Out, мимо Mix) и в SEND.
6. **SEND** возвращает сигнал COMP2 в хранилище: запись = `tanh(выход COMP1 + Send·COMP2)`. Хранилище пишется всегда.
7. **OUT** = `(1 − Mix)·dry + Mix·reverb + FbOut·COMP2`.

## Параметры

Диапазоны и значения по умолчанию заданы в `PluginProcessor.cpp` (`createLayout`).

**FEEDBACK**
- `Feedback On` — включает ветку повторов.
- `Frame` — длина кадра, 5–2000 мс, по умолчанию 250 мс. Недоступна при Tempo Sync.
- `Tempo Sync` — длина кадра берётся из темпа хоста. `Division`: от 1/32 до 2 тактов. `Feel`: Straight, Dotted, Triplet. Без хоста используются 120 BPM и 4/4.
- `Depth` — глубина хранилища в кадрах, 1–16, по умолчанию 4.
- `Falloff` — вес каждого более старого кадра относительно предыдущего, 0–100 %, по умолчанию 65 %.

**SEND**
- `Send On`, `Send` — возврат COMP2 в FEEDBACK, 0–95 %, по умолчанию 60 %.
- `Fb Out` — COMP2 на выход, мимо Mix, 0–100 %, по умолчанию 55 %.

**REVERB**
- `Reverb On`, `Reverb Type` (Fat или Tape), `Decay` (RT60 средних, 0.2–10 с, по умолчанию 2.5 с), `Damping`, `Mix`.

**COMP 1 и COMP 2**
- `Comp 1 On`, `C1 Threshold` (−48…0 дБ, по умолчанию −18), `C1 Ratio` (1–20, по умолчанию 4).
- `Comp 2 On`, `C2 Threshold` (по умолчанию −12 дБ), `C2 Ratio`.

**SAND**
- `Sand On`, `Sand Level` — уровень ветки SAND в сумме перед COMP2, по умолчанию 70 %.

**DRIVE** (педаль подогрева)
- `Drive On`, `Drive Type`:
  - Overdrive: усиливается полоса выше ~720 Гц (горб на середине), мягкий `tanh`;
  - Tube: клип со смещением рабочей точки, чётные гармоники;
  - Fuzz: ФВЧ 100 Гц, большое усиление, почти жёсткий клип.
- `Drive` 0–40 дБ (по умолчанию 12), `Tone` (ФНЧ 1–12 кГц), `Drive Level` −24…+6 дБ.
- Громкость компенсируется автоматически (таблица, замер на розовом шуме): Drive меняет характер, а не уровень.

**CRUSHER** (биткрашер)
- `Crusher On`, `Bits` 1–16 (дробные допустимы, по умолчанию 8), `Rate` 200 Гц – 48 кГц (по умолчанию 11 кГц, логарифмическая шкала, выборка с дробной фазой).
- `Companding`: μ-law сжатие перед квантованием и неполное расширение после, поэтому тихое поднимается, а в нём больше шума квантования. Ноль даёт точный провод.
- `Noise`: шум, умноженный на огибающую входа (до −12 дБ). На тишине шума нет.
- `Low Shelf` (200 Гц), `High Shelf` (4 кГц), ±12 дБ. `Crusher Mix`, по умолчанию 100 %.

**MOD** (двигают глубину хранилища, до ±8 кадров)
- `LFO1` (синус), `LFO2` (треугольник), `Random`: Rate и Amount (−100…+100 %). `Envelope Amount` — громкость входа.

## Включатели

Включатели есть у Feedback, Send, Reverb, Comp 1, Comp 2, Sand, Drive и Crusher. Это обычные параметры: они автоматизируются в DAW, а в схеме показаны светодиодами на блоках. Переключение сглажено (~30 мс), без щелчков.

| Блок | В выключенном состоянии |
|---|---|
| Comp 1, Comp 2, Drive, Crusher | провод: сигнал проходит без обработки |
| Reverb | ветка COMP1 питается сухим входом, реверб на выходе исчезает, его линии очищаются |
| Feedback, Sand | ветка глушится |
| Send | возврата в хранилище нет: остаются повторы без рециркуляции |

Выключенный блок не считается, это экономит CPU.

## Интерфейс

Окно 1100×720. Сверху схема сигнала:
- нажатие на блок выбирает его, и снизу открываются его настройки (у SAND три секции: Drive, Crusher и Sand);
- нажатие на светодиод в углу блока включает или выключает блок;
- яркость и толщина проводов следуют за уровнем, выключенные блоки и их провода тускнеют;
- на блоках живые значения: подавление в компрессорах, число кадров и длина кадра, тип реверба, Bits и Rate;
- блок Mod показывает пунктиром, что он двигает глубину хранилища.

Выбранный блок хранится в состоянии проекта (свойство `uiNode`).

## Поведение

- **Смена длины кадра** — equal-power кроссфейд 40 мс между двумя точками чтения. Гистерезис 4 сэмпла защищает от дрожания при изменении темпа.
- **Смена типа реверба** — кроссфейд 120 мс. Новый реверб стартует с пустых линий.
- **Сглаживание** — параметры сглажены за ~30 мс. Тяжёлые вычисления идут на control-rate, раз в 16 сэмплов.
- **Защита от самовозбуждения:**
  - память ограничена 32 секундами и 16 кадрами;
  - петля идёт через COMP2 и `tanh`, запись в хранилище тоже через `tanh`;
  - `Send` не выше 95 %, плюс нормировка весов отводов, поэтому усиление петли не выше Send;
  - SAND вне петли, поэтому Drive и полки на устойчивость не влияют;
  - если на входе тишина дольше N кадров, петля плавно закрывается.
- **Максимальная длина кадра** — 2 с вручную. В Sync ограничивается буфером 32 с (например, 2 такта при 60 BPM = 8 с).
- **Tail** — 40 с (`getTailLengthSeconds`), чтобы хост не обрезал хвост.

## Состояние

Состояние сохраняется через APVTS (`getStateInformation` / `setStateInformation`). ID параметров задаются в `PluginProcessor.cpp`: `kParamIds` и `createLayout`. Если переименовать или удалить параметр, старые пресеты перестанут загружаться. Для такой миграции нужна версия состояния. Пресеты версий до схемы Drive/Crusher несовместимы с текущими.

## Известные ограничения

- Окно фиксированного размера 1100×720.
- Компрессоры вычисляют `log10` и `pow` на каждом сэмпле.
- У Drive нет оверсэмплинга: алиасинг здесь часть звука, а после педали всё равно стоит крашер.

## Лицензия

Copyright (C) 2026 gmuse.

SandVerb распространяется на условиях [GNU Affero General Public License v3.0](LICENSE). Исходный код открыт. Плагин собран на фреймворке [JUCE](https://juce.com/), который тоже доступен по AGPLv3.
