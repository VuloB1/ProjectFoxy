# Contributing to Project Foxy

*(Español más abajo / Spanish below.)*

Thanks for wanting to help! The rules of the game, in a few lines.

## First of all

- **A bug or an idea**: open an [*issue*](../../issues/new/choose) (there are templates). For a bug, tell me what you
  did, with which file and, if you can, a screenshot. If the problem depends on a particular image, attach it.
  English or Spanish are both fine.
- **A big change** (a new feature, touching the architecture): open an *issue* first and we talk it through, so no work
  is lost.

## Build and test

Everything is in the [README](README.md#build). In short: Windows, Visual Studio 2022 (or Build Tools), CMake, Ninja,
Qt **6.7.3** and vcpkg.

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release        # they all have to pass
```

`tools\Build-Release.ps1` does all of that and also builds the installer and the portable `.zip`.

## Code style

- C++20, `src/core` has **no** QML dependencies (it is tested in isolation); the QML layer only talks to `src/app`.
- Imitate what is around: names, comments (in English in the code, explaining the *why*), level of detail.
- Every new feature comes with tests (`tests/`, QtTest). A fixed bug comes with a test that used to fail.
- Image effects measure distances as a **percentage** of the picture (never pixels), so the preview and the final
  result match.
- Before proposing anything that changes dependencies, read [`docs/LICENCIAS.md`](docs/LICENCIAS.md): the project is
  GPL-3.0-or-later and every new library has to be compatible.

## Texts and translations

The program's texts are written in **Spanish** (Rioplatense *voseo*: «elegí», «tocá», «arrastrá») and the Spanish text
is the key of the translations: `qsTr("…")` in QML, `core::tr("…")` in C++. The translations (English, Portuguese,
Korean, Chinese, Japanese) are in [`i18n/strings.tsv`](i18n/strings.tsv), one row per text.

```bash
python tools/i18n_tool.py check    # texts missing in the table, placeholders (%1) that do not match
python tools/i18n_tool.py build    # regenerates resources/i18n/*.json (commit them)
```

Fixing a translation or adding a language is a great first contribution.

## One change, one pull request

- A branch from `main`, small and focused changes, a commit message that says **what** and **why**.
- `ctest` must pass and the interface you touched must look right in the light and dark themes.
- The technical documentation ([`docs/DESARROLLO.md`](docs/DESARROLLO.md)) is kept up to date with what you change.

## License of what you contribute

By sending a change you agree that it is published under the project's license, **GPL-3.0-or-later**.

---

# Cómo contribuir (Español)

¡Gracias por querer ayudar! Estas son las reglas del juego, en pocas líneas.

## Antes de nada

- **Un error o una idea**: abre un [*issue*](../../issues/new/choose) (hay plantillas). Si es un error, cuéntame qué hiciste,
  con qué archivo y, si puedes, una captura. Si el problema depende de una imagen concreta, adjúntala.
- **Un cambio grande** (una función nueva, tocar la arquitectura): abre primero un *issue* y lo hablamos, para no
  perder trabajo.

## Compilar y probar

Todo está en el [README](README.md#compilar). En corto: Windows, Visual Studio 2022 (o Build Tools), CMake, Ninja,
Qt **6.7.3** y vcpkg.

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release        # tienen que pasar todas
```

`tools\Build-Release.ps1` hace todo lo anterior y además arma el instalador y el `.zip` portable.

## Estilo del código

- C++20, `src/core` **sin** dependencias de QML (se prueba de forma aislada); la capa QML solo habla con `src/app`.
- Imita lo que hay alrededor: nombres, comentarios (en inglés en el código, explicando el *porqué*), nivel de detalle.
- Los textos de la interfaz se escriben **en español** (voseo rioplatense: «elegí», «tocá», «arrastrá») dentro de `qsTr(...)` /
  `core::tr(...)`, y después se traducen en [`i18n/strings.tsv`](i18n/strings.tsv) (ver `python tools/i18n_tool.py check`).
- Toda función nueva lleva pruebas (`tests/`, QtTest). Un error corregido lleva una prueba que antes fallaba.
- Los efectos de imagen miden las distancias como **porcentaje** de la imagen (nunca píxeles), para que la vista previa y
  el resultado final coincidan.
- Antes de proponer algo que cambie dependencias, lee [`docs/LICENCIAS.md`](docs/LICENCIAS.md): el proyecto es
  GPL-3.0-or-later y cada librería nueva tiene que ser compatible.

## Un cambio, un *pull request*

- Rama desde `main`, cambios pequeños y enfocados, mensaje de *commit* que diga **qué** y **por qué**.
- Que `ctest` pase y que la interfaz que tocaste se vea bien en los temas claro y oscuro.
- La documentación técnica ([`docs/DESARROLLO.md`](docs/DESARROLLO.md)) se mantiene al día con lo que cambias.

## Licencia de lo que aportes

Al enviar un cambio aceptas que se publique bajo la misma licencia del proyecto, **GPL-3.0-or-later**.
