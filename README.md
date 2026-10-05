# Project Foxy

Visor de imágenes moderno, ligero y rápido **para Windows**, con un editor de
fotos integrado. Interfaz en español y tres temas: *Moderno Claro*, *Moderno
Oscuro* y *Windows 98*.

> Documentación técnica completa (arquitectura, pipeline de edición, decisiones,
> riesgos conocidos y hoja de ruta): [`docs/DESARROLLO.md`](docs/DESARROLLO.md).

## Qué hace

**Visor**
- Abre un archivo (diálogo, arrastrar y soltar, "Abrir con" o menú contextual del
  Explorador) y recorre su carpeta con orden natural.
- Barra lateral colapsable con miniaturas (caché en memoria y en disco),
  zoom/desplazamiento por GPU, presentación, comparar antes/después,
  copiar/pegar, fondo de escritorio, papelera, información EXIF con histograma.
- **Color correcto**: las fotos en Display P3 (iPhone) o AdobeRGB se convierten a sRGB al
  abrirlas, en lugar de verse apagadas; lo que se guarda va etiquetado como sRGB.
- Respeta la **orientación EXIF** (fotos de móvil) y abre archivos y carpetas con
  cualquier nombre (acentos, japonés, cirílico, emoji).
- GIF, APNG y WebP animados con reproducción real.
- **Varias imágenes** a la vez (2 a 6) con zoom y desplazamiento vinculados, que se pueden desvincular.
- Exportación por lote (JPG/WebP, conserva los metadatos) y configuración persistida
  en un `.ini`.

**Formatos que abre** (cada uno comprobado con un archivo real en las pruebas):
JPEG (también CMYK), PNG, WebP y TIFF (libvips; los de 16 bits por canal se escalan a 8) ·
BMP, GIF e ICO (Qt) · RAW de cámara (LibRaw) · HEIC/HEIF y AVIF (libheif) · GIF, APNG y WebP animados. **Guarda** en PNG, JPG, BMP, TIFF y WebP, y **crea** GIF, APNG y WebP animados.

**Guardado seguro**: escritura atómica (un fallo nunca deja el original a medias), JPEG
de alta calidad (95, sin submuestreo de croma), conserva EXIF, IPTC y XMP del
original en JPEG, PNG y WebP (desde HEIC/RAW se guarda **avisando** de que sus
metadatos no se conservan), pide confirmación antes de sobrescribir y avisa de los cambios
sin guardar al cerrar o abrir otra imagen. El guardado **no congela la ventana** (se hace en segundo plano; con «Guardando…» y
«Guardado» en pantalla; cerrar durante el guardado espera a que termine). Nunca guarda
mientras otra imagen se está cargando, y «sin guardar» significa «la pantalla no coincide con el archivo» (también
tras Guardar y Deshacer).

**Editor**
| Pestaña | Contenido |
|---|---|
| Recortar | Recorte con tiradores, proporciones, rotar, enderezar, voltear · **Marco**: forma (rectángulo, elipse, hexágono, estrella, corazón…), esquinas redondas, suaves, cortadas o cóncavas, contorno, margen, sombra y fondo |
| Tamaño | Por píxeles o escala, remuestreo Lanczos‑3 con nitidez sin halos al ampliar |
| Ajustes | 18 controles de tono/color/detalle, niveles y curvas por canal, auto, histograma en vivo, grano/ruido (4 tipos) |
| Filtros | 38 "looks" con miniaturas de la propia foto, cantidad, viñeta y grano |
| Efectos | 43 efectos en 7 grupos (desenfoque, estilo, color, dibujo, luz, distorsión y acabado: blanco y negro, borrar niebla, celofán, semitono de imprenta, líneas, destellos, bokeh, estirar, perspectiva…) con vista previa en toda la imagen, sliders propios, preajustes y mezcla; "Aplicar" los deja como un paso deshacible |
| Lente | Corrige la distorsión, las franjas de color y las esquinas oscuras de tu objetivo con la base de perfiles de Lensfun (reconoce la cámara y el objetivo por el EXIF), o a mano |
| Crear (barra superior) | **GIF animado** (imágenes con duración, efecto y texto propios, transiciones, GIF/APNG/WebP) y **Collage** (1 a 12 fotos, líneas divisorias ajustables o diseño libre, fotos que se mueven y se acercan sin límites dentro de su celda, plantillas propias guardables) |

La vista previa de Ajustes y Filtros se calcula en la GPU y el archivo guardado
se genera en CPU con la misma matemática, **también con imágenes semitransparentes**; una
prueba automática (`test_gpu_parity`) las compara con Direct3D 11 (ver
`docs/DESARROLLO.md`, sección 4).
**Deshacer/Rehacer** cubre todo, incluidos los movimientos de Ajustes y Filtros (un
arrastre completo de un slider es un solo paso).

## Stack
- C++20 · Qt 6.7 (Qt Quick/QML, RHI con Direct3D 11)
- libvips (JPEG/PNG/WebP/TIFF) · LibRaw (RAW) · libheif + libde265 + libaom
  (HEIC/HEIF/AVIF) · exiv2 (metadatos EXIF/IPTC/XMP)
- CMake + Ninja + MSVC 2022, dependencias nativas con vcpkg

## Estructura
```
├── CMakeLists.txt, CMakePresets.json, vcpkg.json
├── src/
│   ├── app/    capa que depende de Qt Quick (biblioteca `imageviewer_app`): AppController (puerta principal
│   │           hacia QML), ImageProvider, FolderModel, ThemeManager,
│   │           AppSettings, BatchExporter, LensController, AnimStudio, CollageStudio,
│   │           proveedores de imágenes (miniaturas, paneles, vistas previas)
│   └── core/   biblioteca estática sin QML
│       ├── ImageLoader, DecoderRegistry, ThumbnailCache, MetadataReader,
│       │   Histogram, ImageWriter
│       ├── decoders/  Vips, QtImage (BMP/GIF/ICO), Raw, Heif y Animated (GIF/APNG/WebP)
│       ├── edit/      Operations, EditStack (deshacer/rehacer), AdjustMath, Looks,
│       │              Effects (por familias), Blend, Resample, ParallelRows
│       ├── lens/      LensDatabase (perfiles de Lensfun)
│       ├── anim/      codificadores de GIF, APNG y WebP animado, armado de fotogramas
│       └── collage/   dibujo y diseños del collage
├── qml/        ventana, lienzo, panel de edición, controles tematizados
│   ├── controls/  envoltorios App*.qml de cada control + iconos vectoriales
│   └── shaders/   Grade.frag y Detail.frag (vista previa en GPU)
├── tests/      QtTest (430 casos): núcleo, el AppController real y la paridad CPU/GPU
│               con Direct3D 11 + prueba de humo del programa real
│               + fixtures (un archivo de ejemplo de cada formato, EXIF/XMP/IPTC, CMYK…)
├── tools/      archivos .reg del menú contextual y generador de avisos de terceros
└── docs/       documentación de desarrollo y de licencias
```

`src/core` no depende de QML: toda la decodificación, la caché y la edición se
prueban de forma aislada. La capa QML solo consume esa lógica a través de
`src/app`.

## Compilar

Requisitos: Windows, Visual Studio 2022 (o Build Tools), CMake ≥ 3.21, Ninja,
Qt 6.7.3 (la versión validada; el mínimo declarado es 6.6) y vcpkg (`VCPKG_ROOT` definido).

```bash
# copia CMakeUserPresets.json.example a CMakeUserPresets.json y ajusta CMAKE_PREFIX_PATH
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
cmake --install build/release --prefix dist     # la carpeta completa para entregar
```

El preset `windows-release` exige exactamente Qt 6.7.3; con otra versión la configuración
falla (el de depuración solo avisa). La prueba `smoke_startup` arranca el programa real
sin ventana visible y falla si muere o escribe cualquier mensaje al arrancar. La prueba
`test_gpu_parity` necesita un dispositivo Direct3D 11 y se omite sola si no lo hay.
`tools/Test-Install.ps1` instala en una carpeta temporal y comprueba que el programa
instalado arranca sin depender de nada de esta máquina.

`pkg-config` no hace falta instalarlo aparte: `vcpkg.json` declara `pkgconf`
como herramienta del host y `CMakeLists.txt` lo localiza solo.

## Estado
- Visor, formatos, animaciones, temas, configuración, lote, Ajustes, Filtros,
  Efectos, guardado seguro y deshacer completo: **terminados**.
- Licencia del proyecto: **GPL-3.0-or-later**, copyright 2026 Vulito (ver `LICENSE`). Es la
  coherente con exiv2 (GPL); las licencias de las demás dependencias están en
  `docs/LICENCIAS.md`.
- Desde el 4‑5 de octubre también: más efectos, corrección de lente, marcos, varias imágenes,
  creador de GIF/APNG/WebP y collage (ver `docs/DESARROLLO.md`, secciones 4.13 a 4.18).
- Pendientes: GitHub privado y CI, atajos de teclado, capas de texto y formas, pinceles y máscaras,
  recetas por lote. Ver `docs/DESARROLLO.md`, sección 9.
