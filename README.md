# arrange_tool

Консольная утилита для автоматического размещения STL-моделей на платформе 3D-принтера.

## Алгоритм

Используется алгоритм **Next Fit Shelf**.

Алгоритм не является оптимальным, но обеспечивает стабильный и предсказуемый результат.

## Сборка

### Требования

* Windows 10/11
* Visual Studio 2022
* CMake 3.20+
* vcpkg
* Git

### Установка зависимостей

```bash
vcpkg install --triplet x64-windows
```

Используемые библиотеки:

* Boost.Program_options
* Boost.Geometry
* polyclipping
* NLopt

### Сборка

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Исполняемый файл:

```text
build/bin/Release/arrange_tool.exe
```

## Использование

```bash
arrange_tool.exe --bed 200x200 --spacing 5 model1.stl model2.stl model3.stl
```

### Параметры

| Параметр          | Описание                                     |
| ----------------- | -------------------------------------------- |
| `--bed`, `-b`     | Размер платформы `ШИРИНАxВЫСОТА`, мм         |
| `--spacing`, `-s` | Зазор между моделями, мм. По умолчанию `1.0` |
| `--help`, `-h`    | Справка                                      |

STL-модели передаются после параметров командной строки.

### Статусы

* `PLACED:X,Y` — модель успешно размещена.
* `SKIPPED:insufficient space` — недостаточно места на платформе.
* `FILTERED:width W > bed B` — ширина модели превышает размер платформы.
* `FILTERED:height H > bed B` — высота модели превышает размер платформы.
* `FAILED:load error` — ошибка загрузки STL-файла.

## Лицензия

`arrange_tool` распространяется под **GNU Lesser General Public License v3.0 (LGPL-3.0)**.

Полный текст лицензии: [`LICENSE.md`](LICENSE.md).

Информация о сторонних библиотеках: [`NOTICE.md`](NOTICE.md).

## Автор

**Yatsenko Egor**

Email: `yatsenko1147@gmail.com`

Copyright © 2026 Yatsenko-Egor
