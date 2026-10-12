# Plan: terminar de dividir `AppController`

Estado: **opción B elegida** (11‑oct‑2026): fases 0–2; la fase 3 se decide después. Fase 0 en el PR que acompaña a este documento.

## 1. Dónde estamos

La revisión externa de la 0.2.1 dijo que `AppController` es un *God object* "todavía no desastroso". Se hicieron tres cortes, todos mover código sin cambiar comportamiento (PR #22, #23, #24):

| Salió | A dónde | Tests propios |
|---|---|---|
| Reproducción de animaciones | `AnimationPlayback` | 5 |
| Fondo de escritorio | `DesktopWallpaper` | (cubierto por el compilador y el CI; la lógica de escritorio no es testeable sin un escritorio) |
| Estado y cálculos de la vista previa de Efectos | `EffectPreview` | 10 |
| Guardado en segundo plano + solicitudes en espera | `SaveCoordinator` | 10 |

`AppController.cpp`: **1970 → 1434 líneas**; `AppController.h`: **613 → 551**. Esos cortes eran los separables: tenían estado propio y poca relación con el resto.

Lo que queda en `AppController` es lo que **comparte estado entre sí**. Seguir ya no es mover código: hay que decidir quién es dueño de cada dato. Por eso este plan.

## 2. Qué queda y cómo se enreda

Líneas de `AppController.cpp` por área (medidas sobre `main`, 4e4ed02):

| Área | Líneas aprox. | Estado que posee | Con quién se enreda |
|---|---|---|---|
| **Ajustes** (`adjustMap`, `setAdjustParam`, niveles, curvas, auto, LUT) | ~285 | `m_liveAdjust`, `m_lutRevision` | historial (cada cambio se registra), caché de histograma ajustado |
| **Filtros** (`lookMap`, listas, miniaturas, `applyFilterPreset`, LUT) | ~120 | `m_liveFilterPreset/Intensity`, `m_look*Revision`, `m_lookProxyDirty` | historial, `ImageProvider` |
| **Imágenes derivadas** (`structuralBaked`, histogramas, copia reducida, `currentBaked`, `republishBaked`, `publishPicture`) | ~170 | las 5 cachés `m_*Dirty` | historial, documento, `ImageProvider`, señales de QML |
| **Ediciones estructurales** (recortar, escalar, enderezar, girar, voltear) | ~80 | — (empujan al historial) | historial, cachés, vista previa de efectos |
| **Historial y sesión de herramienta** (`m_editStack`, deshacer/rehacer, `beginToolSession`/`endToolSession`, fusión por tiempo, receta guardada, `isDirty`) | ~210 | `m_editStack`, `m_toolSession*`, `m_liveCoalesce*`, `m_savedRecipe`, `m_loadStartRecipe` | **todo** |
| **Documento y carga** (`loadPath`, `applyNewDocument`, metadatos, series) | ~170 | `m_currentFilePath`, `m_originalImage`, `m_iccProfile`, `m_metadata`, `m_currentImageSize`, `m_docSerial`, `m_isLoading`, `m_loader` | guardado, historial, animación, efectos |
| **Cola de efectos / guardado** (pegamento hacia `EffectPreview` y `SaveCoordinator`) | ~230 | — | documento, historial |
| **Portapapeles, papelera** | ~135 | — | documento (borrar limpia el visor) |

### Los puntos calientes (lo que hace difícil esto)

1. **`m_liveAdjust` se toca en ~52 líneas** repartidas por casi todas las áreas (Ajustes, Filtros, historial, guardado, receta, efectos, reinicio). Es el estado "vivo" que la GPU aplica sobre la imagen. Moverlo mal rompe la paridad con lo que se guarda.
2. **`markStructuralDirty()`** es el interruptor central: suelta la vista previa del efecto, detiene la fusión de deshacer e invalida cinco cachés. Se llama desde 13 sitios.
3. **El orden de las señales importa para QML.** Ejemplo real de esta misma sesión: el proxy de pantalla exigió emitir `currentImageSizeChanged` *antes* de `currentSourceChanged`. Cualquier movimiento puede cambiar el orden sin que ningún test lo note.
4. **Invariantes ya arregladas en rondas anteriores** (`DESARROLLO.md` 4.10): una carga solo la publica la petición más reciente; el archivo cambia con los píxeles; no se guarda mientras otra imagen carga; lo editado durante una carga no se pierde; "sin guardar" compara contenido (`EditRecipe`); guardar confirma el efecto pendiente. El código que las cumple vive justo en las áreas que quedan.
5. **Hilos:** lo que ya salió tiene su pool y su espera en el destructor. Lo que quede no debe volver a capturar `this` en el pool global.

## 3. Reglas del juego (no negociables)

- **La fachada de QML no cambia.** `AppController` expone hoy 45 propiedades y ~48 slots; la interfaz usa **84 miembros distintos en 264 sitios**. Ningún nombre, tipo ni señal cambia, y no se expone ningún objeto nuevo a QML.
- **El orden de emisión de señales se conserva** en cada función movida (se compara leyendo el diff, no se confía en los tests).
- **Cero cambios de algoritmo** en estos PR. Si aparece un bug, va en otro PR.
- **Una fase = un PR**, con sus tests propios y la suite completa en verde en Windows y Linux (CI + WSL).
- **Primero la red de seguridad, luego el movimiento.** No se mueve un área sin pruebas que la fijen (fase 0).

## 4. Arquitectura destino

```
AppController            fachada para QML: propiedades, slots, señales, cableado   (~500–600 líneas)
 ├─ DocumentSession      archivo, píxeles originales, perfil ICC, metadatos, serie, carga
 ├─ EditSession          historial, estado vivo (Ajustes + Filtro), sesión de herramienta,
 │                       fusión por tiempo, receta guardada → "sin guardar"
 ├─ PictureCache         imagen estructural, histogramas, copia reducida; invalidación
 ├─ AdjustModel          sliders de Ajustes: mapa ↔ AdjustOp, niveles, curvas, auto, LUT
 ├─ LookModel            Filtros: listas, miniaturas, LUT
 ├─ EffectPreview   ✔    SaveCoordinator   ✔    AnimationPlayback   ✔    DesktopWallpaper   ✔
 └─ DesktopActions       portapapeles (copiar/pegar) y papelera
```

`EditSession` es la pieza clave: es el **dueño único** de `m_editStack` y del estado vivo. Hoy ese estado está repartido por todo el controlador; con un dueño único, `AdjustModel`, `LookModel`, la receta y el guardado le piden las cosas en vez de tocar variables sueltas. Es la fase más delicada, por eso va después de preparar el terreno.

## 5. Fases

### Fase 0 — Red de seguridad (solo tests, sin tocar código de producción)

`test_app_controller` ya cubre carga, guardado, sesión de herramienta, deshacer/rehacer y efectos (35 casos). **No cubre**, y hay que cubrir antes de mover:
- Ajustes: cada parámetro se acota; niveles y curvas; `autoAdjust`; el mapa `adjust` coincide con lo que se aplicó; `resetAdjust`/`resetLevels`; deshacer de un ajuste y fusión por tiempo (mismo control = una entrada).
- Filtros: `applyFilterPreset` fija `lookActive`/`gradeActive`; intensidad acotada; deshacer.
- Cachés: tras una edición estructural el histograma y la imagen estructural se recalculan (no quedan viejos).
- Orden de señales: un test con `QSignalSpy` que fije el orden de las señales de `applyNewDocument`, `republishBaked` y `publishPicture` (el punto caliente 3).
- Borrar el archivo actual y copiar/pegar.

**Hecho cuando:** los tests nuevos pasan sobre el código actual (si alguno revela un bug existente, se anota y se arregla aparte). **Costo:** 1 PR, ~25 tests nuevos.

### Fase 1 — `AdjustModel` y `LookModel` (el mayor ahorro con el menor riesgo)

Mover la **parametrización de sliders** (~400 líneas): conversión mapa ↔ `AdjustOp`, acotado, niveles/curvas, auto, construcción de las LUT y las listas/miniaturas de filtros. Son funciones casi puras sobre un `AdjustOp`/`QString`.

Interfaz (esbozo): cada modelo recibe el estado vivo por referencia a un pequeño `LiveState {AdjustOp adjust; QString lookId; double lookAmount;}` que **por ahora sigue viviendo en el controlador** y emite `changed(coalesceKey)`; el controlador registra el cambio en el historial y emite las señales como hoy. Así no se mueve todavía el punto caliente 1: solo se aísla la lógica de parámetros.

**Riesgos:** la paridad GPU↔CPU (`test_gpu_parity` y `test_edit_stack` la vigilan) y el orden de señales de `liveAdjustChanged`/`liveFilterChanged`.
**Hecho cuando:** `AppController.cpp` baja ~400 líneas; los modelos tienen tests unitarios sin QML. **Costo:** 1–2 PR.

> **Resultado (11‑oct‑2026):** hecho en un PR, pero como **funciones sin estado** (`AdjustModel`, `LookModel`: reciben el `AdjustOp` actual y devuelven el siguiente), no como clases con estado. `AppController.cpp` bajó **200 líneas, no 400**: la estimación contaba también la LUT, el estado vivo y las señales, que se quedaron en el controlador a propósito (es lo que la fase 3 mueve). Los 27 tests de la fase 0 pasaron sin cambios y se agregaron 19 de los módulos.

### Fase 2 — `PictureCache`

Mover las 5 cachés derivadas y su invalidación (~110 líneas): `structuralBaked`, histograma, copia reducida, histograma ajustado. `markStructuralDirty()` pasa a llamar a `cache.invalidate()` además de lo suyo. Entrada: imagen original + operaciones activas del historial (ya hay `activePixelOps()`).

**Riesgo bajo-medio:** es un interruptor central; el test de la fase 0 sobre "no quedan cachés viejas" es la red.
**Hecho cuando:** ~110 líneas menos y un test unitario por caché. **Costo:** 1 PR.

### Fase 3 — `EditSession` (la fase que importa y la que más riesgo tiene)

Mover el historial (`m_editStack`), el estado vivo, la sesión de herramienta, la fusión por tiempo y las recetas (guardada y de inicio de carga). Operaciones que devuelven **qué cambió** (`Nada | Vivo | Píxeles`), para que el controlador decida qué invalidar y qué emitir, en vez de que el historial toque cachés.

Antes de empezar esta fase se escribe aquí el detalle: la lista de invariantes de `DESARROLLO.md` 4.10 convertida en tests con nombre, y la tabla de "quién llama a qué" de `m_liveAdjust` (las 52 líneas).

**Riesgos:** las invariantes 4–6 de 4.10 (cambios durante una carga, "sin guardar" por contenido, guardar confirma el efecto) y la fusión de deshacer.
**Hecho cuando:** el estado vivo y el historial no se tocan fuera de `EditSession`; ~250 líneas menos. **Costo:** 2–3 PR.

### Fase 4 — `DocumentSession` y `DesktopActions` (punto de decisión)

- `DesktopActions`: copiar/pegar y papelera (~135 líneas). Depende de que "borrar el archivo" pueda decir `document.clear()`.
- `DocumentSession`: archivo, píxeles, ICC, metadatos, serie y el flujo de carga. Está entrelazado con el guardado (la serie evita que un guardado viejo toque un documento nuevo) y con la carga más reciente. Es lo que **conviene dejar en el controlador** si el costo no compensa: que el controlador sea el dueño de "cuál es el documento actual" es razonable para una fachada.

**Decisión pedida al llegar aquí.**

## 6. Cómo se verifica cada PR

1. Suite completa en Windows (local) y Linux (WSL + CI): 27/27 hoy; no baja.
2. Los tests nuevos de la fase corren **antes** del movimiento (sobre el código viejo) y **después** (sobre el nuevo): mismos resultados.
3. Lectura del diff para el **orden de señales** de cada función movida.
4. Para lo que toca la pantalla (Ajustes, Filtros, cachés): la comparación de capturas de la ventana real (el arnés temporal que se usó con el proxy), proxy contra sin cambios; hasta ahora no hay prueba automática de interfaz y es el hueco principal. Si se decide hacer las fases 1–3, conviene **dejar ese arnés como herramienta del repositorio** (script + variable de entorno) en vez de reescribirlo cada vez.
5. Cada PR declara lo que no se probó, como hasta ahora.

## 7. Qué ganamos y qué no (estimación, no medición)

| Tras la fase | `AppController.cpp` aprox. |
|---|---|
| hoy | 1434 |
| 1 (Ajustes + Filtros) | ~1030 estimado; **real: 1234** |
| 2 (cachés) | ~920 |
| 3 (historial/estado vivo) | ~670 |
| 4 (documento + escritorio) | ~420 |

Los números salen de las líneas por área de la sección 2; el resultado real será algo distinto (parte del código que se mueve deja pegamento detrás).

**Lo que sí ganamos:** áreas con tests propios sin QML; un dueño único para el estado de edición (hoy el origen de las carreras que ya se corrigieron en las rondas 2–4); cambios futuros (capas, máscaras) con dónde apoyarse.
**Lo que no ganamos:** ni más velocidad, ni funciones, ni menos bugs *por sí mismo*. Es deuda técnica pagada para poder cambiar después con menos miedo. El riesgo es real: tocar el historial y el estado vivo puede reintroducir las carreras de las rondas anteriores.

## 8. Opciones y recomendación

- **A. Parar aquí.** `AppController` queda en 1434 líneas con cuatro piezas ya fuera. Es defendible: lo extraído eran los trozos con estado propio y el resto es "la lógica del visor". Costo cero.
- **B. Fases 0–2 (recomendada).** Red de seguridad + Ajustes/Filtros + cachés: ~510 líneas menos, riesgo bajo-medio, mucho ya cubierto por `test_gpu_parity` y `test_edit_stack`. Se decide la fase 3 con los tests nuevos en la mano y un plan más fino.
- **C. Todo (0–4).** Solo si hay una razón concreta para el cambio de fondo (capas o máscaras a la vista). Sin esa razón, la fase 3 es riesgo sin retorno visible.

**Recomendación: B**, y decidir la fase 3 después, porque para entonces habrá tests que digan qué tan firme es el terreno.
