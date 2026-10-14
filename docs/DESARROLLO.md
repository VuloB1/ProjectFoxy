# Project Foxy — Documento de desarrollo

> Documento pensado para pasárselo a un revisor externo (por ejemplo ChatGPT) y
> pedirle opinión sobre el desarrollo: qué mejorar, qué riesgos hay y qué falta
> para que el programa sea lo más profesional posible.
>
> Estado descrito: **5 de octubre de 2026**. Es la **cuarta ronda** de revisión (las secciones
> 4.13 a 4.18 describen lo añadido después, el 4‑5 de octubre, y todavía no pasaron por
> revisión externa): la
> primera versión (1‑oct) y las siguientes ya pasaron por revisión externa, y la sección 8
> recoge qué dijo cada una, qué se comprobó contra el código y qué se corrigió (la última,
> la auditoría técnica final, está en 8.8). Todo lo que aparece
> aquí sale del código, de pruebas ejecutadas o de mediciones; lo que **no está
> verificado** está marcado como tal (sección 10.2).
>
> **Actualización del 11 de octubre de 2026 (versión 0.2.1).** Desde el 5 de octubre se añadieron: git/GitHub, CI,
> instalador, licencia, 6 idiomas, el puerto a Linux (sección 16) y mejoras de navegación y miniaturas (sección 17).
> Las secciones 8 y 10 conservan el texto de su fecha; lo que quedó **superado** está marcado con «(superado: …)».

---

## 0. Cómo usar este documento (instrucciones para el revisor)

Eres un ingeniero senior de software de imagen / aplicaciones de escritorio.
Lee el documento completo y responde en español, con prioridades claras.

1. Empieza por la **sección 8** (sobre todo **8.8**): contiene las revisiones anteriores
   contrastadas con el código. Dime si las correcciones te parecen suficientes y bien planteadas, y si
   alguna conclusión de esa sección te parece errónea.
2. **Arquitectura**: ¿la separación núcleo (`core`) / capa app / QML es sólida?
   `AppController` (1 500 líneas) concentra mucho: ¿es ya un *God object*?
3. **Modelo de edición** (secciones 4.1 y 4.7): ahora todo —píxeles, efectos,
   Ajustes y Filtros— comparte un único historial. ¿Es un buen punto de partida para
   capas/máscaras o hay que rediseñarlo antes?
4. **Calidad de imagen**: 8 bits por canal, todo convertido a sRGB al abrir (4.12),
   remuestreo, guardado (sección 4).
5. **Rendimiento y memoria**: hay mediciones reales (sección 5.1).
6. **Pruebas y calidad de ingeniería** (sección 7): ¿qué falta todavía?
7. **Qué falta para ser profesional** (sección 11) y en qué orden (sección 9).
8. **Licencias** (sección 12 y `LICENCIAS.md`): ¿ves algo que se me escapó?
9. Señala cualquier cosa que esté mal planteada, aunque no esté en las preguntas.

Pide el código de un archivo concreto si lo necesitas; los nombres están en el
apéndice (sección 13).

---

## 1. Qué es el producto

Visor de imágenes moderno, ligero y rápido **solo para Windows**, con un editor
integrado de fotos. Interfaz en español. Tres temas visuales: *Moderno Claro*,
*Moderno Oscuro* y *Windows 98*.

**Visor**
- Abre un archivo (diálogo, arrastrar y soltar, "Abrir con", menú contextual del
  Explorador mediante archivos `.reg` incluidos) y escanea su carpeta con orden
  natural (`img2` antes que `img10`). Funciona con nombres de archivo y carpetas en
  cualquier idioma (acentos, japonés, cirílico, emoji).
- Barra lateral izquierda colapsable con miniaturas (caché en memoria + caché en
  disco), navegación con flechas, zoom/desplazamiento por GPU, ajustar a pantalla.
- Comparar antes/después, copiar/pegar del portapapeles,
  fondo de escritorio, papelera, panel de información con EXIF e histograma.
- **Respeta la orientación EXIF** (fotos de móvil), en tamaño completo y en miniatura.
- GIF, APNG y WebP animados con reproducción real.
- **Comparador** (antes «Varias imágenes»: 2 a 10 a la vez, disposición automática / fila / columna) con zoom y desplazamiento vinculados (4.16, 4.19).
- Exportación por lote (JPG/WebP con calidad configurable, renombrado con relleno de
  dígitos); conserva los metadatos del original. Configuración en un `.ini`.

**Formatos** (decodificadores en `src/core/decoders`, **comprobados con un archivo
real de cada tipo** en las pruebas)
- libvips: JPEG, PNG, WebP, TIFF.
- Qt (`QtImageDecoder`): BMP, GIF estático, ICO.
- LibRaw (`libraw_r`, reentrante): RAW de cámaras (CR2, NEF, ARW, DNG…).
- libheif + libde265 + libaom: HEIC/HEIF (HEVC) y AVIF (AV1).
- `AnimatedDecoder` propio para GIF/APNG/WebP realmente animados (APNG reconstruido a
  mano, sin librería externa; el WebP animado con libwebpdemux).
- **No** se abren PSD, TGA ni JXL (no hay ImageMagick/libjxl en este build): antes
  se anunciaban y fallaban.
- Guardado en PNG, JPG, BMP, TIFF y WebP (sección 4.7).

**Editor** (pestañas del panel derecho)
| Pestaña | Contenido |
|---|---|
| Recortar | Dos páginas: **Recorte** (selección con 8 tiradores, proporciones predefinidas, rotar 90°, enderezar, voltear) y **Marco** (forma, esquinas redondas/suaves/cortadas/cóncavas, contorno, margen, sombra, fondo; 4.15) |
| Tamaño | Por píxeles o por escala (con tope en 100 %), Lanczos‑3 + nitidez sin halos al ampliar |
| Ajustes | 18 controles de tono/color/detalle, niveles y curvas por canal, auto color/niveles/contraste, histograma en vivo, grano/ruido en 4 tipos |
| Filtros | 38 "looks" por receta con miniaturas de la propia foto, control "Cantidad", acabado (viñeta y grano) |
| Efectos | **43 efectos** de un solo uso en 7 grupos (desenfoque, estilo, color, dibujo, luz, distorsión, acabado; 4.13), con vista previa en toda la imagen, sliders propios, preajustes, tiradores sobre la imagen y "Mezcla" |
| Lente | Corrección de distorsión, franjas de color y esquinas oscuras con la base de perfiles de Lensfun (cámara y objetivo reconocidos por el EXIF) o a mano (4.14) |
| Crear (menú de la barra superior) | **GIF animado** (4.17): tira de imágenes con efecto y texto por fotograma, transiciones, exportación a GIF/APNG/WebP · **Collage** (4.18): 1 a 12 fotos en celdas con líneas divisorias ajustables o diseño libre, fotos sin límites dentro de su celda, plantillas guardables |

**Deshacer/Rehacer** cubre todo, en orden cronológico: recortes, tamaños, giros,
efectos **y también los movimientos de Ajustes y Filtros** (un arrastre completo de un
slider = un paso). **Avisos de seguridad**: confirmación antes de sobrescribir el
original y pregunta antes de perder cambios al cerrar o abrir otra imagen.

Tamaño del código (líneas no vacías, aproximado, 5‑oct‑2026): núcleo C++ ≈ 11 300 · capa app C++
≈ 5 100 · QML ≈ 10 600 · shaders GLSL ≈ 350 · pruebas ≈ 6 800.

---

## 2. Stack y entorno

- **Lenguaje/UI**: C++20, Qt 6.7.3 (mínimo declarado 6.6), Qt Quick/QML con estilo
  *Basic* totalmente personalizado, renderizado RHI por **Direct3D 11** (se puede
  cambiar con `QSG_RHI_BACKEND`).
- **Build**: CMake ≥ 3.21 + Ninja + MSVC 2022 (Build Tools), dependencias nativas con
  **vcpkg** en modo manifiesto (`vcpkg.json`: libvips, libraw, libheif **sin
  características por defecto** + aom, exiv2, pkgconf). `CMakePresets.json` define
  `windows-debug/release`; cada desarrollador usa un `CMakeUserPresets.json` (hay un
  `.example`).
- **Shaders**: `qt_add_shaders` compila `Grade.frag` y `Detail.frag` a `.qsb`
  (objetivos GLSL `150,300es` porque el ruido usa enteros sin signo).
- **Pruebas**: QtTest, 19 ejecutables, **430 casos** (incluida la **paridad CPU↔GPU con
  Direct3D 11** y el `AppController` real), más una **prueba de humo** del programa real
  (`smoke_startup`): 12 entradas en `ctest`, todas pasan.
- **Plataforma**: Windows (usa `Shell32`/`User32`, fondo de escritorio por
  `SPI_SETDESKWALLPAPER`, papelera de Windows) y, desde la 0.2.0, Linux (sección 16). *(Superado: el texto original
  decía «solo Windows».)*

---

## 3. Arquitectura

```
┌──────────────────────────── QML (qml/) ────────────────────────────┐
│ Main · TitleBar · Toolbar · StatusBar · ThumbnailStrip · ImageCanvas│
│ EditPanel (Recortar/Tamaño/Ajustes/Filtros/Efectos) · Diálogos      │
│ controls/App*.qml  → envoltorios tematizados de cada control Qt     │
│ shaders/Grade.frag + Detail.frag  → vista previa en GPU             │
└───────────────▲──────────────────────────────▲──────────────────────┘
   context properties / image://                │
┌───────────────┴───────── src/app (depende de Qt Quick) ─────────────┐
│ AppController  (estado de edición, API para QML, hilos de trabajo)  │
│ ImageProvider  image://viewer/{current,original,lut,looklut,        │
│   lookthumb/<id>, effectthumb/<id>}  ·  ThumbnailImageProvider      │
│ FolderModel · ThemeManager · AppSettings · BatchExporter            │
└───────────────▲─────────────────────────────────────────────────────┘
                │ llamadas directas
┌───────────────┴──── src/core (biblioteca estática, sin QML) ────────┐
│ ImageLoader (async/cancelable, pool propio, precarga de vecinas)    │
│ DecoderRegistry → Raw / Heif / Animated / QtImage / Vips Decoder    │
│ ThumbnailCache (memoria + disco) · MetadataReader (exiv2)           │
│ ImageWriter (guardado atómico + metadatos) · Histogram              │
│ edit/ EditStack · Operations · AdjustMath · Looks · Effects ·       │
│       Resample · ParallelRows                                       │
└─────────────────────────────────────────────────────────────────────┘
```

Principios:
- `core` **no depende de QML**: la decodificación, la caché, la edición y el guardado
  se prueban de forma aislada (`tests/`). La UI solo consume esa biblioteca.
- `AppController` es la **puerta principal** entre QML y el motor (carga, edición,
  guardado). QML también consume `folderModel`, `themeManager`, `appSettings` y
  `batchExporter`; ningún QML llama a `core` directamente.
- Los píxeles viajan a QML por **`QQuickImageProvider`** con una URL que lleva un
  número de revisión (`image://viewer/current?rev=N`) para forzar la recarga.
- Un único `VipsGuard` (mutex global) serializa toda entrada a libvips (sección 5.2).
- Los trabajos pesados por filas usan un **pool propio** (`rowWorkerPool()`), no el
  pool global de Qt, para evitar interbloqueos con otras tareas (miniaturas).

### 3.1 Tematización
`ThemeManager` (C++) guarda una tabla fija de 3 temas (colores, radios, tiempos de
animación, tipografía y un campo `skin`: `modern` o `win98`). Cada control de Qt Quick
Controls usado tiene un envoltorio `qml/controls/App*.qml` cuyo fondo cambia según
`themeManager.skin` mediante un `Loader`. Los iconos son **vectoriales dibujados en
`Canvas`** (`AppIcon.qml`). Los temas Windows Vista/7 (Aero con desenfoque real) y
Windows XP se construyeron y se **eliminaron** a petición del usuario (sección 8.6).

---

## 4. Pipeline de edición y de imagen (el corazón técnico)

### 4.1 Un único historial, dos clases de operación
El historial (`EditStack`) es una lista ordenada de operaciones con deshacer/rehacer:
1. **Operaciones de píxeles** (`CropOp`, `ResizeOp`, `RotateOp`, `FlipOp` y
   `EffectOp`, sección 4.6): cambian la imagen. Se "hornean" en CPU
   (`structuralBaked()`, con caché y bandera de sucio) y se republican por el
   `ImageProvider`.
2. **Capa viva** (Ajustes y Filtros): no se hornea; la **GPU** la dibuja encima de la
   imagen y solo se aplica en CPU al guardar/exportar. Su estado se guarda en el
   historial como `LiveOp` (una *instantánea* de sliders + look), que **no cambia
   píxeles**: la reproducción de la pila la salta, y el estado en cualquier punto es
   simplemente el `LiveOp` más reciente anterior a esa posición, así que deshacer y
   rehacer lo restauran sin lógica extra.

   **Transacciones**: los pasos de un mismo control (`p:exposure`, `levels:2`,
   `curves:0`, `look`…) dentro de 900 ms se **funden en una sola entrada** (se
   reemplaza el `LiveOp` en lugar de apilar otro). Un arrastre completo de
   «Exposición» es un paso; mover luego «Brillo» es otro. Verificado en la interfaz.

**Qué es «sin guardar» (`isDirty`).** Se compara el *contenido*, no la posición del
historial: `AppController` conserva la «receta» (`EditRecipe`: las operaciones de píxeles
activas más la capa viva normalizada) de la imagen tal como se abrió o se guardó por última
vez, y `isDirty()` es «la receta actual es distinta de esa». Así *Recortar → Guardar →
Deshacer* **sí** es un cambio sin guardar (la pantalla ya no coincide con el archivo), una
rama nueva tras deshacer también (misma posición, otro contenido) y dos ediciones que se
anulan entre sí no lo son. Antes era «hay alguna operación activa y se tocó algo desde el
último guardado», que daba falso con *Guardar → Deshacer* y dejaba cerrar sin avisar.

`EditStack::bake()` rehace la pila desde la imagen original (simple, sin error de
redondeo acumulado), con una excepción: **puntos de control** (resultados terminados
por posición del historial, ligados a la imagen de origen por su `cacheKey`), de modo
que deshacer y rehacer —incluso un efecto lento— no repiten trabajo. La caché es **LRU**
con un **presupuesto en bytes** que `AppController` fija al abrir cada imagen: un cuarto
de la RAM libre en ese momento, entre 256 MB y 1,5 GB (máx. 6 entradas).

### 4.2 Una definición, dos renderizadores (Ajustes)
`AdjustMath.{h,cpp}` es la definición de referencia de la matemática; la GPU la
replica (ver la salvedad de abajo):
- **Etapa A** — tabla de tonos por canal (3×256 bytes, `buildAdjustLut`): balance de
  blancos, exposición, niveles, gamma, negros/blancos, curvas, brillo, contraste.
- **Etapa B** — mezcla de color por píxel (`ColorMix`): sombras/luces, saturación,
  intensidad (*vibrance*), matiz, negativo.
- **Etapa C** — `applyDetail`: claridad, nitidez de 5 taps, viñeta, ruido/grano.

La vista previa en GPU replica esto línea a línea: `Grade.frag` (etapas A+B; la tabla
se pasa como textura 256×1 servida por el `ImageProvider`) y `Detail.frag` (etapa C).
Cadena en `ImageCanvas.qml`:

```
Image oculta (píxeles de la pila)
  → lookEffect  (Grade.frag con el look de Filtros)
  → adjustEffect(Grade.frag con los sliders de Ajustes)  → ShaderEffectSource (mipmap)
  → detailEffect(Detail.frag: nitidez, claridad, viñeta, ruido)   [lleva la transformación de vista]
```

**Salvedad que apunta la revisión externa (aceptada)**: llamar a `AdjustMath` «única
fuente de verdad» es conceptual; técnicamente hay **dos implementaciones** (C++ y GLSL)
de varias operaciones. Las une **`test_gpu_parity`** (sección 7), que renderiza la cadena
real de shaders con Direct3D 11 y la compara con el archivo que escribiría la CPU. La CPU
es el resultado autoritativo de la exportación y la GPU la representación interactiva.

**Contrato de alfa (cuarta ronda, 8.8).** Qt Quick entrega las texturas a los shaders
**premultiplicadas** y espera un resultado premultiplicado; la CPU trabaja con RGBA
*recto*. Con imágenes opacas no se nota (alfa 1), pero con transparencia la GPU aplicaba
la matemática de color a valores ya multiplicados por el alfa (diferencias de hasta 87
niveles en «Brillo» y 247 en «Negativo»). Contrato actual, igual en CPU y GPU: las
operaciones **por píxel** (tabla de tonos, mezcla de color, extras de looks, viñeta, grano)
operan sobre color recto —los shaders lo des‑premultiplican al entrar y lo premultiplican
al salir—; las **de vecindad** (nitidez y claridad) operan sobre color premultiplicado, que
es la forma correcta de filtrar con alfa: un vecino transparente pesa cero y no «sangra» su
color oculto. El alfa nunca se modifica.

Regla importante: **todo uniforme del bloque UBO debe declararse como propiedad QML**
en el `ShaderEffect`, o llega como basura sin inicializar (Qt solo avisa).

### 4.3 Filtros ("looks")
`Looks.{h,cpp}` es un catálogo en código (sin assets): cada look es un `AdjustOp` +
extras (mapa de degradado de 3 paradas, tono dividido, viñeta, grano). 38 looks en 5
grupos: Cine (7), Retro (9), B/N (7), Duotono (8), Color (8). `applyLook(imagen, id,
cantidad)` = `applyGrade()`; "Cantidad" mezcla entre original y resultado. Las
miniaturas se renderizan con la **misma** función sobre un proxy de 128×128.

### 4.4 Ruido / grano
Modelado sobre el diálogo "Añadir ruido" de PhotoScape X Pro. Cada píxel obtiene su
propia muestra de un **hash entero** `noiseHash(x,y,canal)` (tipo lowbias32, idéntico en
CPU y GLSL porque los `uint` envuelven igual). Distribuciones: Uniforme, Gaussiana
(Box–Muller), Impulso (sal y pimienta) y Laplaciana, todas de varianza unitaria, con
opción Monocromático. Se reescribió **dos veces** a petición del usuario.

### 4.5 Redimensionado de calidad (sin IA)
`Resample.{h,cpp}`: Lanczos‑3 separable por **bandas de 32 filas de salida** (sin
intermedio float del tamaño completo), alfa premultiplicado. Al ampliar >1,05× se añade
un *unsharp* solo sobre luma con *coring* y recorte al mínimo/máximo del vecindario 3×3
(evita halos). Medido con PSNR de «reducir y volver a ampliar» sobre fotos reales:
Lanczos supera al suavizado de Qt en 1–3 dB y la nitidez suma hasta 0,9 dB.

### 4.6 Efectos (pestaña "Efectos")
`Effects.{h,cpp}`: catálogo de 17 efectos y `applyEffect(imagen, id, valores, mezcla,
cancelar)`.
- **Toda distancia es un porcentaje del lado largo** (nunca píxeles): el mismo ajuste da
  el mismo aspecto sobre una copia reducida y sobre la foto completa (prueba: diferencia
  media de 0,1–0,45 niveles entre «efecto a 480 px y reducir» y «reducir y efecto a 120 px»).
- Los efectos que mezclan vecinos trabajan en RGBA **premultiplicado** (el color oculto
  de un píxel transparente no se filtra); el resto usa alfa directo.
- Es una operación del historial (`EffectOp`): se hornea y se deshace.
- **Vista previa asíncrona en dos etapas**: cada cambio de parámetro crea una
  «generación» y cancela la anterior con una bandera atómica que consultan todas las
  pasadas por filas. Etapa 1: si la foto mide más de 2 200 px, el efecto se calcula
  sobre una copia de 1 600 px y se estira (aparece al instante); etapa 2: el exacto a
  tamaño completo tras 260 ms sin tocar los sliders. Se publica sin la señal que
  reinicia el zoom.
- **Aplicar** reutiliza el resultado exacto si ya está calculado y lo registra como
  punto de control. La vista previa pendiente se descarta al cambiar de pestaña, cerrar
  el panel, deshacer o restablecer.
- **Guardar confirma el efecto pendiente; copiar y fondo de escritorio no.** Guardar
  escribe lo que se ve y, para que el archivo y el historial cuenten lo mismo, primero
  convierte la vista previa en un paso real y deshacible (`commitEffect()`). Si no se
  confirmara, «Cancelar efecto» dejaría la pantalla sin efecto y el archivo con él. Copiar
  al portapapeles y poner de fondo usan lo que se ve **sin** tocar el historial.
  (Historia: la primera versión confirmaba al guardar; la segunda ronda lo quitó por ser un
  efecto lateral poco visible; la cuarta ronda demostró que eso desincronizaba el archivo y
  el estado «sin guardar», y se restituyó con una prueba que lo cubre.)
- Revisión de los cuatro puntos de la revisión externa en el código: (1) una generación
  antigua nunca publica tras una nueva —`finishEffectJob` compara generación y bandera—;
  (2) el resultado exacto que reutiliza Aplicar queda identificado por generación, que
  cambia con cualquier cambio de parámetros; (3) cambiar de imagen o de operaciones
  invalida la vista previa (`markStructuralDirty()` → `dropEffectPreview()`); (4) una
  tarea cancelada libera su buffer completo de inmediato (corregido: antes quedaba en un
  evento en cola hasta que lo descartaba el hilo de la interfaz).
- Rendimiento con 25 MP: casi todos 0,5–1,2 s en su versión exacta; movimiento, zoom y
  giratorio 4,5–6,5 s (muestreo por píxel). Tras tres arrastres rápidos con el más pesado,
  la interfaz queda libre 2,6 s después del último: no se acumulan cálculos viejos.

### 4.7 Guardado seguro (`ImageWriter`)
Hasta el 2‑oct‑2026 «Guardar» hacía `QImage::save(ruta)` **sobre el original**: sin
confirmación, **JPEG con la calidad por defecto de Qt (≈75, con submuestreo de croma)**,
escritura no atómica y **sin ningún metadato** (se perdían cámara, fecha, GPS y perfil de
color). Ahora:
- **Atómico**: se codifica en memoria y se escribe con `QSaveFile` (archivo temporal en
  la misma carpeta + renombrado). Un fallo —disco lleno, extensión no soportada,
  carpeta inexistente— deja el original intacto y no deja temporales (probado).
- **Calidad**: JPEG/WebP a 95 por defecto; JPEG con **4:4:4** (sin submuestreo de
  croma) desde calidad 90, vía libvips. Medido sobre una imagen de prueba: **38,1 dB →
  49,5 dB** de PSNR respecto del original con el mismo contenido (el archivo pasa de
  ~11 KB a ~38 KB). La transparencia se aplana sobre blanco en JPEG.
- **Metadatos**: se copian EXIF descriptivo (cámara, objetivo, exposición, fechas, GPS,
  copyright), IPTC y XMP del original (JPEG, PNG y WebP; **el perfil ICC ya no se copia**,
  ver 4.12: el archivo se etiqueta como sRGB). Se restablece
  la orientación a 1 (los píxeles ya están girados) y se actualizan las dimensiones. No
  se copian las notas de fabricante (sus desplazamientos dependen del archivo original).
  También se aplica en la exportación por lote. **Corrección de la cuarta ronda (8.8):**
  esto se afirmaba, pero el exiv2 de vcpkg se había compilado **sin zlib y sin el toolkit
  XMP** (funciones `png` y `xmp` del port, desactivadas por defecto): no reconocía los PNG
  y no leía ni escribía XMP, de modo que el XMP no se conservaba en ningún formato y los PNG
  no conservaban nada. Ahora `vcpkg.json` activa `png` y `xmp`, y hay pruebas con fixtures
  reales (EXIF + ICC + XMP + IPTC en JPEG; eXIf + iCCP + XMP en PNG; las cuatro conversiones
  entre JPEG, PNG y WebP). Los orígenes **HEIC/AVIF/RAW** siguen sin poder entregar sus
  metadatos (requeriría la función `bmff` de exiv2 o extraerlos con libheif/LibRaw): el
  guardado funciona y **avisa** con una nota no bloqueante («La imagen se guardó, pero…»).
- **Asíncrono** (después de la cuarta ronda): pulsar Guardar no congela la ventana. Ver
  4.11.
- **Confirmación** antes de sobrescribir (desactivable con «No volver a preguntar» o en
  Configuración) y **aviso de cambios sin guardar** al cerrar la ventana o al abrir otra
  imagen (Guardar / Descartar / Cancelar). «Guardar» sobre un RAW/HEIC o una imagen
  pegada se convierte en «Guardar como».
- Nombres de archivo en cualquier idioma, en todos los formatos (probado).

### 4.8 Formatos: orientación EXIF y nombres Unicode
- **Orientación EXIF** (1–8): el libvips de este build **no lee EXIF** (no tiene la
  característica `exif`/libexif), así que ni siquiera su vista previa giraba bien. Ahora
  la orientación se obtiene de libvips cuando el formato la trae (TIFF) y, para JPEG, del
  lector de Qt, y se aplica una sola vez —igual en vista previa y a tamaño completo—
  desactivando la rotación automática de libvips. Probado con las 8 orientaciones.
- **Rutas Unicode**: `VipsDecoder` pasaba la ruta con `QFile::encodeName` (página de
  códigos ANSI) a una libvips que espera UTF‑8: **ni «ñandú.jpg» se abría**. Corregido;
  `main.cpp` pasó de `fromLocal8Bit(argv[1])` a `QCoreApplication::arguments()`. Probado
  de punta a punta abriendo por línea de comandos una foto en una carpeta con ñ y japonés.
- **Archivos bloqueados**: tras decodificar un `.webp`, libvips conservaba el archivo
  abierto (por su caché de operaciones), lo que en Windows impide borrarlo, renombrarlo o
  sobrescribirlo con «Guardar». Se desactiva esa caché (la app ya tiene la suya).
- **Formatos que no abrían**: BMP, GIF estático e ICO no funcionaban en este libvips; los
  cubre `QtImageDecoder`. PSD/TGA/JXL se dejaron de anunciar.
- **Modelos de color (cuarta ronda)**: `VipsDecoder` solo trataba «menos de 3 bandas» y
  «3 bandas» y daba por RGBA cualquier otra cosa. Comprobado con fixtures: un PNG/TIFF de
  **16 bits por canal se abría completamente blanco** (`cast` recorta en lugar de escalar) y
  un JPEG **CMYK** salía con colores erróneos y **agujeros transparentes** (el canal K se
  leía como alfa). Ahora todo lo que no es sRGB pasa por `colourspace(sRGB)` (que escala los
  16 bits, convierte grises y conserva el alfa) y el CMYK se convierte con el lector JPEG de
  Qt (este libvips no tiene motor ICC: `vips_icc_present()` vale 0). Es una conversión
  *simple*, sin perfil: el color de un CMYK real no será el de un RIP profesional.

### 4.9 Lo que el pipeline **no** hace (hoy)
- Todo es **8 bits por canal RGBA** (`Format_RGBA8888`); los archivos de 16 bits se **leen**
  y se escalan a 8, pero no se edita ni se guarda a 16 bits; sin HDR.
- **Color**: todo se convierte a sRGB de 8 bits al abrir (4.12). No se conserva el gamut
  ancho (lo que sale de sRGB se recorta), no se aplica el perfil del monitor y no hay
  16 bits ni HDR internos.
- La pila de edición no se serializa a disco (el comentario de `Operations.h` habla de un
  "sidecar", pero no está implementado).
- No hay modelo de capas, máscaras, texto ni pinceles (ver sección 9).

### 4.10 Invariantes del documento: carga, archivo y «sin guardar» (cuarta ronda)
La auditoría final (8.8) señaló que el documento visible, su archivo y el estado guardado
vivían en variables independientes de `AppController`. Reglas que ahora se cumplen y se
prueban (`test_image_loader`, `test_app_controller`):
1. **Una carga solo la publica la petición más reciente de su id.** `ImageLoader` da a
   cada petición una bandera de cancelación; el trabajador la consulta antes de decodificar
   (una petición aún en cola ni se decodifica) y antes de leer los metadatos, y la entrega
   al hilo de la interfaz se filtra **allí**, en el momento de publicar. Antes `cancel()`
   solo borraba el id de un `QSet` que nadie consultaba: con A lento y B rápido, A
   reaparecía encima de B (5 de 6 casos de prueba fallaban).
2. **El archivo cambia con los píxeles, no antes.** `m_currentFilePath` solo cambia en
   `applyNewDocument()`. Antes `loadPath()` lo cambiaba al pedir la carga; con B aún
   decodificando —o si B fallaba— quedaban los píxeles de A con el nombre de B.
3. **No se guarda mientras otra imagen carga** (`saveBlocked`, y la interfaz lo
   refleja). Era el bloqueador: *editar A → guardar → abrir B → Ctrl+S mientras B carga*
   **escribía los píxeles de A encima del archivo B** (comprobado en el estado anterior).
4. **Lo editado durante una carga no se pierde en silencio**: si la imagen nueva llega y
   mientras tanto se cambió la receta, se hace la misma pregunta «cambios sin guardar» en
   lugar de reemplazar.
5. **«Sin guardar» compara contenido** (4.1): `EditRecipe` actual frente a la guardada.
6. **Guardar confirma el efecto pendiente** (4.6).
7. Guardar habilita también el caso «la pantalla difiere del archivo sin ediciones activas»
   (*Guardar → Deshacer*).

Las reglas del guardado asíncrono (instantánea, un solo guardado, navegación y cierre que
esperan) están en 4.11.

Dejado a propósito: si B **falla**, la barra lateral queda posada en B (para que «siguiente»
no se atasque en un archivo corrupto) mientras el documento sigue siendo A.

### 4.11 Guardado asíncrono
Antes, `saveEdited()` hacía todo en el hilo de la interfaz: reproducir el historial, el
efecto pendiente a tamaño completo, el filtro, los ajustes, codificar, copiar metadatos y
escribir. Medido con una foto de **12 MP con filtro + ajustes + JPEG: 603 ms** con la
ventana congelada (con 37 MP y un efecto pesado, varios segundos). Ahora:
- **Instantánea inmutable.** Al pulsar Guardar, el hilo de la interfaz solo valida y toma una
  copia de lo que el archivo contendrá: la imagen original (`QImage` es copy‑on‑write: no
  copia píxeles), las operaciones de píxeles activas, el resultado estructural si ya está en
  caché, el filtro y los ajustes, la receta (`EditRecipe`), la ruta y las opciones. Un
  trabajador (`m_savePool`, un solo hilo) hace **todo lo pesado**: `bakeOperations()`
  (reproducción sin caché ni estado compartido, comprobada idéntica a `EditStack::bake`),
  `renderLiveOverlay()` (filtro y ajustes: la única definición de «lo que contiene el
  archivo», compartida con copiar y fondo de escritorio), codificación, metadatos y escritura
  atómica con `QSaveFile`.
- **Efecto pendiente.** Se confirma en el historial al instante. Si su resultado exacto ya
  estaba calculado se reutiliza; si no, **lo calcula el trabajador** (el hilo de la interfaz
  no lo calcula) y el resultado vuelve para reutilizarse si el documento sigue pidiendo
  exactamente eso.
- **«Guardado» es lo que contenía la instantánea**, no lo que haya en pantalla al terminar:
  `m_savedRecipe` se fija a la receta de la instantánea. Una edición hecha mientras se
  escribe queda **sin guardar** después (probado: el archivo tiene el recorte, la pantalla ya
  tenía además un giro, `isDirty()` sigue verdadero).
- **Un guardado a la vez.** Otro se rechaza con un mensaje (`isSaving`/`canSaveInPlace`
  también lo reflejan en la interfaz).
- **Abrir otra imagen o pegar mientras se guarda espera** a que termine y entonces se hace
  (sin preguntar «cambios sin guardar» de forma espuria, porque hasta que acaba la escritura
  la imagen figuraba como sin guardar). Si el guardado **falla**, lo que esperaba se descarta,
  la barra lateral vuelve a la imagen abierta y las ediciones siguen sin guardar. La
  respuesta «Guardar» de la pregunta de cambios sin guardar usa el mismo mecanismo.
- **Cerrar la ventana mientras se escribe** la deja abierta (con «Guardando…») hasta que
  termina y entonces cierra; si falla, no cierra y el error queda a la vista. El destructor
  del controlador también espera al trabajador.
- **Eliminar el archivo** se rechaza mientras se guarda. El número de serie del documento
  (`m_docSerial`) impide que un guardado tardío toque el estado de otra imagen.
- **Interfaz**: «Guardando…» y después «Guardado» (o el aviso de metadatos) en el rótulo
  superior del lienzo; Guardar y Guardar como… se desactivan mientras tanto. Señales:
  `isSaving`, `saveFinished(ok, ruta)`.
- **Medido** (`savingDoesNotFreezeTheInterface`, misma foto de 12 MP): empezar el guardado
  tarda **0 ms** y el peor parón del bucle de eventos durante todo el guardado es de
  **17 ms**; con un efecto «mediana» pendiente sobre 4,3 MP, empezar también tarda 0 ms. En
  la ventana real con una foto de 37 MP: «Guardando…» visible a los 150 ms, cambiar de
  pestaña durante el guardado responde, y cerrar en mitad de un guardado con filtro espera y
  deja el archivo completo (7720×4824, sin temporales).
- **Límites**: no se puede cancelar un guardado en curso; copiar al portapapeles, poner de
  fondo, el histograma y el botón «Aplicar» de un efecto siguen calculándose en el hilo de la
  interfaz; una edición estructural hecha mientras un efecto se calcula en el trabajador
  fuerza su recálculo síncrono si luego se necesita la imagen estructural.

### 4.12 Color: todo en sRGB (quinta ronda)
**El problema.** El perfil ICC se *conservaba* al guardar pero nunca se *aplicaba*: un archivo
en Display P3 (todo iPhone reciente) o AdobeRGB se mostraba con sus números leídos como
sRGB, es decir, **apagado** (medido: un P3 (200,120,60) debía verse (213,115,42) y se veía
(200,120,60)). Peor: al guardar se copiaba el perfil del original sobre los píxeles, de
modo que cualquier visor lo aplicaba **dos veces** (en la prueba, (213,115,42) reabierto
salía (228,108,0)).

**El contrato.** El editor trabaja en **un solo espacio: sRGB de 8 bits**. `ColorManagement`
(`convertToSrgb`) usa `QColorSpace`/`QColorTransform` de Qt (sin dependencias nuevas: este
libvips no tiene motor ICC, `vips_icc_present()` vale 0). Cada decodificador convierte al
terminar, así que la imagen, las miniaturas, la vista previa y el archivo guardado hablan de
los mismos números y `DecodeResult::image` es siempre sRGB:
- **libvips** (JPEG/PNG/WebP/TIFF, 8 y 16 bits): perfil leído de la cabecera.
- **libheif** (HEIC/AVIF): perfil ICC, o **nclx con primarios de Display P3** (el perfil
  cuelga del manejador de la imagen, no de la imagen decodificada; BT.709 ya es sRGB y
  BT.2020/HDR se dejan tal cual).
- Perfiles de gris o CMYK no se aplican (el CMYK ya pasa por la conversión simple de 4.8).
- Un perfil RGB que Qt **no puede usar** (tablas, dañado) deja los píxeles intactos y se
  conserva en `DecodeResult::iccProfile`/`ImageDocument`/`SaveOptions::iccProfile` para
  volver a adjuntarlo al guardar; solo un perfil RGB puede quedarse así.
- Un perfil que resulta ser sRGB (el de Windows, el de Qt…) se detecta y no se toca.

**Al guardar.** El perfil del original **nunca** se copia. JPEG/WebP lo reciben de exiv2,
TIFF de libvips y PNG de un trozo `iCCP` construido en `ImageWriter` (exiv2 lo escribía con
el **nombre vacío**, que PNG no permite: libpng avisaba «bad keyword»). Sin perfil pendiente
se escribe sRGB.

**Pruebas** (`test_color_management` 16, más casos en decodificadores, escritor y
controlador): P3 y AdobeRGB frente a **matemática de referencia independiente** (matrices
D65, ±2 niveles); PNG 8 y 16 bits, JPEG, TIFF, HEIC y AVIF (ICC y nclx) en P3 convertidos a
la referencia; la vista previa convierte igual; un archivo sRGB conserva sus números; el
alfa no se toca; guardar y reabrir no vuelve a convertir; el perfil pendiente se adjunta.
Verificado además **en la ventana real**: un PNG sólido en P3 se pinta (213,115,42) y uno sin
perfil se queda en (200,120,60).

**Coste**: convertir 12 MP de píxeles variados cuesta **17 ms** (unos 55 ms con 37 MP).

**Límites**: el gamut mayor que sRGB se recorta al convertir (la salida es sRGB de 8 bits);
no se aplica el perfil del monitor (se supone sRGB); perfiles RGB basados en tablas no se
convierten (se conservan); BT.2020/HDR no se tratan; la conversión CMYK sigue siendo simple.

---

### 4.13 Efectos: de 17 a 43 (réplica y mejora de PhotoScape X Pro)
El catálogo ya no vive todo en `Effects.cpp`: cada **familia** tiene su archivo con dos
puntos de entrada (`addXSpecs` y `renderX`) y `applyEffect` los consulta antes que a los
efectos antiguos.
- `EffectsColor`: blanco y negro (mezclador de canales con presets, tinte neutro en
  luminancia y limitado al gamut), borrar niebla (canal oscuro con filtro guiado rápido),
  mejorar documento, aberración cromática (radial, d(r)=A·4u(1−u)) y celofán.
- `EffectsPattern`: **Semitono** de imprenta (pantallas por canal a 45°/22,5°/0°; el semitono
  anterior pasó a llamarse **Punteado**).
- `EffectsDecor`: líneas, círculos concéntricos, velocidad radial, relleno de degradado, relleno
  de motivo (22 motivos), línea de borde (8 estilos de trazo).
- `EffectsLight`: bokeh, destello de lente, fuga de luz, rayos, polvo, destellos, resplandor
  y foco, dibujados **proceduralmente** (sin imágenes con licencia) sobre una capa reducida
  (≤1 000–2 000 px) que se superpone con la mezcla *Trama* (exacta para Screen).
- `EffectsGeometry`: **Estirar** y **Perspectiva**, que **cambian el tamaño** de la imagen.
- `Blend.{h,cpp}`: los 22 modos de fusión de PhotoScape para los efectos decorativos.
Infraestructura nueva: parámetros de tipo opción/interruptor/color/semilla con visibilidad
condicional (`dependsOn`), **preajustes**, **tiradores sobre el lienzo** (`EffectHandles.qml`:
punto o línea arrastrable en vez de un slider), `changesSize` + `effectOutputSize()` (la vista
previa se calcula sobre la copia de 1 600 px, se estira al tamaño final y el lienzo se
reajusta), `hidden` (efectos que maneja otra herramienta: `lens` y `frame`) y
`kMaxEffectParams = 24`. Las fórmulas de PhotoScape se **midieron** sobre imágenes
calibradas y se reprodujeron: B/N exacto, perfil de la aberración, desplazamientos del
celofán, retícula del semitono, ancho y tamaño de la perspectiva. Pruebas:
`test_effect_families` (34).

### 4.14 Lente (corrección con la base de Lensfun)
`core/lens/LensDatabase` lee los XML de **Lensfun** (56 archivos, 5 MB, embebidos como recursos
Qt; solo los datos, no la biblioteca) y calcula la corrección en unidades de Hugin (r = 1 a
medio lado corto): distorsión `ptlens`/`poly3`/`poly5`, aberración cromática `poly3`, viñeteo
`pa`; interpola entre distancias focales como Lensfun (spline sobre 1/f, IDW para el viñeteo) y
elige el juego de calibración según el factor de recorte. `MetadataReader` ahora devuelve el
objetivo (`Exiv2::lensName` y claves de notas del fabricante) y la distancia al sujeto: la
herramienta reconoce cámara y objetivo sola y deja elegirlos a mano (con buscador). Detrás hay
un efecto oculto `lens` (`EffectsLens.cpp`: remapeo con interpolación bilineal, aberración por
canal, ganancia de viñeteo, y «quitar los bordes vacíos» que recorta lo justo para ambos signos
de la distorsión). Modo «A mano» con tres deslizadores para objetivos fuera de la base. Los 16
valores del efecto salen de `correctionValues()`/`manualValues()` (en el núcleo, con pruebas).
Licencia de los datos: **CC BY‑SA 3.0** (aviso en `LICENCIAS.md` y en el generador de avisos).
Pruebas: `test_lens` (11).

### 4.15 Marco (Recortar > Marco)
Segunda página de la herramienta Recortar sobre el efecto oculto `frame` (`EffectsFrame.cpp`):
recorta la foto con una **forma** (rectángulo, elipse, hexágono, octágono, rombo, triángulo,
estrella, corazón; todos polígonos con esquinas tratables), esquinas **redondas, suaves, cortadas
o cóncavas** con redondez en %, cada esquina del rectángulo activable por separado, **contorno**
afuera o adentro (color, opacidad), margen, **sombra** (color, distancia, desenfoque, ángulo) y
fondo (color, degradado, transparente o la propia foto desenfocada). La imagen crece para que
todo entre (o «Mantener el tamaño» achica la foto); el lienzo no pasa de 100 MP ni de 20 000 px.
Todo en % del lado corto, así que la vista previa de 1 600 px y el resultado final coinciden.
Se pinta con **una sola capa del tamaño del lienzo**: foto, borrado de lo que queda fuera de la
forma (borde antialiasado por cobertura), contorno interior con `SourceAtop`, y después —por
detrás, con `DestinationOver`— contorno exterior, sombra (capa pequeña desenfocada y ampliada) y
fondo; pintar por detrás evita la línea clara que dejan dos bordes a medias. Pasar de la página
Recorte a Marco aplica antes el recorte o el enderezado pendientes; Cancelar deshace todo.
Pruebas: `test_frame` (20).

### 4.16 Varias imágenes
Botón nuevo en la barra superior: de 2 a 6 imágenes a la vez (`MultiView.qml`, `PaneView.qml`).
Cada panel guarda su vista como **zoom relativo al ajuste** y **punto de la imagen (fracciones
de ancho y alto) en el centro del panel**, valores que sirven para cualquier imagen: fotos de
distinto tamaño o proporción miran el mismo lugar. Con el zoom **vinculado** (por defecto) lo
que se hace en un panel lo siguen los demás; se desvincula con la cadena; **Shift** hace lo
contrario del vínculo solo durante ese gesto; al volver a vincular los demás alcanzan al
último panel usado. Ajustar todas, 100 % (cada una en sus píxeles reales), agregar/quitar
paneles, apilados o lado a lado, soltar un archivo sobre un panel. Las imágenes llegan por
`image://pane/` (`PaneImageStore`): los mismos decodificadores que el visor (RAW, HEIC, AVIF,
giro EXIF, sRGB), caché de 384 MB y tamaño real de cada archivo; un panel pide una copia de
2 048 px y, al acercar mucho, otra más nítida (hasta 16 384 px con dos paneles, 6 144 con más
de cuatro). Pruebas: `test_pane_images` (7).

### 4.17 Animaciones: crear GIF, APNG y WebP animado
`core/anim`: **GIF propio** (cuantización por corte de mediana sobre un histograma de 5 bits,
tramado Floyd‑Steinberg opcional, paleta global o por fotograma, solo el rectángulo que cambió
—el resto transparente sobre el fotograma anterior—, LZW de 12 bits con reinicio de tabla,
extensión de repeticiones; con transparencia cada fotograma se pinta solo sobre fondo
limpio), **APNG propio** sobre el PNG de Qt (`acTL`/`fcTL`/`fdAT`, solo la parte cambiada) y
**WebP animado** con `WebPAnimEncoder` de libwebp (`libwebpmux`, que ya viajaba con libvips; se
verificó que `cmake --install` lo despliega y que la carpeta instalada arranca). Los
codificadores piden los fotogramas de uno en uno a un `FrameSource`, así que 500 fotogramas no
están a la vez en memoria; todos admiten cancelación. `Compose.cpp` arma los fotogramas: ajustar
al lienzo (entera con fondo / llenar / estirar), transiciones (fundido, deslizar ×4, zoom con
suavizado), invertir, ida y vuelta, velocidad y duración mínima de 20 ms. `AnimStudio` (modelo de
lista + mapa de opciones + proveedores `animprev` y `animsrc`) y `qml/GifStudio.qml`: tira de
imágenes (mover, duplicar, quitar, duración por fotograma), vista previa con los mismos
fotogramas que se guardarán, exportación en segundo plano con progreso y archivo atómico. Un
GIF/APNG/WebP animado que se agregue se desarma en sus fotogramas con sus tiempos. El visor
ahora también **reproduce WebP animado**. Pruebas: `test_anim` (20: el GIF se valida leyéndolo
con el decodificador de Qt, el APNG con `AnimatedDecoder`, el WebP con libwebpdemux) y
`test_anim_studio` (12).

**Calidad del GIF.** La paleta sale de cortes por la mediana y se afina con unas vueltas de
k‑means sobre el histograma (los fondos planos quedan exactos y los degradados oscuros no se
rompen); la tabla de búsqueda usa los colores realmente vistos en cada casilla y el error del
tramado Floyd‑Steinberg se limita a ±24 por canal, porque con una paleta que no tenía nada cerca
de un color el error crecía de píxel en píxel y llenaba la imagen de puntos de colores ajenos
(verdes, en sombras suaves de imágenes con transparencia). Pruebas: `test_anim`
(`aNeutralDarkGradientStaysNeutralInTheGif`, `transparentPicturesShowTheBackground...`).

**Efecto y texto por fotograma.** Cada imagen de la tira puede llevar un `FrameStyle`
(`core/anim/Decorate.cpp`): un efecto del catálogo de Efectos (uno de sus *presets* o los valores
de fábrica, con «Cantidad») y un texto (varias líneas, tipo de letra, tamaño en % del lado corto,
posición en % del lienzo, color, negrita y contorno). Se aplica **después de ajustar la imagen al
lienzo**, así que el texto viaja con la imagen en las transiciones y todo es independiente de la
resolución (las distancias de los efectos ya son relativas). Solo valen los efectos que no
cambian el tamaño, no son ocultos y no necesitan tamaño completo (`effectUsableOnFrames`). El
texto se achica si no cabe a lo ancho y nunca sale del cuadro. La vista previa guarda los
fotogramas ya decorados (la firma del estilo entra en la clave de la caché). «A todos» copia el
estilo y la tira marca con «Fx» los fotogramas que lo tienen. Prueba en `test_anim` (decorar) y
`test_anim_studio` (modelo y exportación).

### 4.19 Cambios del 6 de octubre
- **Barra superior**: «GIF animado» y «Collage» son botones sueltos; «Exportar por lote» y «Renombrar por lote»
  pasaron al menú «⋯» (`Toolbar.qml`).
- **Comparador** (`MultiView.qml`): hasta 10 paneles y tres disposiciones. La automática prueba todas las
  repartos en filas (1…n filas, cada fila con las imágenes que le tocan) y se queda con el que cubre más
  superficie al encajar cada imagen en su celda según su proporción (`plan()`/`relayout()`); se recalcula al
  cambiar la cantidad, el tamaño de la ventana o cuando se conoce la forma de una imagen.
- **Leyenda (meme)** (`EffectsCaption.cpp`, efecto «caption» del grupo Acabado): primer efecto con **texto**.
  `EffectOp` lleva un `QString text`, `applyEffect`/`effectOutputSize` lo reciben y `AppController` lo expone
  (`effectUsesText`, `effectText`, `setEffectText`). Todo se mide en % del ANCHO de la imagen (la vista previa
  de 1600 px y el original dan lo mismo); el texto se ajusta al ancho con `QTextLayout` y la franja crece lo
  que haga falta, por eso cambia el tamaño. Modos: franja arriba/abajo o texto sobre la imagen (contorno por
  desplazamientos en círculo). Cuatro preajustes (meme de franja blanca, franja negra, estilo Impact, subtítulo).
- **Cuentagotas** (`ColorPicker`): `altDown()` se consulta cada 50 ms mientras el mouse está sobre la imagen
  (apretar Alt no manda ningún evento de mouse), el cursor se cambia con `QGuiApplication::setOverrideCursor` y el
  clic copia `#RRGGBB` (`#RRGGBBAA` si no es opaco). La lectura de píxel muestra HEX en lugar de RGBA y aparece
  también con Alt aunque el modo pixel esté apagado.
- **Leyenda en el estudio de GIF**: `FrameStyle.caption` (-1 = texto libre; 0..3 = un preajuste de la leyenda) y
  `renderFrame()` (Decorate.cpp): calcula cuánto mide la franja con `effectOutputSize("caption")`, encaja la imagen
  en lo que queda del lienzo, le aplica el efecto del fotograma y recién después añade la franja y el texto, así
  el lienzo conserva su tamaño y la imagen no se tapa. `captionValues()` (Effects.h) arma los valores sin el panel.
  Si el texto no cabe se achican las letras hasta que la franja ocupe menos del 55 % del alto.
- **Estirar**: mover una guía mientras su Horizontal/Vertical está en 100 % pone esa dirección en 150 %
  (`AppController::setEffectValue`); sin eso, la guía no mostraba nada.
- **Zoom suavizado** (`AppSettings.smoothZoom`, por defecto apagado; con «Zoom en números enteros» del modo pixel
  también se desliza, hacia el siguiente paso entero): la rueda solo mueve un objetivo y una
  `FrameAnimation` se acerca a él geométricamente (constante de tiempo ~60 ms) manteniendo bajo el cursor el
  punto elegido (guardado como posición en la ventana, porque el contenido se mueve mientras tanto).
- **Arrastrar y soltar** en el estudio de GIF vale en toda la ventana y las rutas llegan como texto; el diálogo de
  archivos y los *drops* de GIF y Collage convierten las URL antes de pasarlas a C++.

### 4.18 Collage
`core/collage`: el collage son **celdas rectangulares** (`Cell`: rectángulo normalizado +
`Content`) y un `Style`. Cada foto se mueve, se acerca **sin límites** (también por debajo de la
celda: se ve el fondo), se gira, se espeja y, si se pide, **sale de su celda** y pasa por
encima de las vecinas (se dibuja al final). Hay hasta 17 diseños de fábrica por cantidad de
fotos (filas, columnas, «grande a un lado», principal y filas) más mosaicos por cortes al azar con
semilla. Las **líneas divisorias** se *detectan* en cualquier conjunto de rectángulos
(`findDividers`: bordes que coinciden, unidos donde se tocan) y arrastrar una mueve el borde
compartido de todas las celdas que toca (`moveDivider`, con tamaño mínimo); el modo **Libre**
deja mover y redimensionar cada celda por su cuenta, con superposición. Estilo: separación,
margen, esquinas redondeadas, borde, sombra, relleno de celda, fondo (color, degradado o la foto
desenfocada, o transparente); todo en % del lado corto. Cada foto se pide con la resolución que
hace falta (copia de prueba de 512 px para conocer la proporción y una más nítida si el
zoom la necesita). `CollageStudio` + `qml/CollageMaker.qml`; guarda PNG, JPG o WebP en segundo
plano y cancelable. Pruebas: `test_collage` (16) y `test_collage_studio` (16).

**Plantillas.** «Mis plantillas» (en Diseño) guarda el diseño actual con un nombre: los
rectángulos de las celdas, el giro/espejo/«sale de su celda» de cada una y todo lo que define
cómo se ve (separación, margen, esquinas, borde, sombra, fondo, tamaño, modo Libre), pero **no
las fotos** ni el formato de guardado. Viven en `collage-templates.json` dentro de la carpeta de
datos de la aplicación (`AppDataLocation`), se escriben de forma atómica y se pueden borrar con
la cruz de la miniatura. Al aplicar una, las fotos que ya están se quedan en su orden dentro
de las celdas de la plantilla (si hay menos celdas que fotos, las últimas se sueltan).

## 5. Concurrencia, rendimiento y memoria

- `ImageLoader`: carga asíncrona con pool propio; precarga vecinas. La cancelación es
  *cooperativa* (una librería no se puede interrumpir a medias): ver 4.10.
- El guardado corre en un **pool propio de un hilo** (`m_savePool`, 4.11); el destructor lo
  espera.
- Los cálculos de efectos corren en un **pool propio** de `AppController`
  (`m_effectPool`); el destructor activa la bandera de cancelación y espera a **todos**
  (antes esperaba 3 s sobre el pool global, compartido con otros trabajos, y un trabajador
  que sobreviviera conservaba un `this` colgante).
- Miniaturas: `QQuickAsyncImageProvider` + `ThumbnailCache` (memoria thread‑safe y caché
  en disco bajo `CacheLocation/thumbnails`; la clave incluye la fecha de modificación).
- `forEachRowParallel` / `forEachBandParallel` (`ParallelRows.h`): reparten filas o
  bandas; el primero cae a ejecución serie si hay menos de `hilos×32` filas.
- Histograma de salida y miniaturas de looks/efectos sobre **proxies** pequeños.
- Decodificación de vista previa con *shrink‑on‑load* de libvips.

### 5.1 Memoria medida (foto de 37 MP, 7720×4824; 32 GB de RAM en la máquina)
| Momento | Memoria residente | Memoria comprometida |
|---|---|---|
| Recién abierta | 704 MB | 1 620 MB |
| Con el editor y la cadena GPU activa | 716 MB | 1 628 MB |
| Gaussiano en vista previa (etapa 1 + exacta) | 1 005 MB (pico 1 287 MB) | 1 948 MB |
| Tras aplicar 2 efectos, 2× deshacer, 2× rehacer | 1 009 MB | 2 042 MB |
| Efecto pesado (movimiento) en vista previa | 1 158 MB (**pico 1 548 MB**) | 2 190 MB |
| Tras cancelarlo | 1 016 MB | 2 245 MB |

Un búfer RGBA de esta foto ocupa ~142 MiB, así que 704 MB equivalen a unos 5 búferes
completos solo por abrirla. Aplicar, deshacer y rehacer no hacen crecer el consumo de
forma sostenida. Atribuir el consumo inicial a copias concretas de la RAM requeriría un
perfilador; es una hipótesis, no un dato.

**Memoria de la GPU (medida en la quinta ronda)**, con los contadores `GPU Process Memory`
de Windows sobre el programa real (AMD RX 6750 XT, Direct3D 11):
| Foto | Memoria dedicada de GPU | Compartida |
|---|---|---|
| 0,64 MP (1000×640), recién abierta | **55 MB** | 9 MB |
| 37 MP, recién abierta (solo visor) | **949 MB** | 149 MB |
| 37 MP con el panel de edición, Ajustes | 953 MB | 149 MB |
| 37 MP con exposición y un filtro aplicados | 957 MB | 149 MB |
Es decir, **~950 MB para una sola foto de 37 MP (≈6,1 veces su búfer RGBA de 149 MB)** y
**casi no cambia al editar**: la cadena de tres pasadas de shaders reserva todo desde que
se abre la imagen, aunque no haya ninguna edición. Desglose **estimado** a partir de la
estructura de `ImageCanvas.qml` (no medido textura a textura): imagen con *mipmaps* ≈199 MB,
imagen original con *mipmaps* (solo se usa al mantener «Comparar») ≈199 MB, salida del filtro
≈149 MB, salida de Ajustes con *mipmaps* ≈199 MB; ~750 MB, y el resto serían la cadena de
presentación y el controlador.

**Reducción (implementada después de esa medición).** Dos cambios en `ImageCanvas.qml`:
1. La imagen original solo se carga mientras se mantiene «Comparar»
   (`originalImage.source` vale `""` el resto del tiempo).
2. Una etapa sin efecto se apaga y libera su textura: `AppController::lookActive()` (hay un
   filtro con intensidad > 0) y `gradeActive()` (los controles de tono y color de Ajustes no
   son la identidad; claridad, nitidez, viñeta y grano pertenecen a la etapa Detalle, que
   siempre corre y no cuenta). Cada etapa apagada tiene `visible: false` y su
   `ShaderEffectSource` pasa a `sourceItem: null`; `detailEffect` lee la etapa más reciente
   que sí corre (o la imagen sin más). Los *mipmaps* se piden solo a la fuente que Detalle
   muestra reducida. `ParityScene.qml` replica el cambio y `test_gpu_parity` añade
   `untouched` y `backToNeutral` (apagar y volver a encender etapas no deja restos).

Medido con el programa real, la misma foto de 37 MP (JPEG sintético 7360×5000):
| Estado | Antes | Ahora |
|---|---|---|
| Recién abierta / panel abierto, sin ediciones | 949–953 MB | **233–241 MB** |
| Con un ajuste de tono o color (exposición) | ~953 MB | **618 MB** |
| Con filtro **y** ajustes a la vez (peor caso) | 957 MB | 953 MB (igual) |
| «Restablecer» (vuelve a neutro) | — | 241 MB: la memoria se libera |
| Pasar entre dos fotos de 37 MP | — | pico 243 MB (muestreo cada 0,5 s) |
No se midió el estado con «Comparar» pulsado. Queda sin hacer una versión reducida para
pantalla (4–6 K) que cambie a la completa al acercarse al 100 %, que bajaría también el
peor caso.

**Modo edición bajo demanda (sexta ronda).** El panel de edición (sliders, curvas, niveles,
38 filtros, efectos) ya no se construye al arrancar: `Main.qml` lo crea con un `Loader` solo
al pulsar «Editar» y lo destruye al salir. `Main.qml` es dueño de `editMode`
(`setEditMode(on)`); `Toolbar`, `StatusBar` (antes `FloatingToolbar`) e `ImageCanvas` reciben `editMode` y emiten
señales (`editModeRequested`, `saveAsRequested`, `closeEditRequested`) en vez de tocar el panel.
Las ediciones viven en `AppController`, así que salir del modo edición no las pierde. Medido
solo visualizando una foto pequeña (misma máquina, 7 s tras abrir; el ejecutable «antes» es la
instalación de prueba previa, que tampoco tenía el ahorro de pasadas GPU de arriba):
RAM 138 → 119 MB (privada 172 → 129 MB), GPU 68 → 43 MB.

**Ajustes de interfaz posteriores.** Cuarto tema «Windows 98 Oscuro» (misma `skin` `win98`, paleta oscura; `ThemeManager::fieldFace()` da el fondo de los campos hundidos). Reacción al ratón en botones, pestañas, casillas, listas desplegables, cuadros numéricos, miniaturas, cabeceras de sección y celdas de filtros. Los botones primero y último de la barra flotante siguen la curvatura de la cápsula (`AppToolButton/AppButton.edge`, solo temas modernos). Al salir del modo edición se cancelan la vista previa de «Enderezar» y sus guías. Mientras la imagen está ajustada a la ventana, `ImageCanvas` la re-centra cada vez que cambia el tamaño del contenido (corrige el salto tras girar 90°).

**Herramientas de edición como transacciones (séptima ronda).** El panel de edición es ahora
una lista de herramientas (Recortar, Tamaño, Ajustes, Filtros, Efectos). Al elegir una, su
página se desliza desde la izquierda sobre la lista y solo se sale con **Aplicar** (conserva) o
**Cancelar** (deshace todo lo hecho en ella). Cada herramienta es un archivo
(`CropTool/ResizeTool/AdjustTool/FilterTool/EffectsTool.qml`) que solo existe mientras está
abierta y ofrece `canApply`, `apply()` y `cancel()`. La exactitud de Cancelar la da el
historial: `AppController::beginToolSession()` recuerda la posición de la pila y
`endToolSession(keep)` deshace hasta ella y descarta lo rehacible (`EditStack::undoPosition()`,
`truncateRedo()`); dentro de la sesión Deshacer no pasa del punto de entrada y el primer cambio
de un slider no se fusiona con uno anterior. Mientras hay una herramienta abierta `Main.qml`
(`toolLocked`) desactiva salir del modo edición, cambiar de foto (miniaturas, flechas, abrir,
soltar archivos), guardar, restablecer, borrar y exportar/renombrar por lote; la barra flotante
solo deja Comparar, Deshacer y Rehacer. Aplicar en Recortar aplica un enderezado pendiente
o, si no lo hay, el marco de recorte; los giros y volteos se aplican al pulsarlos pero cuentan
como parte de la sesión (Cancelar los deshace). 6 pruebas nuevas en `test_app_controller`.

**Ronda de pulido de las herramientas (octava ronda).**
- *Efectos nuevos:* **Negativo**, **Viñeta** (6 parámetros: cantidad con signo, tamaño, suavidad, redondez, centro X/Y) y **Grano y ruido** (cantidad, tamaño del grano, tipo, monocromático) viven ahora en Efectos (horneados, con su deshacer), grupo «Acabado». Se quitaron de la interfaz de Ajustes y Filtros («Acabado», «Grano y ruido» y la casilla «Negativo»); **el motor de los sliders antiguos** (`AdjustOp.vignette/grain/negative`, shaders y su paridad) **sigue en el código sin interfaz**: es candidato a eliminarse. Los parámetros de un efecto pasan de 3 a `kMaxEffectParams = 6` (`EffectValues`) y `EffectParam` admite `options` (una elección entre varias, p. ej. el tipo de ruido).
- *Ajustes:* la temperatura se muestra en kelvin (6500 K neutra, 2000 K calienta, 12000 K enfría) con el slider invertido (izquierda = naranja/cálido, derecha = azul/frío) y el surco pintado con esos colores; «Matiz» lleva un degradado magenta ↔ verde (`AppSlider.trackColors`, `AdjustRow.inverted/formatter`).
- *Recortar:* la proporción activa queda resaltada (`AppButton.checked`), botón «Rotar a la izquierda» (`RotateOp{270}`), «Enderezar» con cero magnético de 3° y marca en el surco, y las guías solo aparecen mientras la inclinación no es 0. *Tamaño:* el slider de escala llega a 120 %, el campo admite hasta 250 %.
- *Disposición:* Cancelar / Aplicar (se probó el orden inverso y se volvió a este); «Cantidad» de Filtros fija encima de los botones; el texto de ayuda de Efectos arriba de las categorías; la página de una herramienta entra y sale por la derecha; los parámetros de un efecto largo se desplazan dentro de su zona.
- *Cursor en zonas no clicables:* causa real: la página de la herramienta no cerraba el ratón a la lista de herramientas que tenía debajo, cuyas entradas (con cursor de mano y clic) seguían «viendo» el ratón a través de las zonas vacías. Ahora la página y la barra flotante son sólidas al ratón. Verificado con un mapa del cursor sobre todo el panel (antes: mano en toda zona vacía; después: ninguna).

**Pulido de barras, menús y diálogos (novena ronda; la barra flotante de esa ronda es hoy `StatusBar`, ver 21).** Barra superior: se eliminó el menú «⋯»; quedan Editar, Exportar por lote, Renombrar por lote, Información y Configuración, todos con icono (copiar, pegar, fondo y eliminar siguen en el menú contextual). Barras flotantes: todas usan el mismo estilo plano de `AppToolButton` (antes el modo edición mezclaba botones con borde), con más aire horizontal (`hPad`, `outerPad` en los extremos). Menú contextual (`AppMenu`/`AppMenuItem`): tarjeta redondeada con sombra, iconos por opción y variante `danger` para eliminar. Diálogo de confirmación reutilizable `ConfirmDialog` (icono redondo, texto en dos niveles, botón «Eliminar» rojo; `AppButton.primary/danger`) para eliminar archivo. Configuración: tarjetas por tema, interruptores (`AppSwitch`) en lugar de casillas y una línea de ayuda por página. La rueda de carga se sustituyó por `AppLoadingPill`: una píldora que se llena (estimación, el decodificador no informa el avance; aparece pasados 250 ms y se completa al llegar la imagen). `FloatingToolbar.updateEnds()` ahora solo reasigna los bordes que cambian: con el relleno extra, reasignar todos provocaba un bucle de `polish()`.

**Exportar y renombrar por lote con selección (décima ronda).** Los dos diálogos abren una ventana con la lista de archivos de la carpeta (`BatchFileList`: casilla, miniatura y nombre; filtro por nombre; Todos / Ninguno / Invertir sobre lo visible; Mayús + clic para un tramo) y las opciones al lado. *Exportar* usa solo lo marcado, muestra el resultado al terminar y `BatchExporter` ya no escribe encima del original ni pisa a otro archivo del mismo lote con el mismo nombre base (añade `_2`, `_3`…). *Renombrar* arranca sin nada marcado, enseña «nombre actual → nombre nuevo» antes de tocar nada y explica por qué se rechazaría (nombre vacío o ilegal, o un nombre ya ocupado por un archivo no elegido): `FolderModel::checkRename` / `renameFiles`, en dos fases y **todo o nada** (si un paso falla se devuelve todo a como estaba; `renameAllInFolder` desapareció). 8 pruebas nuevas (`test_folder_model`).
**Miniaturas con transparencia.** Causa: la caché de disco guardaba todas las miniaturas como JPEG, que no tiene alfa: al leerlas de nuevo (otra sesión) los recortes con fondo transparente mostraban su color oculto. Ahora las que tienen píxeles translúcidos se guardan como PNG y la clave lleva un prefijo `v2|` que deja sin uso los archivos antiguos (`test_thumbnail_cache`, 2 casos; falló antes del arreglo). Otros cambios: los temas Windows 98 se llaman «Classic Claro» y «Classic Oscuro» (ids internos sin cambio), el marco de selección de las tarjetas de tema se dibuja encima del dibujo (antes lo tapaban sus barras) y se quitó el botón de pausa de los GIF.

**Modo pixel (undécima y duodécima ronda).** En Configuración > Apariencia hay una tarjeta «Modo pixel» con un interruptor y cinco extras, cada uno con el suyo (`AppSettings`: `pixelMode`, `pixelIntegerZoom`, `pixelCheckerboard`, `pixelGrid`, `pixelReadout`, `pixelSharpEdit`; claves `view/pixel*` en el `.ini`). Todo está apagado por defecto y los extras solo cuentan mientras el modo está encendido (en la ventana aparecen atenuados e inertes si no). **El modo en sí** solo quita la suavización: los píxeles se ven nítidos desde el 100 % de zoom (`ImageCanvas.nearest`: `smooth: false` en la imagen y en los `ShaderEffectSource`; al reducir sigue con mipmaps) y el zoom llega a 64× (`topScale`). **Extras:** (1) *zoom en números enteros* (`ImageCanvas.integerZoom`, `pixelSteps`; el «ajustar» de una imagen pequeña es el mayor entero que cabe); (2) *fondo de ajedrez* bajo la imagen (`Checker.frag`); (3) *cuadrícula* entre píxeles desde 8× (`PixelGrid.frag`); (4) *lectura del píxel*, abajo a la izquierda (`AppController::pixelAt` sobre `ImageProvider`); (5) *editar sin suavizar*: «Tamaño» redimensiona por vecino más cercano (`ResizeOp.resampleFilter == "nearest"`, `resizeNearest`, escala hasta 800 % y campo hasta 3200 %) y «Enderezar» gira sin suavizar (`RotateOp.smooth = false`). **Migración:** el modo nació como un único interruptor (`view/pixelArt`); quien lo tenía encendido conserva todos los extras encendidos (cada extra toma por defecto ese valor mientras no tenga clave propia). Los ajustes en vivo (Ajustes/Filtros) y las miniaturas no cambian. Pruebas: 6 de remuestreo/lectura de píxel y 9 en `test_app_settings` (valores por defecto, persistencia uno a uno, señales, migración).

### 5.2 `VipsGuard`
Un mutex global serializa **toda** entrada a libvips tras haberse visto corrupción de
memoria intermitente en glib/gio con llamadas concurrentes (firma en glib/gio/gobject,
misma dirección de fallo, probablemente una inicialización perezosa de GIO). La
revisión externa pide investigar la causa en lugar de tomar el mutex como solución
definitiva, con razón: además **serializa la decodificación a tamaño completo con las
miniaturas**. No se ha investigado todavía (sección 9). Si `vips_init()` falla, ahora se
devuelve un error de decodificación/guardado (antes solo había un `qWarning` y se seguía
llamando a una librería sin iniciar).

---

## 6. Interfaz — decisiones que conviene conocer

- Barra flotante tipo "píldora" (cuadrada en Win98); en modo edición muestra
  Comparar/Deshacer/Rehacer y Cancelar/Aplicar recorte o Restablecer/Guardar/Guardar
  como/Cerrar.
- Barra lateral colapsable que termina donde acaban las miniaturas; las miniaturas se
  deslizan (no escalan) al abrir. HUD de zoom/índice junto al aviso del recorte.
- Rueda del ratón normalizada en menús desplazables (110 px por muesca).
- Controles propios: `AppSlider` con tope en el valor neutro y en 100 %, etc.
- Diálogos nuevos: «Sobrescribir el original» y «Cambios sin guardar» (ambos con
  los tres temas); atajo Ctrl+S.
- Edición desactivada en imágenes animadas. Errores de decodificación como aviso rojo.
- Iconos de la ventana corregidos (el recurso estaba en otra ruta).

---

## 7. Calidad y verificación

**Pruebas automáticas** (QtTest, 430 casos contando `initTestCase`/`cleanupTestCase`, + humo;
`ctest` ejecuta 20 programas; los de lo añadido el 4‑5 de octubre están al principio de esta lista):
- Lo añadido el 4‑5 de octubre: `test_effect_families` (34), `test_lens` (11), `test_frame` (20),
  `test_pane_images` (7), `test_anim` (19), `test_anim_studio` (11), `test_collage` (16),
  `test_collage_studio` (15); y en `test_app_controller` (35) el flujo de un efecto que cambia el
  tamaño y el del marco (vista previa, cancelar, aplicar, deshacer).
- `test_color_management` (16): P3 y AdobeRGB frente a matemática de referencia independiente,
  sRGB sin tocar (incluido el perfil de Windows), perfiles inválidos o CMYK rechazados, alfa
  intacto, una imagen compartida no se modifica, coste.
- `test_decoder_registry` (53): decodificador elegido por extensión; **un archivo real de
  cada formato que se anuncia** (PNG, JPG, BMP, TIFF, WebP, GIF, ICO, HEIC, AVIF) en
  tamaño completo y reducido; **las 8 orientaciones EXIF**; **nombres Unicode** (acentos,
  cirílico, japonés, coreano, emoji); que el archivo **no queda bloqueado** tras
  decodificar; que no se anuncian formatos que no abren; **modelos de color**: PNG/TIFF de
  16 bits (RGB y gris), gris con alfa y un JPEG CMYK real (colores y sin agujeros de alfa);
  **perfiles de color**: Display P3 en PNG 8/16 bits, JPEG, TIFF, HEIC, AVIF (ICC y nclx)
  convertidos a sRGB, la vista previa también, sRGB intacto, perfil inutilizable conservado.
- `test_image_writer` (35): ida y vuelta por formato con PSNR; **calidad JPEG nueva frente a
  la de Qt**; calidad explícita; nombres Unicode al guardar; transparencia; **guardado
  atómico** (un fallo deja el original intacto y sin temporales); **conservación de
  metadatos** (cámara, ISO, apertura, GPS, fecha, orientación 1, dimensiones, perfil ICC y
  sin doble rotación); **EXIF + XMP + IPTC en JPEG, EXIF + XMP en PNG y las cuatro
  conversiones JPEG/PNG ↔ WebP**, comprobados leyendo el archivo guardado con exiv2; aviso
  cuando el origen es HEIC (y ninguno para un GIF); sobrescribir el propio original; fuente
  de metadatos ilegible; **color**: los archivos guardados llevan perfil sRGB y nunca el del
  original (JPEG/PNG/WebP/TIFF), el trozo `iCCP` del PNG es válido, el perfil pendiente se
  adjunta, y P3 → guardar → reabrir no vuelve a convertir.
- `test_edit_stack` (63): identidad, tabla de tonos, ajustes, looks, ruido (σ y curtosis),
  remuestreo, los **efectos** (forma, fuerza cero, bordes, transparencia, determinismo,
  mezcla, independencia de resolución, cancelación), **historial con `LiveOp`**
  (instantáneas, fusión de un gesto en un paso), **puntos de control** y su presupuesto;
  que la reproducción sin caché del trabajador de guardado da **exactamente** lo que hornea
  la pila.
- `test_image_loader` (8): con un decodificador de mentira (lento/rápido/que falla) —la
  petición más reciente gana, un fallo viejo no se muestra, `cancel()` suprime, ids
  independientes, las peticiones ya sustituidas **no se decodifican**.
- `test_app_controller` (31): el `AppController` **real**: un fallo de carga conserva el
  documento y su ruta; la ruta cambia solo cuando llega la imagen; **guardar mientras otra
  carga no puede escribir sobre ese archivo**; lo editado durante una carga no se pierde;
  *Guardar → Deshacer* es «sin guardar», *Rehacer* lo deja limpio, una rama nueva es «sin
  guardar», ediciones que se anulan no lo son; **guardar confirma el efecto pendiente**;
  destruir el controlador a mitad de un efecto pesado es seguro y rápido; el aviso cuando el
  guardado pierde metadatos; **guardado asíncrono**: no congela (arranque < 150 ms y parón del
  bucle < 250 ms con 12 MP), lo editado durante la escritura sigue sin guardar, un segundo
  guardado se rechaza, abrir otra imagen espera, «Guardar» de la pregunta guarda y luego abre,
  un guardado fallido conserva las ediciones y descarta lo que esperaba, un efecto pendiente
  lo calcula el trabajador (arranque < 300 ms), no se puede eliminar el archivo mientras se
  guarda, y destruir el controlador a mitad de guardado deja el archivo completo.
- **`test_gpu_parity` (54)**: **paridad CPU↔GPU automática.** Renderiza con Direct3D 11 la
  cadena real de shaders (`tests/gpu_parity/ParityScene.qml`, copia de la de `ImageCanvas.qml`
  que lee el `AppController` real) y la compara, en RGBA premultiplicado, con el archivo que
  escribe la CPU. 24 ajustes (cada slider, tres looks, y combinaciones) × imagen opaca e
  imagen con alfa de 8 a 255. Tolerancias: ≤3 niveles de 255 los ajustes sueltos; más margen
  (5–17) para nitidez y combinaciones, porque cada etapa de la vista previa es una textura de
  8 bits y la nitidez multiplica el redondeo de la anterior. Se omite sola (`QSKIP`) si no
  hay dispositivo Direct3D 11. Estado anterior a la corrección de alfa, medido con esta
  misma prueba: 14 de 15 ajustes se desviaban hasta 247 niveles con transparencia.
- `test_animated_decoder` (9): GIF/APNG con fixtures.
- `test_app_settings` (11): el interruptor del Modo pixel y sus cinco extras —todo apagado por defecto,
  cada uno se recuerda por separado, una señal por cambio real— y la migración desde el interruptor
  único antiguo (corre contra la carpeta de configuración de prueba de Qt, nunca contra el `.ini` real), más el modo portable.
- **`smoke_startup`**: arranca el programa real sobre una foto, sin ventana visible
  (plataforma «offscreen», render por software), lo deja correr 6 s y **falla si muere o si
  escribe cualquier mensaje** a su salida de errores. Es justo la clase de problema que las
  pruebas unitarias no ven (los tres avisos de arranque que llevaban semanas en el log).
  Con control negativo: forzando mensajes de Qt, la prueba falla.

Regla seguida en esta ronda: **antes de arreglar, la prueba se escribe y se ejecuta contra el
código sin arreglar para ver que falla** (carga: 5 de 6; controlador: 7 de 12; alfa: 14 de
15 ajustes con transparencia; metadatos: 8 de 8 casos nuevos; modelos de color: 3 de 5).

**Verificación de paridad CPU/GPU, histórica** (manual, con scripts de PowerShell + NumPy
fuera del repositorio): captura del lienzo GPU al 100 % de zoom frente al PNG guardado por
la CPU. Ya está automatizada en `test_gpu_parity` (arriba, con imágenes opacas: máximo de
0–1 nivel salvo nitidez/combinaciones); estos números se conservan como medición con
fotos reales y con la ventana real.

| Elemento | Diferencia media | Máximo |
|---|---|---|
| Sliders de tono/color (etapas A+B) | ≤ 0,22 niveles | 7 |
| Nitidez sola | — | 1 |
| Niveles + curvas por canal + negativo | **0 (exacto)** | 0 |
| Claridad | 0,4–0,9 | ~14 (la disposición de *mips* de la GPU no es reproducible exactamente) |
| Looks (Cámara vieja, Atardecer, Cine, Sepia) | — | 1–3 |
| Ruido (4 tipos × mono/color) | 0,01–0,07 | p99 = 1 |

Condiciones: altura de lienzo **par** (con altura impar la imagen cae en medio píxel y el
ruido por píxel parece un fallo total); solo a 100 % de zoom.

**Lo que ha encontrado la verificación real (no unitaria)**: el export ignoraba los
controles nuevos de Ajustes; las filas de sliders de Efectos escribían todas el parámetro
0; tres avisos de arranque; y, en la ronda de la revisión externa, los errores de la
sección 8.

**Lo que NO está automatizado**: pruebas de interacción de la interfaz (los diálogos y el
historial de Ajustes se verificaron a mano con capturas; el aviso de metadatos y el estado
del botón Guardar, solo leyendo el QML); ejecutar todo esto en CI (no hay CI); análisis
estático; *fuzzing* de decodificadores; memoria de la GPU.

---

## 8. Revisión externa (ChatGPT, 2‑oct‑2026) contrastada con el código

El revisor recibió la versión anterior de este documento y pidió ver el código antes de
opinar a fondo. Aquí está **cada punto suyo, qué se comprobó y qué se hizo**. La regla fue
no fiarse de una opinión: cada afirmación comprobable se probó (muchas con una prueba
que debía fallar si el problema existía).

### 8.1 Sospechas que resultaron ser errores reales
| Punto de la revisión | Resultado de comprobarlo | Acción |
|---|---|---|
| **Guardado seguro** (sobrescribe, recomprime, pierde EXIF/ICC) | **Confirmado y peor de lo descrito**: JPEG a calidad ≈75, escritura no atómica, sin confirmación | Reescrito (sección 4.7) |
| **Orientación EXIF** | **Confirmado**: 7 de las 8 orientaciones salían mal. La causa de fondo: el libvips del build no lee EXIF | Corregido y con 8 pruebas (4.8) |
| **Unicode en `argv[1]`** | **Confirmado, y más amplio**: además `VipsDecoder` no abría **ningún** archivo con caracteres fuera de ASCII (ni «ñandú café.jpg») | Corregido ambos; probado de punta a punta |
| **Cambios sin guardar** | **Confirmado**: no existía ningún aviso al cerrar ni al cambiar de imagen | Diálogo y bloqueo en `AppController` |
| **x265 / libheif en vcpkg** | **Confirmado**: `libx265.dll` (GPL) se distribuía porque faltaba `default-features: false` | Corregido; HEIC/AVIF siguen abriéndose (sección 12) |
| **exiv2 GPL‑2.0‑or‑later** | **Confirmado** (0.28.8) | Sigue siendo el bloqueo legal principal (`LICENCIAS.md`) |

### 8.2 Hallazgos nuevos al contrastar (no los mencionaba la revisión)
- BMP, GIF estático e ICO **no se podían abrir**; PSD/TGA/JXL se anunciaban sin poder abrirse.
- Los `.webp` abiertos quedaban **bloqueados** en disco (caché de operaciones de libvips).
- El README anunciaba formatos que el build no soportaba.

### 8.3 Decisiones de diseño sugeridas y qué se decidió
| Sugerencia | Decisión |
|---|---|
| Mantener CPU como resultado autoritativo y GPU interactiva | De acuerdo; se mantiene. Se matiza el texto de «única fuente de verdad» (4.2) |
| **Undo transaccional de Ajustes/Filtros** | **Hecho** (4.1): un gesto = un paso, en el mismo historial cronológico |
| Efectos asíncronos: dejar igual y revisar 4 detalles | Revisados en el código (4.6); uno corregido (liberar buffers cancelados) |
| No confirmar el efecto pendiente al guardar | **Hecho**: se guarda lo que se ve sin tocar el historial |
| Checkpoints: caché LRU con presupuesto según memoria | **Hecho** (4.1) |
| Prueba de humo real de QML (fallar con avisos) | **Hecha**, con control negativo (7) |
| CI con paridad CPU/GPU automática | Pendiente (sección 9) |
| Modelo de documento antes de capas/pinceles | De acuerdo; pendiente de decisión (sección 9) |
| 16 bits e ICC (primero ICC) | **ICC hecho** (4.12: todo a sRGB al abrir, etiquetado al guardar); 16 bits solo se leen y se escalan a 8 |
| Fuzzing y causa de `VipsGuard` | Pendiente (5.2, sección 9) |
| Git, tooling dentro del repo, versiones fijadas | Parcial: `smoke_startup`, `make_third_party_notices.ps1` y fixtures ya están en el repo; **git no** en esa fecha (después sí: ver la actualización del encabezado) |

### 8.4 Cosas de la revisión que NO he podido verificar
- Estado actual de las licencias de patentes de HEVC y de que exiv2 no ofrezca licencia
  comercial: lo anoto como dicho por el revisor.
- Que ningún módulo de Qt usado sea solo GPL/comercial: lo deduje; hay que confirmarlo en
  la página de licencias de Qt.
- «El resultado de `qsb` no se vuelve GPL»: razonable, no verificado.

### 8.5 Valoración
Con solo la descripción del proyecto, el revisor señaló como sospechosas tres cosas que
las pruebas confirmaron como errores reales (EXIF, rutas Unicode, x265), y marcó bien
qué era lo más urgente (la integridad del archivo del usuario). Contrastarlo con el
código encontró, además, problemas que él no podía ver (sección 8.2).

### 8.6 Historial de decisiones anteriores y lecciones
- **Aero/XP eliminados (29‑sep)**: se construyó un desenfoque real "cristal" para Aero.
  Bajo carga de GPU concurrente (otra app/juego) la captura quedaba en blanco sin error ni
  forma fiable de detectarlo. El usuario decidió que no valía el coste y se quitaron ambos
  temas por completo.
- **Ajustes (30‑sep)**: arquitectura "una definición, dos renderizadores" y su
  verificación numérica (4.2 y 7).
- **Bug de curvas**: `QVariantList::append(QVariantList)` *concatena*, aplastando las
  listas; el editor de curvas se reiniciaba al soltar. Ahora se envuelve con
  `QVariant::fromValue`.
- **`Flickable` robaba arrastres**: niveles/curvas usan `preventStealing: true`.
- **Grano**: dos reescrituras por feedback visual (4.4). **Redimensionado**: de
  `QImage::scaled(Smooth)` a Lanczos‑3 propio por medición (4.5).
- **Iconos**: un bug de `Translate` vs `x/y` en el centrado de iconos `Canvas`; sistema de
  iconos propio en `AppIcon.qml`.
- **Tres avisos de arranque corregidos (1‑oct)**: el icono de la ventana buscaba una ruta
  de recurso equivocada (la ventana no tenía icono); el atajo de pegar usaba `sequence` en
  lugar de `sequences`; un bucle de enlace del `Flickable` del lienzo.
- **Reglas de trabajo adoptadas**: planificar los cambios grandes de UI y verificarlos con
  captura en cada paso; comprobar el tamaño real de la ventana y de la pantalla antes de
  fiarse de una captura o de un clic por coordenadas (la distribución de monitores cambió
  durante una sesión y los clics de prueba cayeron fuera de la ventana: se comprobó que
  dieron en el escritorio y no afectaron a nada); no tocar nunca instancias de la app que
  no se hayan lanzado desde la herramienta.

### 8.7 Segunda pasada: `vcpkg.json` y `CMakeLists.txt` revisados por el mismo revisor

Tras la primera tanda el revisor leyó los dos archivos de compilación. Cada afirmación
suya, contrastada:

| Afirmación | Comprobación | Resultado / acción |
|---|---|---|
| libheif sin `x265`: HEIC se decodifica con libde265, AVIF con aom | `libx265.dll` no está en el árbol de build ni en la carpeta instalada | **Cierto.** Se anotó el porqué en `vcpkg.json` (`$comment`, aceptado por vcpkg) y `tools/Test-Install.ps1` falla si aparece `libx265.dll` |
| exiv2 0.28.8 es GPL‑2.0‑or‑later y es lo único grave | Coincide con `LICENCIAS.md`; solo lo usan `MetadataReader.cpp` y `ImageWriter.cpp` | **De acuerdo.** Sin decidir (necesita elegir licencia). Hay una vía de sustitución concreta: `libvips[exif]` + libexif (ambos LGPL‑2.1+) en lugar de exiv2; **no está probada** (ver `LICENCIAS.md`, sección 3) |
| `builtin-baseline` fijado: dependencias reproducibles | Cierto | Sin cambios |
| **Qt no está fijado** (`find_package(Qt6 6.6)`) | **Cierto**, y `CMakeUserPresets.json.example` decía 6.7.0 mientras se compila con 6.7.3 | `IMAGEVIEWER_VALIDATED_QT = 6.7.3`: aviso con otra versión y **error** en el preset `windows-release` (`IMAGEVIEWER_REQUIRE_VALIDATED_QT=ON`). No se usó `EXACT` a secas para no romper máquinas de desarrollo con otra 6.x; el ejemplo ya apunta a 6.7.3 |
| El copiado manual de QtQuick.Effects con `../../../` es frágil | **Cierto en parte.** Probado: `windeployqt` con el escaneo de QML **sí** despliega `effectsplugin.dll` y `Qt6QuickEffects.dll` por sí solo | La carpeta de entrega ya no depende del copiado manual (la produce `cmake --install`). El copiado se mantiene solo para ejecutar desde el árbol de build y ahora usa `QT6_INSTALL_PREFIX/QT6_INSTALL_QML`, que define el propio Qt, en lugar de `../../..` (también en `tests/CMakeLists.txt`) |
| «El CMake no describe una app distribuible» (sin `install()`) | **Cierto** | Añadido (ver abajo) |
| «Debería existir `cmake --preset windows-release`» | **Ya existía** (`CMakePresets.json`), pero solo configuraba y compilaba | Añadidos presets de prueba y fijado `VCPKG_TARGET_TRIPLET=x64-windows` |
| ShaderTools solo como herramienta de compilación | De acuerdo | Sin cambios |
| ¿Enlace estático o dinámico? (importa para la LGPL) | **Respuesta**: el triplet es `x64-windows` (DLL). `imageviewer_core` es una biblioteca estática **propia**; libvips, LibRaw, libheif y exiv2 son DLL junto al `.exe` | Triplet fijado en los presets y documentado en `CMakeLists.txt` y `LICENCIAS.md` |

**Entrega reproducible (`cmake --install`).** Produce la carpeta completa de 93,8 MB:
`ProjectFoxy.exe`, Qt (por `windeployqt` a través del script de despliegue de Qt, que
encuentra QtQuick.Effects escaneando el QML; sin la DLL de OpenGL por software de 20 MB),
las **27 DLL de vcpkg que el programa necesita de verdad** (se resuelven con
`file(GET_RUNTIME_DEPENDENCIES)` sobre el ejecutable: no se copia toda la carpeta `bin`
de vcpkg, que trae además `jasper`, `spng`, `turbojpeg`, 32 `.pdb`…), el runtime de C++
(instalado por CMake, porque `windeployqt` lo busca con `vswhere.exe` y lo omite en
silencio si no está en el PATH) y `licenses/THIRD_PARTY_NOTICES.txt`.
**`tools/Test-Install.ps1`** instala en una carpeta temporal, arranca el programa
**instalado** con un PATH reducido a las carpetas de Windows (para que una DLL olvidada no
se encuentre por casualidad en el vcpkg o en el SDK de Qt) y falla si no arranca limpio o
si hay `libx265.dll`. Control negativo: sin `libde265.dll` el programa termina con
`0xC0000135` y la prueba falla.

Lo que sigue **sin** hacerse: instalador (Inno Setup/WiX), firma de código, CI y
asociación de archivos.

### 8.8 Cuarta ronda: auditoría técnica final (con todos los archivos)

El revisor leyó la documentación y unos 20 archivos de código (controlador, cargador,
pila de edición, escritor, decodificador, shaders, QML principal) y entregó una lista
priorizada (P0–P2). Cada punto, contrastado. **Lo comprobable se midió; en los P0 y varios
P1 la prueba se escribió primero y se vio fallar.**

| Punto (prioridad) | Comprobación | Resultado / acción |
|---|---|---|
| Carrera en `ImageLoader`: una carga vieja reemplaza a la nueva (P0) | `cancel()` solo borraba un id de un `QSet` que nadie consultaba | **Confirmado**: 5 de 6 pruebas fallaban (dos señales `loaded`, el fallo viejo se mostraba, `cancel()` no suprimía, las 22 peticiones se decodificaban). Corregido (4.10) |
| `m_currentFilePath` cambia antes de completar la carga (P0) | `loadPath()` lo fijaba al pedir | **Confirmado.** Corregido (4.10) |
| Guardar A sobre B (P0) | Escenario del revisor reproducido con un decodificador lento | **Confirmado: `saveEdited()` devolvía `true` y escribía los píxeles de A encima de `slow_b.png`.** Corregido: rechazo en el *backend* mientras carga (la interfaz lo refleja, pero la garantía es del controlador) |
| `isDirty()` no representa «distinto de lo guardado» (P0) | *Recortar → Guardar → Deshacer* | **Confirmado** (daba falso). `EditRecipe` (4.1). Detalle propio: el botón y el atajo Guardar quedaban desactivados en ese caso; ahora se habilitan |
| Guardar un efecto pendiente desincroniza disco e historial (P0) | Guardar con efecto pendiente, luego deshacer | **Confirmado.** Guardar confirma el efecto (4.6). Esto **revierte** una decisión de la segunda ronda, con la razón documentada |
| CPU/GPU divergen con alfa premultiplicado (P0) | Banco GPU real (`test_gpu_parity`) | **Confirmado y cuantificado**: 14 de 15 ajustes, hasta 87 niveles (Brillo) y 247 (Negativo); solo la viñeta coincidía. Corregidos los dos shaders y `applyDetail` (4.2). Opacas sin cambios (0–1) |
| exiv2 GPL si el programa será propietario (P0 legal) | Sin cambios desde la ronda 3 | **De acuerdo; sin decidir** (necesita elegir licencia). Vía de sustitución en `LICENCIAS.md` (no probada) |
| JPEG CMYK atraviesa mal la ruta RGBA (P1) | JPEG CMYK real | **Confirmado**: colores erróneos y el canal K como alfa. **Hallazgo propio, más grave**: PNG/TIFF de **16 bits salían blancos**. Ambos corregidos (4.8) |
| Metadatos con inconsistencias (P1) | Fixtures con EXIF/XMP/IPTC/ICC en JPEG y PNG | **Confirmado y peor**: el exiv2 de vcpkg estaba compilado sin `png` ni `xmp`; XMP no se conservaba nunca y PNG nada (4.7). Corregido y probado. Cierto también: TIFF no es destino de metadatos; HEIC/RAW no son origen (ahora se **avisa**); `MetadataReader::raw` solo vuelca EXIF (su comentario decía EXIF/IPTC/XMP) y lee el archivo entero a RAM |
| Los avisos de metadatos no llegan al usuario (P1) | Solo había `qWarning` | **Confirmado.** Nota no bloqueante en el lienzo y señal `saveNotice` |
| Ciclo de vida de los trabajadores de efectos (P1) | Código leído | **Cierto** (espera de 3 s sobre el pool global). Pool propio y espera completa. No se reprodujo el uso tras liberación; sí se prueba que cerrar a mitad de un efecto pesado es seguro y rápido |
| Decodificación sin cancelación real (P1) | Código leído | **Parcial**: ahora hay bandera cooperativa en el cargador (omite lo no empezado, p. ej. leer metadatos); no se puede interrumpir una librería a media decodificación y los decodificadores no reciben el token |
| Guardado síncrono bloquea la interfaz (P1) | Medido: 603 ms con 12 MP; efectos exactos, segundos | **De acuerdo. Hecho después de esta ronda** (4.11): instantánea inmutable, trabajador, «sin guardar» asociado a la instantánea; arranque 0 ms y parón máximo 17 ms |
| Memoria de la GPU / `ImageProvider` ignora `requestedSize` (P1) | Código leído (`Q_UNUSED(requestedSize)`) | **Cierto el código; sin medir la VRAM.** Propuesta del revisor (proxy de pantalla de 4–6 K) pendiente de medición |
| Gestión de color / ICC (P1) | Perfil Display P3 real a través de los decodificadores | **De acuerdo. Hecho después de esta ronda** (4.12): todo a sRGB al abrir. Hallazgos propios: el ICC del original se copiaba sobre píxeles ya convertidos (doble conversión) y exiv2 escribe el trozo `iCCP` de PNG con nombre vacío |
| `AppController` es un *God Object* (P2) | ~1 500 líneas | **De acuerdo**; el revisor mismo dice «después de los P0». Pendiente |
| Pasadas GPU innecesarias (P2) | Cadena de 3 `ShaderEffect` siempre activa | **Cierto y corregido** (5.1): una etapa sin efecto se apaga. 949 → 233 MB sin editar |
| LUT recalculada sin necesidad (P2) | `setLiveAdjust` la reconstruye siempre | **Cierto, irrelevante: 51 µs por llamada.** Sin cambios |
| Fusión de *undo* con 900 ms (P2) | Código leído | **Cierto.** Una pausa de 0,9 s a mitad de un arrastre parte el gesto. Cablear `begin/endGesture` toca cinco componentes QML (deslizadores, niveles, curvas, cantidad del filtro); pendiente |
| Entrega incompleta (P2) | — | Parcial desde la ronda 3 (`cmake --install`); sin instalador ni CI |
| «`IMAGEVIEWER_REQUIRE_VALIDATED_QT` es configuración muerta» | `grep` | **No coincide con el archivo actual**: se usa en `CMakeLists.txt` (líneas 27–38) desde la ronda 3. Probablemente leyó una copia anterior |
| «La documentación está atrasada» (LiveOp, presupuesto de checkpoints, Unicode, orientación, `QSaveFile`…) | Comprobado en este documento | **No coincide con la versión actual**: todo eso ya estaba descrito. Probablemente leyó la primera versión. Sí había una afirmación **falsa** que él no vio: «conserva XMP/PNG» (arriba) |
| `VipsGuard`: conservar; si `VIPS_INIT` falla, propagar el error (P2) | Código leído | De acuerdo. **Hecho** lo segundo |
| Antes de guardar, ¿«Guardar como» cambia el documento? (UX) | — | Sin cambios: el documento sigue apuntando al original. Etiquetar el botón «Guardar copia como…» es decisión de producto |

**Lo que el revisor no podía saber y salió al contrastar**: 16 bits en blanco; XMP y PNG sin
conservar pese a lo que decía el README; que `saveEdited()` escribía de verdad sobre otro
archivo; que la deriva GPU/CPU con **imágenes opacas** crece con el número de etapas
(combinación completa: hasta 11 niveles, por el redondeo a 8 bits de cada textura
intermedia; una opción de mejora es dar formato `RGBA16F` a los `ShaderEffectSource`, no
hecha).

**Límites de esta ronda**: el banco GPU compara con una escena que *copia* la cadena de
`ImageCanvas.qml` (si una cambia, hay que cambiar la otra); no se probó una foto real con
alfa en la ventana real; el cuantizado a 8 bits de la textura premultiplicada impide que la
vista previa de píxeles casi transparentes (alfa < 10 %) iguale exactamente a la CPU (de
ahí las tolerancias).

---

## 9. Hoja de ruta propuesta

Orden recomendado por la revisión, tras contrastarlo (**es una propuesta; el propietario
decide**):

**A. Base segura** — *(avanzada)*: guardado atómico y avisos ✔ · EXIF ✔ · Unicode ✔ ·
licencias analizadas ✔ · invariantes de carga/guardado/«sin guardar» ✔ (4.10) · alfa
CPU/GPU ✔ · 16 bits/CMYK ✔ · metadatos reales en PNG/XMP ✔ · guardado asíncrono ✔ (4.11) ·
**pendiente**: control de versiones (git, `.gitignore`, etiquetas), scripts de build y
verificación dentro del repo, elegir licencia del proyecto, resolver exiv2. *(Todo resuelto después; ver la actualización de arriba.)*

**B. Ingeniería de entrega**: CI (compilar → pruebas → humo → paridad → empaquetado) ·
ejecutar `ctest` completo en el CI (la paridad CPU/GPU ya es una prueba: `test_gpu_parity`;
necesita un dispositivo Direct3D 11, que WARP puede proveer) · ASan en una configuración
aparte · registro a archivo y minidump.

**C. Rendimiento medible**: presupuestos (apertura 12/25/50 MP, miniaturas, memoria pico,
carpeta de 10 000 imágenes) y medir la memoria de la GPU (después, un *proxy* de pantalla
en lugar de la textura completa; `RGBA16F` en las etapas intermedias; *bypass* de las
pasadas sin efecto) · investigar `VipsGuard`
(inicializar GIO una vez al arrancar; probar sin el mutex bajo ASan).

**D. Modelo de edición**: decidir el modelo de documento (capas, máscaras, texto) **antes**
de la Fase 5 del estudio de PhotoScape. El historial único actual encaja bien en un
editor raster sin capas. El revisor propone formalizar un `DocumentSession` (archivo +
píxeles + historial + estado guardado + tareas) y dividir `AppController` (fachada QML,
sesión de edición, motor de vistas previas de efectos, coordinador de guardado,
animación, integración con la plataforma) **después** de los P0, antes de las capas.

**E. Distribución**: instalador, asociaciones, firma, versionado, changelog, accesibilidad,
teclado completo, HiDPI, localización.

**F. Calidad fotográfica**: ICC aplicado ✔ (todo a sRGB al abrir, 4.12); queda 16 bits/canal
o float interno (para no recortar el gamut ancho), el perfil del monitor y HDR.

**Después**, del estudio de PhotoScape X Pro: Fase 4 (marcos, bordes, esquinas redondeadas,
sombra) · Fase 5 (capas de texto/formas/flechas, pinceles, máscaras) · Fase 6 (varita
mágica / quitar fondo, recetas por lote) · opcionales (tamaño de grano; IA solo con
modelos externos pequeños).

---

## 10. Deuda técnica y riesgos

### 10.1 Confirmados
1. ~~El proyecto no está bajo control de versiones.~~ *(Superado: está en git y GitHub, con CI en cada PR.)*
2. Los **scripts de compilación y de verificación de paridad** viven en una carpeta
   temporal, **fuera del repositorio**; el directorio de build es
   `C:\build\imageviewer\release`. (El ejemplo de presets de usuario ya apunta a Qt 6.7.3,
   la versión validada; ver 8.7.)
3. `VipsGuard` serializa toda la decodificación (sección 5.2).
4. Los efectos pesados (movimiento, zoom, giratorio) tardan 4,5–6,5 s a 25 MP en su
   versión exacta (la etapa aproximada cubre la espera).
5. `BatchExporter` decodifica todos los fotogramas de un GIF/APNG para usar solo el primero.
6. Animaciones: bucle siempre infinito y sin edición ni selección de fotograma.
7. ~~Sin traducciones.~~ *(Superado: 6 idiomas, sección 15.)*
8. Hay carpeta de entrega reproducible (`cmake --install`, ver 8.7). *(Superado: ya hay instalador, CI,
   `LICENSE`, `CHANGELOG.md`, `SECURITY.md` y `CONTRIBUTING.md`, y las versiones se publican con `release.yml`.)*
   **Sigue sin haber** firma de código ni informe automático de fallos.
9. Los efectos de ruido solo se verificaron a 100 % de zoom.
10. Sin pruebas automáticas de interacción de la interfaz (la paridad GPU↔CPU **sí** está
    automatizada: `test_gpu_parity`).
11. **8 bits por canal, solo sRGB** (4.9 y 4.12): el gamut ancho se recorta al abrir.
12. `AppController` tiene ~1 500 líneas y concentra carga, edición, efectos, guardado,
    historial y avisos: candidato a dividirse (ya tiene tests propios, lo que facilita
    hacerlo sin romper nada).
15. El guardado ya es asíncrono (4.11), pero **no se puede cancelar**, y copiar, fondo de
    escritorio, histograma y «Aplicar» efecto siguen en el hilo de la interfaz.
16. **Memoria de la GPU**: una foto de 37 MP sin editar ocupa ~235 MB (antes ~950), con un
    ajuste de tono ~620 MB y con filtro y ajustes a la vez sigue en ~950 MB (5.1).
    `ImageProvider` entregaba la imagen a resolución completa. *(Superado en parte: proxy de
    pantalla, sección 18.)*
17. HEIC/AVIF/RAW **no entregan sus metadatos** al guardar (se avisa); `MetadataReader` lee
    el archivo entero a memoria y solo vuelca EXIF.
18. Los `ShaderEffectSource` intermedios son de 8 bits: la deriva GPU↔CPU crece con las
    etapas (≤ 11 niveles con todo activo).
19. Dos copias de la cadena de shaders en QML (`ImageCanvas.qml` y
    `tests/gpu_parity/ParityScene.qml`): hay que mantenerlas a la par.
20. Fusión de *undo* por tiempo (900 ms) en lugar de gestos explícitos.
13. La miniatura de un archivo recién sobrescrito en la barra lateral no se refresca hasta
    que se vuelve a pedir (la clave de caché incluye la fecha de modificación).
14. **Licencia**: exiv2 (GPL) impide distribuir cerrado. *(Superado: el proyecto es GPL-3.0-or-later y tiene `LICENSE`.)*

### 10.2 Sospechas o puntos **sin verificar**
- **Accesibilidad** (lectores de pantalla, teclado, DPI altos): no revisado; los controles
  muy personalizados y los `Canvas` hacen que merezca una auditoría real.
- **Archivos modificados fuera de la app**: no hay `QFileSystemWatcher`.
- **Instancia única**: cada apertura desde el Explorador lanza un proceso.
- **Memoria de la GPU** y atribución del consumo inicial por copias concretas.
- **Robustez ante archivos corruptos o maliciosos** (no hay *fuzzing*); además el
  decodificador GIF/APNG es propio.
- Que la configuración HEIC con HEVC se abra con **fotos reales de iPhone** (se probó con
  una muestra sintética).

---

## 11. Qué falta para que sea "profesional" (lista de partida)

Ingeniería: control de versiones + ramas/etiquetas · CI (compilar, pruebas, humo,
empaquetar) · compilación reproducible desde cero con un solo comando · scripts de
verificación dentro del repo · paridad GPU/CPU automatizada · análisis estático y
sanitizers · *fuzzing* de decodificadores · registro a archivo · informe de fallos.

Producto: instalador con asociación de archivos · actualizaciones · firma de código ·
icono del `.exe` y metadatos de versión · localización · accesibilidad · gestión de color
completa y 16 bits · instancia única y detección de cambios externos · rendimiento con
presupuestos · documentación de usuario.

Legal: elegir licencia del proyecto, resolver exiv2, incluir avisos de terceros y los
textos de licencia (sección 12).

---

## 12. Licencias (resumen; detalle en `LICENCIAS.md`)

- `exiv2` 0.28.8 es **GPL‑2.0‑or‑later** y se enlaza en el programa: bloquea una
  distribución cerrada. Opciones: publicar con licencia compatible con la GPL, o
  sustituirlo (el uso está aislado en `MetadataReader.cpp` y `ImageWriter.cpp`).
- `x265` (**GPL‑2.0‑or‑later**) se distribuía sin necesidad (`libx265.dll`, 5 MB) por
  heredar libheif la característica `hevc`; **corregido** con `"default-features": false`.
  Comprobado: x265 ya no está instalado ni se despliega, y las pruebas abren un HEIC (HEVC,
  vía libde265) y un AVIF reales.
- Qt, libvips, libheif, libde265, LibRaw (a elegir LGPL), GLib y el resto son LGPL/MIT/BSD:
  compatibles con una aplicación cerrada si se cumplen sus condiciones (todas se enlazan
  como DLL). Patentes HEVC: aparte.
- **Datos de Lensfun** (perfiles de cámaras y objetivos de la herramienta Lente): **CC BY‑SA 3.0**;
  van sin modificar, como recurso, y su aviso está en `LICENCIAS.md` (sección 7b) y en el generador
  de avisos. Solo se usan los datos: la biblioteca Lensfun (LGPL) no se enlaza.
- `docs/THIRD_PARTY_NOTICES.txt` (388 KB, 19 componentes) se genera con
  `tools/make_third_party_notices.ps1` a partir de los archivos de licencia de vcpkg.

---

## 13. Apéndice

### 13.1 Inventario de archivos
```
CMakeLists.txt · CMakePresets.json · vcpkg.json · README.md
docs/    DESARROLLO.md · LICENCIAS.md · THIRD_PARTY_NOTICES.txt
src/app   AppController · ImageProvider · FolderModel · ThumbnailImageProvider
          ThemeManager · AppSettings · BatchExporter · LensController · PaneImageProvider
          AnimStudio · CollageStudio   (biblioteca `imageviewer_app`, para poder probar
          el controlador real) · main.cpp (solo el ejecutable)
src/core  ImageLoader · ImageDocument · DecoderRegistry · ThumbnailCache
          MetadataReader · Histogram · ImageWriter · ColorManagement
          decoders/ Vips · Raw · Heif · Animated · QtImage · VipsGuard · IImageDecoder
          edit/     Operations · EditStack · AdjustMath · Looks · Resample
                    ParallelRows · Effects (+ Common, Color, Pattern, Decor, Light, Geometry,
                    Lens, Frame) · Blend · Denoise
          lens/     LensDatabase (lector de Lensfun y matemática de la corrección)
          anim/     Anim.h · Compose · GifEncoder · ApngEncoder · WebpEncoder
          collage/  Collage (dibujo) · Layouts (diseños y líneas divisorias)
qml/      Main · ImageCanvas (1 100 líneas) · EditPanel (810) · Toolbar · CropTool · FramePanel
          LensTool · MultiView · PaneView · GifStudio · CollageMaker
          StatusBar · ThumbnailStrip · LookCell · EffectParamRow
          UnsavedChangesDialog · OverwriteDialog · AdjustRow/Section
          LevelsEditor · CurvesEditor · HistogramView · SettingsDialog …
qml/controls/  App*.qml (Button, Slider, CheckBox, RadioButton, Menu, Dialog,
               ComboBox, SpinBox, TextField, ProgressBar, TabBar, ScrollBar,
               WheelScroll, Icon, ToolTip, HintBox, PanelBackground, Win98Bevel…)
qml/shaders/   Grade.frag · Detail.frag
tests/    test_edit_stack · test_decoder_registry · test_image_writer
          test_animated_decoder · test_image_loader · test_app_controller
          test_color_management · test_gpu_parity (+ gpu_parity/ParityScene.qml) · smoke_startup.cmake
          test_effect_families · test_lens · test_frame · test_pane_images · test_anim
          test_anim_studio · test_collage · test_collage_studio
          (+ fixtures: fotos de ejemplo de cada formato, las 8 orientaciones EXIF, una foto
          con EXIF/GPS/ICC, `meta_rich.jpg/png` con XMP e IPTC, `cmyk_sample.jpg`,
          `p3_icc.heic/avif` en Display P3; `color_reference.h`: matemática de color de referencia)
tools/    context_menu_add.reg · context_menu_remove.reg · make_third_party_notices.ps1
          Test-Install.ps1
```

### 13.2 Compilar y probar
```
cmake --preset windows-release          # requiere VCPKG_ROOT y CMAKE_PREFIX_PATH (Qt)
cmake --build --preset windows-release
ctest --preset windows-release          # equivale a ctest --test-dir build/release
cmake --install build/release --prefix dist    # carpeta completa para entregar
powershell -File tools/Test-Install.ps1        # comprueba que la carpeta instalada es autosuficiente
```
`windows-release` exige exactamente Qt 6.7.3 (`IMAGEVIEWER_REQUIRE_VALIDATED_QT`); el preset
de depuración solo avisa. En la máquina de desarrollo se compila con un `.bat` que carga `vcvars64` y llama a
`cmake --build C:\build\imageviewer\release`. Cambiar `vcpkg.json` hace que vcpkg
reinstale lo afectado en el siguiente `cmake --build`.

### 13.3 Cómo se agrega una función de edición nueva (convenciones)
- Efecto por canal → entra en `buildAdjustLut` (paridad GPU gratis).
- Mezcla multicanal → `ColorMix::apply` + las mismas líneas en `Grade.frag`.
- Efecto espacial vivo → `applyDetail` + `Detail.frag`.
- Efecto de píxeles → una entrada en `buildCatalogue()` y su función en `Effects.cpp`
  (la interfaz, la vista previa, la miniatura y el historial salen del catálogo); sus
  distancias van como porcentaje del lado largo y sus bucles pasan por los envoltorios
  cancelables `rows`/`bands`.
- Operación de píxeles nueva → `Op` en el `std::variant` de `Operations.h` y caso en
  `applyOperation`.
- Todo slider de Ajustes nuevo: tabla `kParams` de `AppController::setAdjustParam`, el
  mapa de `adjustMap()` y una `AdjustRow` en `EditPanel.qml` (su clave de fusión del
  historial sale sola).
- Un formato nuevo → un decodificador registrado en `DecoderRegistry`, **con un archivo
  de ejemplo en `tests/fixtures` y su caso en `test_decoder_registry`**. Los decodificadores
  de mentira de los tests se registran con `DecoderRegistry::registerDecoder()`.
- Todo shader nuevo debe respetar el **contrato de alfa** (4.2): por píxel sobre color
  recto, de vecindad sobre premultiplicado; y llevar una fila en `test_gpu_parity`.

**Versión portable.** `src/app/AppPaths.h`: si junto a `ProjectFoxy.exe` existe `portable.txt`, la configuración (`settings.ini`) y la caché de miniaturas van a `datos\` en vez del perfil de Windows. El zip se arma con `cmake --install` (carpeta autocontenida, ver `tools/Test-Install.ps1`) + `portable.txt` + `LEEME.txt` + `licenses\` (avisos y `LICENCIAS.md`); sin firmar (SmartScreen avisa). Probado extrayendo el zip en una carpeta limpia y arrancando con PATH reducido: sin salida de errores, `datos\` creado, el `.ini` real intacto.

**Barra de miniaturas configurable.** `ThumbnailStrip.qml` pasó de `ListView` a `GridView`: `appSettings.stripColumns` (1–4) × `stripThumbSize` (48–200 px, tope: la mitad de la ventana). Arrastrando el borde derecho con el mouse cambia el tamaño en vivo (`liveCell`, sin animación) y al soltar se guarda en `stripThumbSize`. `appSettings.stripMode`: `manual` (la pestaña abre/cierra), `open` y `closed` (sin pestaña) y `auto` (cerrada al editar y con la ventana de menos de 900 px; la pestaña puede invertir la decisión hasta que cambie la situación). Las miniaturas se piden a 160 px, o a 256 si el tamaño pasa de 110. Se configura en Configuración > Apariencia > «Barra de miniaturas». +1 prueba en `test_app_settings`.

**Nombre e icono: «Project Foxy».** Título de la ventana en `Main.qml`; icono en `resources/icons/icon.png` (ventana, `main.cpp`) y `icon.ico` (el `.exe`, vía `resources/app.rc`). `applicationName`/`organizationName` siguen siendo `ImageViewer` a propósito: de ellos cuelgan las carpetas de configuración y caché, y cambiarlos perdería los ajustes de quien ya lo usa. El ejecutable, el target de CMake y el proyecto aún se llaman `ImageViewer`.

**Cambio de nombre.** El programa se llamaba «ImageViewer» y ahora es **Project Foxy**: ejecutable `ProjectFoxy.exe`, ventana, documentos, licencias y carpeta de configuración (`%LOCALAPPDATA%\ProjectFoxy`; la primera vez se copia el `settings.ini` de la carpeta antigua `ImageViewer`, la caché de miniaturas se regenera). El target de CMake, el módulo QML (`ImageViewerApp`) y las rutas de compilación conservan el nombre antiguo a propósito. (Donde este documento dice «ImageViewer» entre comillas de código es ese nombre interno.)

**Rendimiento de los efectos (lag con otros programas).** Los efectos pesados (Giratorio, Zoom, Movimiento, Semitono, Quitar ruido) saturaban todos los núcleos a prioridad normal y dejaban sin CPU al resto del equipo (un vídeo de YouTube se congelaba mientras se arrastraba un slider). Ahora el pool de filas (`ParallelRows.h`) deja dos núcleos libres (uno en equipos pequeños) y sus hilos corren con prioridad `Lowest`. Además se aceleraron los bucles: muestreo bilineal en `float` (`accumulate`), giro por recurrencia con tabla de seno/coseno en Giratorio, un punto por celda calculado una sola vez en Semitono y red de ordenación de 19 comparaciones para la mediana 3×3. Resultado en 24 MP: Quitar ruido 2,7× más rápido, Semitono 3,7× (salidas idénticas bit a bit), Giratorio y Zoom ~2×. `bench_effects` (en `tests/`, no es una prueba) mide tiempo real y de CPU de cada efecto: `bench_effects [ladoLargo] [id...]`, con `BENCH_IMG`/`BENCH_SAVE` opcionales.

**Reductor de ruido (Non-Local Means).** El antiguo «Quitar ruido» era una mediana 3×3/5×5: solo quita puntos sueltos y apenas cambia el ruido real de una cámara. Ahora «Quitar ruido» (`denoise`, `Denoise.{h,cpp}`) es un Non-Local Means en color y la mediana sigue existiendo como «Mediana» (`median`). **Cómo funciona:** la imagen se pasa a Y/Co/Cg; el nivel de ruido de cada plano se mide de la propia foto (el mayor de dos estimadores: mediana del Laplaciano absoluto y varianza del 10 % de bloques 8×8 más planos, con un sesgo de calibración 1,12; con ruido blanco da 9,17 frente a 9,19 reales y con ruido en manchas, el de cámara, sigue funcionando porque el Laplaciano solo no lo ve); cada píxel se sustituye por la media de los de su ventana 7×7 cuyo parche 5×5 (comparado en los tres planos a la vez) se parece al suyo, con peso exp(−exceso/h²) donde «exceso» es la distancia por encima de lo que causaría solo el ruido. Sliders: *Luminancia* (h de Y, 0,06–1,26 σ) y *Color* (h de Co/Cg, 0,06–2,46 σ, mucho más fuerte porque el ruido de color es más molesto); por defecto 50 y 60. Va por bandas de 24 filas (cada banda carga su margen, sin planos float de toda la imagen) y se cancela como el resto. **Vista previa:** como el ruido depende de los píxeles reales, `EffectSpec::fullSize` hace que no pase por el proxy de 1600 px (que ya habría perdido el ruido): se muestra el resultado exacto cuando el slider reposa (260 ms). **Medido:** ruido gaussiano de 15 sobre una ilustración: 24,7 dB → 37,0 dB (la mediana: 31,9 dB); con ruido en manchas, 26,3 → 29,8 dB (mediana 26,3); foto de cámara de 24 MP: 2,3 s con 16 hilos (15,7 s de CPU, a prioridad baja). Se probó y **se descartó** un segundo paso de suavizado del color con filtro guiado por la luminancia: bajaba la calidad (6 dB menos) y desaturaba. Pruebas en `test_edit_stack` (error medio 11,3 → 2,0 frente a 5,0 de la mediana, borde conservado, alfa intacto, fuerza 0 = identidad).

## 14. Publicar una versión (6‑oct‑2026)

Todo lo necesario para publicar en GitHub está en el repositorio:

- `tools/Build-Release.ps1`: compila, corre las pruebas, hace `cmake --install` a `dist\ProjectFoxy`, arma
  `dist\ProjectFoxy-portable-<versión>.zip` (la misma carpeta + `portable.txt` + `LEEME.txt`, de `installer/`), compila el
  instalador si hay Inno Setup y escribe `dist\SHA256SUMS.txt`. `dist/` está en `.gitignore`.
- `installer/ProjectFoxy.iss` (Inno Setup 6): instala por usuario sin administrador (o para todos, si se elige), acceso
  directo opcional y, opcionalmente, **registra el programa como abridor de imágenes** (`ProjectFoxy.Image` con su
  ícono, «Abrir con» para cada extensión que lee y `Capabilities` + `RegisteredApplications` para que aparezca en
  *Aplicaciones predeterminadas*). Windows no deja que un programa se ponga solo como predeterminado: el último paso lo
  hace la persona; cuando elige Project Foxy para un tipo, el Explorador muestra su ícono sobre esos archivos. El
  desinstalador borra todo lo que el instalador registró.
- `.github/workflows/ci.yml` (compila y prueba en cada *push*/PR) y `release.yml` (al subir una etiqueta `vX.Y.Z`:
  compila, arma instalador y zip y publica la *release*). **No se pudieron probar localmente**; el CI y `release.yml` ya
  funcionan (0.2.0 y 0.2.1 publicadas con ellos; la 0.2.0 necesitó un arreglo del instalador, ya corregido). Usan Qt 6.7.3 con `install-qt-action`, vcpkg del *runner* con caché de binarios y Inno Setup por Chocolatey.
- `tools/make_banner.py` dibuja `docs/assets/banner.png` a partir del logo (`docs/assets/logo.png`); las capturas del
  README están en `docs/screenshots/`.
- La versión sale de `project(... VERSION ...)` en `CMakeLists.txt` (el programa la muestra en el panel de información);
  `resources/app.rc`, `vcpkg.json` y `installer/ProjectFoxy.iss` deben coincidir. Subir versión = cambiar esos sitios,
  añadir la entrada a `CHANGELOG.md`, y `git tag vX.Y.Z && git push origin vX.Y.Z`.

## 15. Idiomas de la interfaz (6‑oct‑2026)

Seis idiomas: español (el original), inglés, portugués (Brasil), coreano, chino simplificado y japonés.

- **El texto en español es la clave.** QML usa `qsTr("…")`; C++ usa `tr("…")` en los `QObject` y `core::tr("…")`
  (`src/core/Translate.h`) en el núcleo. Sin traductor (pruebas, español) devuelven el texto tal cual.
- **`src/app/Translations.{h,cpp}`**: `CatalogTranslator` (un `QTranslator` que busca el texto original en un JSON
  `{"español": "traducción"}` e ignora el contexto) y `Translations` (elige el idioma: `AppSettings::language` =
  `system` | `es` | `en` | `pt` | `ko` | `zh` | `ja`; `system` toma el primer idioma de Windows que tengamos, o inglés).
  Cambiar el idioma instala el traductor y llama a `QQmlEngine::retranslate()`: **se aplica al instante**. Los nombres
  que vienen del núcleo (efectos, ajustes, looks, preajustes, diseños de collage) se traducen en el límite con QML
  (`AppController::effectList()`… llaman a `core::tr()`), y `AppController::retranslate()` avisa a QML con
  `textsChanged`/`effectChanged` para que los pida de nuevo. Los temas usan `QT_TR_NOOP` + `qsTr(nombre)` en QML.
- **Catálogo**: `i18n/strings.tsv` (español, en, pt, ko, zh, ja; una fila por texto) → `tools/i18n_tool.py build` →
  `resources/i18n/<idioma>.json`, que se compilan en el ejecutable (`:/i18n/…`). `tools/i18n_tool.py check` lista los
  textos del código que faltan en la tabla y los marcadores `%1` que no coinciden; `tools/i18n_add.py` agrega filas.
  Una celda vacía = igual que el español. Los JSON generados **se suben al repositorio** (el CI no ejecuta Python).
- **Pruebas**: `test_translations` (qué idioma sale de cada idioma de Windows, el traductor, `core::tr` y que cada
  catálogo tenga los textos clave con sus marcadores).
- **Instalador**: Inno Setup en inglés, español, portugués, coreano y japonés (el chino simplificado no viene con
  Inno Setup; el instalador cae en inglés para ese idioma, el programa sí está en chino).
- Los textos nuevos: escribirlos en español dentro de `qsTr`/`tr`, agregar la fila a `strings.tsv` y `build`.

## 16. Linux (8‑oct‑2026, versión 0.2.0)

Project Foxy corre en Linux como **AppImage** y **Flatpak**. Todo vive en `packaging/linux/` y en `.github/workflows/`.

- **AppImage** (`build-appimage.sh`): linuxdeploy + plugin de Qt fijados por hash, compilado en Ubuntu 22.04; exige
  glibc ≥ 2.35 (libstdc++ de GCC 12). Los plugins de Wayland se copian a mano. `smoke-appimage.sh` lo arranca en el CI.
- **Flatpak** (`flatpak/io.github.vulob1.ProjectFoxy.yml`, `build-flatpak.sh`): runtime KDE 6.10; compila libvips,
  LibRaw, exiv2 e inih desde fuente con las versiones de `vcpkg.json`; `build-flatpak.sh --test` corre la suite contra la
  Qt 6.10 del runtime. Pide `--filesystem=host` porque la barra de miniaturas lista las demás imágenes de la carpeta.
  Para Flathub habría que quitar `appstream-compose: false` (solo está por el `flatpak-builder` viejo de Ubuntu 22.04) y
  usar una fuente git con etiqueta; **no se envió a Flathub** (decisión del propietario, por ahora).
- **Código específico**: `src/app/LinuxPortal.*` (papelera y fondo de pantalla por los portales de escritorio, que es lo
  que se usa dentro de Flatpak), `FontSubstitutions.h` (Segoe UI/Arial/Impact → equivalentes libres),
  `AppPaths::samePathKey` (los nombres de archivo distinguen mayúsculas; `FOTO.JPG` se encuentra igual que `foto.jpg`).
- **CI** (`ci.yml`): `build` (Windows), `linux` (preset `linux-release`, Xvfb + llvmpipe, pruebas, AppImage y humo) y
  `flatpak` (imagen kde-6.10). **`release.yml`** al subir una etiqueta `v*`: `windows`, `linux`, `flatpak` y `publish`
  (una sola release con instalador, zip portable, AppImage, Flatpak y `SHA256SUMS.txt`).
- **Probado a mano (WSL2, 8‑oct)**: AppImage en Ubuntu 24.04, Debian 13, Fedora 43 y Arch (X11 y Wayland); falla en
  AlmaLinux 9 (glibc 2.34, esperado). Flatpak funciona en esas cinco. Qt 6.10.3 pasa las 22 pruebas.
- **Nunca verificado**: un escritorio Linux real con GPU real (WSL no tiene portales ni GPU real), la papelera y el fondo
  de pantalla por portales, y el rendimiento real en Linux.

## 17. Navegación y miniaturas (9‑oct‑2026, versión 0.2.1)

Un tercero reportó una demora de ~300 ms al cargar archivos. Dos cambios (el reporte tenía «2 errores»; **el segundo no se
conoce**):

1. **`FolderModel::openFolderForFile`** listaba la carpeta, la ordenaba con `QCollator` y reseteaba el modelo
   (reconstruyendo la tira de miniaturas) **en cada cambio de imagen**, en el hilo de la interfaz. Ahora solo reescanea si
   cambió la fecha de modificación de la carpeta, es otra carpeta o el archivo no está en la lista. Prueba nueva en
   `tests/test_folder_model.cpp`. **No se midió el tiempo antes/después.** Límite: depende de que el sistema de archivos
   actualice la fecha de la carpeta (no hay `QFileSystemWatcher`).
2. **Miniaturas del sistema** (`src/core/SystemThumbnails.*`, llamado desde `ThumbnailCache::thumbnail` tras la caché
   propia y antes de decodificar): Windows pide la miniatura a la caché del Explorador con `IShellItemImageFactory`
   (`SIIGBF_THUMBNAILONLY | SIIGBF_INCACHEONLY | SIIGBF_BIGGERSIZEOK`; una falta de caché falla al instante y no hace
   generar nada al shell; COM se inicia una vez por hilo); Linux lee el almacén freedesktop
   (`~/.cache/thumbnails/{normal,large,x-large,xx-large}`, nombre = MD5 de la URI, entrada descartada si `Thumb::MTime`
   no coincide, o si el nivel es más de un 20 % menor que lo pedido). Medido en Windows: ~2 ms por miniatura con COM
   iniciado contra 12–78 ms decodificando con Qt; la primera consulta de cada hilo tarda ~55–65 ms. **La parte de Linux
   solo tiene pruebas unitarias** (`tests/test_thumbnail_cache.cpp`). Solo se lee: no se escribe en esos almacenes.
3. **`ThumbnailImageProvider`**: pool de hilos propio (mitad de los núcleos, prioridad baja) y cancelación de las
   peticiones que salen de pantalla, para que las miniaturas no compitan con abrir la imagen.

Pendientes y decisiones abiertas: atajos de teclado (se diseñarán en conjunto, al final), ícono superpuesto en la
miniatura del Explorador (necesita ser la app predeterminada del tipo), firma de código, actualizador automático,
Flathub (descartado por ahora).

## 18. Proxy de pantalla (11‑oct‑2026)

Una foto de 37 MP vista ajustada a la ventana se dibuja con unos pocos millones de píxeles, pero la GPU guardaba
toda la imagen más una textura por etapa de edición. Ahora, mientras la imagen se muestra con **no más de 4096 px de lado
largo** (y la imagen real tiene más de 5120), `ImageProvider` sirve una copia reducida:

- `image://viewer/current?rev=N&side=S` (y `original?…&side=S`, la de «Comparar») devuelve la imagen con el lado largo en
  `S` píxeles (`Qt::SmoothTransformation`), **creada una vez por imagen y lado** y guardada (una por cada una de las dos
  imágenes; `setImage`/`setOriginalImage` la descartan). La imagen completa no se toca: ediciones, lectura de píxeles
  (`pixelAt`), histograma y guardado siguen usándola. Cuesta 13–60 ms en el hilo de la interfaz, una vez.
- En `ImageCanvas.qml` el `Image` conserva el **tamaño real** de la imagen (`appController.currentImageSize`, ya no
  `image.sourceSize`, que pasaría a ser el de la copia), con `fillMode: Stretch`, así zoom, recorte y lecturas no cambian.
  Las etapas (`lookSource`, `adjustSource`) renderizan a una textura del tamaño de lo que está cargado
  (`stageTextureSize`). Para que el tamaño llegue antes que la fuente, `AppController` emite `currentImageSizeChanged`
  **antes** de `currentSourceChanged`/`structuralImageChanged`.
- Cuándo se usa (`proxyAllowed`, `proxyWanted`): imagen grande, **sin** nitidez, claridad ni grano (ni grano del filtro:
  su radio es en píxeles de la imagen y sobre la copia darían otro resultado), sin animación, y tamaño en pantalla
  (`escala × lado largo × devicePixelRatio`) ≤ 4096 (con un 15 % de margen para volver a ella). Al acercarse más se pasa a
  la imagen completa **de inmediato**; volver a la copia espera 350 ms a que el zoom se asiente. Una imagen nueva decide al
  instante, una vez ajustada la vista.
- **Medido** (ventana real, Windows, AMD RX 6750 XT; foto sintética de 7720×4824 con ruido; contadores `GPU Process Memory`):

| Caso | GPU dedicada, antes → ahora | GPU compartida | Memoria del proceso |
|---|---|---|---|
| Visor, ajustada | 236 → **93 MB** | 149 → 43 MB | 553 → 382 MB |
| Ajuste de tono/color | 616 → **196 MB** | 149 → 43 MB | 549 → 380 MB |
| Girada 90° (edición estructural) | 234 → **93 MB** | 149 → 43 MB | 690 → 525 MB |
| Con nitidez activa | 616 → 616 MB (resolución completa, a propósito) | — | — |

- **Fidelidad** (capturas de la ventana, proxy contra sin proxy, ignorando el recuadro de lectura de píxel): visor y giro,
  diferencia máxima ≤ 6 niveles; con ajuste de tono, máxima 11 y media 1,4 sobre una imagen de ruido muy fuerte
  (σ = 12; en una foto normal será menor); con nitidez, idénticas.
- **No medido / no probado:** con fotos reales de cámara, en Linux con GPU real, con monitores de alta densidad (se probó
  con `devicePixelRatio` 1), ni el comportamiento al arrastrar los controles de nitidez/claridad/grano con la imagen
  alejada (al activarlos pasa a la imagen completa, con un salto de carga único). Tampoco hay una prueba automática de la
  interfaz: la comparación se hizo con un arnés temporal que no está en el repositorio.

## 19. Una sola ventana (11‑oct‑2026)

Abrir una imagen mientras Project Foxy ya está abierto (doble clic en el Explorador, «Abrir con») ya no arranca otro
proceso: se la pasa al que corre y le trae la ventana al frente.

- **`SingleInstance`** (`src/app/SingleInstance.{h,cpp}`). La primera instancia escucha en un `QLocalServer` **del usuario**
  (`UserAccessOption`): una tubería con nombre en Windows (`ProjectFoxy-<usuario>`) y, en Linux, un socket en la carpeta de
  ejecución (`XDG_RUNTIME_DIR`, que Flatpak comparte entre ejecuciones de la misma aplicación; `/tmp` si falta). Las siguientes
  se conectan, mandan **una línea JSON** `{"files": [...]}` (rutas **absolutas**, resueltas contra la carpeta del que envía),
  esperan `ok` y salen con código 0. Solo se abre el primer archivo. Línea de más de 64 KB o que no sea JSON con `files`: se
  descarta. Un cliente que conecta y calla se suelta a los 5 s.
- **Resultados** (`sendToRunning`): `Delivered` (salir), `NobodyThere` (no hay nadie: este proceso escucha y sigue) y
  `NoAnswer` (algo aceptó la conexión pero no contestó, p. ej. una instancia colgada: este proceso corre **solo y sin
  escuchar**, para no pisarle el nombre). `listen()` primero prueba conectarse: **no le quita el nombre a quien contesta**;
  si nadie contesta, borra el socket viejo (un cierre brusco lo deja en Linux) y escucha.
- En Windows el cliente llama a `AllowSetForegroundWindow` (acaba de nacer de un clic de la persona), y la instancia que
  recibe hace `showNormal` si estaba minimizada, `raise` y `requestActivate`.
- **Opt‑out:** `--new-window` en la línea de comandos, o el interruptor «Una sola ventana» (Ajustes → Apariencia →
  Ventanas; clave `window/singleInstance`, activado por defecto; se lee al arrancar, así que aplica la próxima vez).
- **Pruebas:** `test_single_instance` (11 en Windows, 12 en Linux): sin nadie, ruta absoluta, nombres Unicode, sin
  archivo, varios lanzamientos seguidos, un segundo `listen()` no roba el nombre, **socket viejo** (solo Linux), oyente mudo
  → `NoAnswer`, mensaje mal formado y mensaje enorme. Además se comprobó con el programa real: el segundo lanzamiento sale en
  ~90 ms con código 0, queda un único proceso, la ventana pasa de la imagen A a la B (comparando capturas), `--new-window`
  crea otro proceso, y tras matar el proceso todo vuelve a funcionar.
- **No probado:** el comportamiento de «traer al frente» con un doble clic real desde el Explorador (la prueba lanzó los
  procesos desde PowerShell: el contenido cambió, pero no se verificó qué ventana quedó al frente), Flatpak/AppImage
  reales, ni dos sesiones de usuario a la vez.

## 20. Pruebas de interfaz (12‑oct‑2026)

Hasta ahora ningún test manejaba la interfaz real: un botón desconectado o un panel que no abre no lo veía nadie.
`tests/test_ui_flows.cpp` carga el `Main.qml` **real**, sobre el **mismo cableado** que el programa
(`AppBootstrap::setup`, que ahora comparten `main.cpp` y la prueba), y lo maneja como una persona: eventos de ratón y de
teclado sintéticos, con la plataforma `offscreen` de Qt y su renderizador por software (sin ventana ni GPU).

- **Los controles se identifican por función, no por posición ni por aspecto**: cada uno que la prueba usa tiene un
  `objectName` (`nextButton`, `previousButton`, `zoomInButton`, `zoomOutButton`, `fitButton`, `editButton`,
  `settingsButton`, `saveButton`, `undoButton`, `tool-<id>`, `rotateRightButton`, `applyToolButton`, `cancelToolButton`,
  `unsavedSaveButton`/`unsavedDiscardButton`/`unsavedCancelButton`, `overwriteConfirmButton`, `imageCanvas`). **Regla para
  rediseñar la interfaz:** se puede mover, reestilizar o cambiar de contenedor cualquier cosa mientras cada nombre siga
  significando el mismo control; si un control se reemplaza, el nuevo hereda el nombre.
- **Recorridos (9):** abrir una imagen y verla en el lienzo; Siguiente/Anterior y las flechas del teclado; los botones de
  zoom y Ajustar; **editar → Recortar → girar → Aplicar → Guardar → «Sobrescribir»** (el archivo queda girado y nada queda sin
  guardar); Deshacer; y **la pregunta de «cambios sin guardar»** al cambiar de imagen (Cancelar se queda y conserva la edición,
  Descartar sigue).
- **Cómo está montado:** `qt_add_qml_module` solo puede colgar de un ejecutable, así que `test_ui_flows` construye **su propia
  copia del módulo** (mismos archivos QML, vía `IV_QML_FILES`, y los mismos recursos, vía `iv_add_app_resources`); nada de eso se
  distribuye. Cada recorrido arranca un motor nuevo; el modo de prueba de `QStandardPaths` evita tocar la configuración de la
  persona. `click()` espera a que el control esté visible, habilitado **y quieto** (los paneles se deslizan: un clic sobre un
  objetivo que se mueve cae en el vacío).
- **Comprobado que atrapa roturas:** con tres mutaciones temporales del QML (Siguiente que retrocede, «Descartar» que en
  realidad cancela, «Guardar» que no hace nada), la prueba terminó con exactamente 3 fallos.
- **Encontró un fallo real de apagado.** Al destruir el motor QML, `FolderModel` (y su caché de miniaturas) se destruye
  **antes** que `ThumbnailImageProvider`, cuyos hilos aún podían estar usando esa caché: corrupción de memoria (en Windows,
  `0xc0000374`) en ~1 de cada 12 corridas. En el programa real solo ocurría al cerrar, sin que nadie lo notara. Ahora
  `ThumbnailImageProvider::shutdown()` detiene y espera a sus hilos cuando el motor empieza a destruirse (`AppBootstrap`);
  tras el arreglo, 0 fallos en 80 corridas seguidas en Windows y 0 en 15 en Linux.
- **No cubre:** el aspecto (es lo que se verifica con capturas, no con estas pruebas), estudios (GIF, collage), el modo de
  comparación de varias imágenes, la configuración, ni accesibilidad. Se amplía cuando se rediseñe la interfaz.

## 21. Rediseño de la interfaz (12‑oct‑2026, versión 0.2.5)

- **Ventana sin marco** (`Main.qml`: `FramelessWindowHint`) con barra de título propia, `TitleBar.qml`: ícono, nombre y
  minimizar/maximizar/cerrar con el tema. Arrastrar y doble clic usan `startSystemMove()` (el sistema sigue
  encargándose del ajuste a los bordes) y ocho `MouseArea` en los bordes llaman a `startSystemResize()`. En Windows,
  `main.cpp` pide a DWM las esquinas redondeadas (`DWMWA_WINDOW_CORNER_PREFERENCE`) y un borde fino con sombra
  (`DwmExtendFrameIntoClientArea`); se enlaza `dwmapi`. Al agrandar rápido la zona nueva se ve negra un instante (el swapchain de Direct3D 11 de Qt;
  `tools/Test-ResizeStrips.ps1` lo mide); `QT_D3D_NO_FLIP=1` no sirve: sale blanca. **Congelamientos al redimensionar
  (12‑oct‑2026):** en una Radeon RX 6750 XT con Direct3D 11 la interfaz se congela de 100 ms a 2 s al agrandar la ventana
  con el mouse; no pasa en la GTX 1050 Ti del mismo equipo, ni con Direct3D 12, Vulkan u OpenGL, y una ventana Qt Quick
  mínima también lo sufre (menos), así que no es de Project Foxy. Las pilas muestreadas dejan al hilo de dibujo esperando
  dentro de `dxgi.dll` / `amdxx64.dll` (el `Present`). **Desactivar el V-Sync lo arregla** (`QSG_NO_VSYNC=1`: `Present(0,
  ALLOW_TEARING)`; la CPU en reposo no sube). Por eso existe el ajuste «Sincronizar con la pantalla (V-Sync)»
  (`AppSettings::verticalSync`, `window/verticalSync`, activado por defecto): `main.cpp` pone `QSG_NO_VSYNC=1` antes de
  crear la ventana si está apagado. Sin probar: tearing en monitores sin FreeSync/G‑Sync, consumo con animaciones, la
  causa exacta en el driver. Todo el análisis y los datos: `revision-chatgpt/redimensionado/` (no versionado).
- **Barra inferior** (`StatusBar.qml`, reemplaza a `FloatingToolbar.qml`): a la izquierda la posición y el color del
  píxel bajo el cursor (`ImageCanvas.pixelInfo`/`pixelCell`; siempre activo, se quitó el ajuste `pixelReadout`), a la
  derecha tamaño, zoom y el botón de información (el panel de información pasó aquí desde `Toolbar`), y en el medio el
  «dock»: una pieza más alta, un solo contorno con la barra (`Canvas`; esquinas redondeadas y empalme cóncavo). Lleva
  todos los botones solo con íconos. El comparador (`MultiView`) ya no tiene píldora flotante: expone `dockContent`
  (un `Component`) y `StatusBar.customDock` lo muestra en el dock. La imagen se ajusta por encima del dock
  (`ImageCanvas.bottomInset`, que `Main.qml` pone en `statusBar.rise`): el ajuste y el centrado usan `viewHeight` (la vista
  menos ese margen) y el `Flickable` llega hasta la barra, con `bottomMargin: bottomInset`; así una imagen con zoom se
  ve también a los lados del dock. «Copiado» aparece junto al cursor 0,7 s al copiar un color.
- **La barra de título es también la barra de herramientas** (`TitleBar.qml`): icono y nombre del programa, nombre del
  archivo, Editar, GIF animado, Collage, Comparador, «Más herramientas» y Configuración, y los botones de ventana.
  `Toolbar.qml` ya no dibuja nada: solo guarda los diálogos que esos botones abren (`requestOpen`, `openSettings`...).
  Todo lo que no es un botón es zona de arrastre (`objectName: "titleDrag"`), que nunca baja de `minDrag` (96 px): el
  nombre del archivo ocupa como mucho la mitad de la barra y, al estrecharse la ventana, GIF animado, Collage y
  Comparador (el último primero) pasan al menú «⋯»; Editar y Configuración siempre quedan a la vista. En Windows
  `WindowsFrame.cpp` devuelve `HTCLIENT` sobre los botones (cualquier `QQuickAbstractButton` o ítem con
  `captionInteractive`) y `HTCAPTION` en el resto. La ventana lleva `WS_CAPTION` (sin él Windows no anima minimizar y
  restaurar desde la barra de tareas); maximizada, `WM_NCCALCSIZE` pone el área de cliente igual al área de trabajo del
  monitor, porque con `WS_CAPTION` Windows deja el marco asomando 8 px por los bordes.
- **Trampas de `Canvas`** (aprendidas dos veces): el contexto 2D conserva su estado entre `onPaint`, hay que empezar con
  `ctx.reset()` (un `translate` acumulado deformaba la pestaña de la barra de miniaturas cada vez que se repintaba), y
  `canvasSize` debe seguir al tamaño del ítem o el dibujo se estira.
- **Pestaña de la barra de miniaturas**: siempre visible; en `open`/`closed`/`auto` invierte lo que dicta el modo
  (`flipped`); el modo «con pestaña» pasó a llamarse «Manual».
- **Componentes nuevos**: `DialogHeader.qml` (encabezado de diálogos y paneles), `controls/AppCard.qml` (tarjeta con
  título), `controls/AppSegmented.qml` (elección única con texto o íconos). Los diálogos de exportar y renombrar por lote
  usan tarjetas; Configuración es más alta, con la página activa teñida, `AppWheelScroll` (100 px por vuelta) y una
  página «Acerca de» (`AboutPage.qml`: degradado radial, autor con su foto —`resources/images/creator.png`—, enlaces,
  agradecimientos con enlace opcional y licencias).
- **Estudios de GIF y de collage**: el ícono y el título van arriba del panel derecho; ya no hay barra superior ni X: el
  botón de la barra superior que los abre los cierra. Con algo armado y sin guardar (`hasWork`: `dirty` desde que algo
  cambia hasta que se guarda) `Main.requestStudio()` pregunta con `ConfirmDialog` y, si se sale, el estudio se vacía
  (`discard()`). Botones a íconos; «Quitar todas» (GIF) queda lejos y pide confirmación.
- **Se eliminó la presentación de diapositivas** (botón, ajustes, `FolderModel::advanceSlideshow` y su estado).
- **PowerShell y archivos fuente**: `Get-Content`/`Set-Content` de Windows PowerShell 5.1 leen en ANSI y escriben con
  BOM; corrompen las tildes. Los scripts de edición usan Python con UTF‑8 explícito.
- **Pruebas**: `test_ui_flows` pasó a 12 recorridos (estudios que se abren y cierran con su botón, pregunta al salir con
  fotogramas, botón de información). Se repiten 15 veces seguidas sin fallos.

### Video (prueba de viabilidad, sin implementar)

Con Qt 6.7.3 Multimedia (motor FFmpeg) se abrieron, reprodujeron a 29–31 fps y se extrajeron fotogramas exactos de MP4
(H.264), MKV (H.264 y HEVC), WebM (VP9) y MKV (AV1), en Windows (FFmpeg incluido en la instalación de Qt, unos 19 MB) y
en Linux (el plugin `libffmpegmediaplugin.so` lo lleva enlazado, 19 MB). En Linux no arrancó el motor GStreamer. Sin
verificar: sonido real, ventana real en Linux, Flatpak/AppImage, videos 4K. Siguiente paso previsto: importar video al
creador de GIF y, después, un visor de video.

## 22. Canal de control para desarrollo (`DevServer`)

Solo para mirar y manejar la ventana real desde un script; no existe en las versiones publicadas. Se compila con
`-DPROJECTFOXY_DEV_SERVER=ON` (apagado por defecto, `CMakeLists.txt`) y aun así no hace nada salvo que se arranque con la
variable `PROJECTFOXY_DEV_PORT` (por ejemplo 47800). Escucha en `127.0.0.1` un protocolo de texto de una línea por
comando (`src/app/DevServer.cpp`); el cliente es `tools/devctl.py`:

- `items`: cada ítem con `objectName` (posición, tamaño, visible, habilitado, texto).
- `get <nombre> <propiedad>`: una propiedad del ítem.
- `click <nombre>`: un clic izquierdo en el centro del ítem.
- `grab <archivo.png>`: la ventana como PNG.

No sustituye a lo que depende del sistema (ajuste a los bordes, DWM, redimensionado nativo), que sigue pidiendo ratón
y pantalla reales. Los estudios se cargan bajo demanda: `gifStudio` solo existe mientras está abierto.

## 23. Lector de cómics y manga (14‑oct‑2026)

Fase 1: leer `.cbz`/`.zip` y carpetas de imágenes como un libro. **No** pasa por el visor ni por el editor: es un modo
aparte (`qml/ReaderView.qml`, un `Loader` en `Main.qml` por encima del lienzo y de los estudios, `z: 7`), así que no toca
la pila de edición ni el documento abierto.

- **`core/ZipArchive`**: lector de ZIP propio (directorio central, ZIP64, nombres UTF‑8/Latin‑1, entradas guardadas y
  *deflate* con zlib). No usa `QZipReader` (API privada de Qt) para no depender de que el SDK de Flatpak la incluya;
  zlib ya viene con las dependencias de libvips. Sin contraseña, sin otros métodos.
- **`core/ComicBook`**: la lista de páginas (carpeta o archivo, orden natural, sin `__MACOSX` ni ocultos) y, para un
  archivo, **desempaqueta cada página a un fichero** en `cacheDir()/comics/<uuid>/NNNNN.ext` la primera vez que se
  pide. Así los decodificadores de siempre (libvips, color, orientación) leen la página como cualquier imagen y no hay
  un segundo camino de decodificación. `close()` borra esa carpeta; al arrancar se borra lo que dejó una caída.
- **`core/ImageHeader.h`**: tamaño de PNG/JPEG/WebP a partir de los primeros bytes. **`core/SpreadLayout.h`**: función
  pura que da las «dobles» (portada sola opcional, las páginas apaisadas van solas y el emparejado se reanuda tras ellas).
- **`app/MangaReader`** (contexto QML `mangaReader`): posición, dirección (derecha a izquierda por defecto), página doble,
  portada sola y ajuste (pantalla/alto/ancho), todo recordado en `settings.ini` (`reader/*`) y la página de cada libro
  (`reader/pos/<sha1 de la ruta>`, 200 libros). La forma de cada página se lee **en segundo plano** (solo si la página
  doble está activa) y la maquetación se rehace cuando llegan; `m_page` es la página **pedida**, no la primera de su
  doble, para que al moverse los pares el lector siga en la página que se pidió. Las imágenes las sirve el proveedor
  `image://pane/` del comparador, con `pageSide` fijado por la vista; el lector precarga las dobles siguientes y la
  anterior con ese mismo tamaño.
- **Entrada**: `AppController::loadPath` desvía los `.cbz`/`.zip` a la señal `comicRequested` (Abrir, soltar, «Abrir
  con», línea de comandos); el botón del libro (`readButton`, cuarta herramienta opcional de la barra de título) lee la
  carpeta de la imagen abierta; el instalador registra `.cbz` y el `.desktop` su tipo MIME.
- **Pruebas:** `test_comic_book` (8: tamaños de cabecera, dobles, `.cbz` hecho a mano con entradas guardadas y
  comprimidas, carpeta, errores) y `theReaderTurnsPagesInTheDirectionOfReading` en `test_ui_flows`.
- **PDF** (Qt PDF, módulo opcional: `find_package(Qt6 OPTIONAL_COMPONENTS Pdf)`; sin él se compila con `PROJECTFOXY_PDF` sin definir y
  abrir un PDF da un aviso): `ComicBook` carga el documento con `QPdfDocument`, da la forma de cada página sin dibujarla
  (`pagePointSize`) y la dibuja a un JPEG (lado largo 2400 px, calidad 95) en la misma caché que las páginas de un `.cbz`.
  Los CI instalan el módulo (`modules: qtshadertools qtpdf`); en Windows `windeployqt` lo recoge solo. **No probado:**
  el Flatpak (hace falta Qt PDF en el runtime de KDE) ni PDF con contraseña (solo avisa).
- **Pendiente (fases 2 y 3):** modo *webtoon* continuo, página doble con corrección manual por página, lupa, recorte de
  márgenes, capítulo siguiente al llegar al final, pantalla completa, RAR/7z (libarchive; la licencia del RAR
  exige revisar `docs/LICENCIAS.md`), biblioteca con portadas y `ComicInfo.xml`. **No probado:** Flatpak/AppImage,
  `.cbz` de varios GB (el soporte ZIP64 está escrito pero ninguna prueba lo ejercita), páginas AVIF/TIFF sin forma conocida.

## 24. Nitidez FSR CAS (14‑oct‑2026)

El enfoque del programa es ahora **FidelityFX CAS** de AMD (MIT), el paso de nitidez de FSR 1. `core/edit/Cas.h` (`casChannel`)
tiene la fórmula por canal sobre el 3×3 de un píxel: mínimo/máximo suaves (la cruz más todo el 3×3), `amp = √(min(mn, 2−mx)/mx)`
(0 si el máximo es 0), pesos `w = −amp / lerp(8, 5, s)` y `(Σ vecinos·w + e) / (1 + 4w)`. La fuerza `s` (0..1) es la del
control y además reparte el resultado: `e + s·(cas − e)`, para que arranque suave y 0 sea exactamente «apagado».

- **Ajustes › Nitidez** (`applyDetail` en `AdjustMath.cpp` y `Detail.frag`): sustituye a la máscara de 5 puntos; se calcula sobre
  color premultiplicado como el resto del paso C y se le suma el término de claridad. La paridad CPU/GPU (`test_gpu_parity`) pasa.
- **Tamaño › «Mejorar nitidez al ampliar»** (`Resample.cpp`, `casBand`): sustituye al enfoque de luminancia ajustado a mano; la
  fuerza sube con la ampliación (0,5–1) y el resultado se limita al rango del 3×3 para no dejar halos (lo exige
  `enlargingBeatsPlainStretchingAndDoesNotRing`).
- **Nitidez de la vista** (botón de la barra inferior, `AppSettings::viewSharpen`, `view/sharpen`): `qml/shaders/Cas.frag` aplicado
  como `layer.effect` (`qml/CasEffect.qml`) del `Flickable` del lienzo y del lector, es decir, **sobre los píxeles de pantalla** al
  zoom actual, no sobre los de la imagen; no toca el archivo ni la pila de edición. Con 0 % la capa ni se crea.
- **Pruebas:** `test_cas` (4: apagado/plano/negro, más contraste con más fuerza, siempre en 0..1 y sin NaN, sin hueco no añade).
  **No probado:** el rendimiento con ventanas 4K y el menú con los temas Classic.

