# Cómo contribuir a Project Foxy

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
- Los textos de la interfaz van **en español** (voseo rioplatense: «elegí», «tocá», «arrastrá»).
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
