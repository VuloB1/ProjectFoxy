<div align="center">

<img src="docs/assets/banner.png" alt="Project Foxy" width="100%">

# Project Foxy

**Visor de imágenes moderno, ligero y rápido para Windows, con un editor de fotos integrado.**

[![Descargar](https://img.shields.io/github/v/release/VuloB1/ProjectFoxy?label=descargar&color=ff6a13&logo=github)](https://github.com/VuloB1/ProjectFoxy/releases/latest)
![Licencia: GPL-3.0-or-later](https://img.shields.io/badge/licencia-GPL--3.0--or--later-ff6a13)
![Windows 10 / 11](https://img.shields.io/badge/Windows-10%20%7C%2011-3a3a3a?logo=windows&logoColor=white)
![Qt 6.7](https://img.shields.io/badge/Qt-6.7-41cd52?logo=qt&logoColor=white)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599c?logo=cplusplus&logoColor=white)

[Descargar](#descargar) · [Qué hace](#qué-hace) · [Compilar](#compilar) · [English](#english)

</div>

---

<p align="center">
  <img src="docs/screenshots/visor.png" alt="El visor, con la barra de miniaturas" width="49%">
  <img src="docs/screenshots/editor-efectos.png" alt="El editor: efectos de luz" width="49%">
</p>
<p align="center">
  <img src="docs/screenshots/editor-leyenda.png" alt="Leyenda de meme" width="32%">
  <img src="docs/screenshots/gif.png" alt="Creador de GIF animado" width="32%">
  <img src="docs/screenshots/collage.png" alt="Collage" width="32%">
</p>

Abre cualquier foto al instante (JPEG, PNG, WebP, TIFF, HEIC, AVIF, RAW de cámara, GIF animados…), la recorre con
su carpeta, y cuando quieres tocarla trae un **editor completo**, un **creador de GIF/APNG/WebP animados**, un
**collage sin límites**, un **comparador** de hasta 10 imágenes con zoom vinculado y más. Interfaz en español con
cuatro temas: *Moderno Claro*, *Moderno Oscuro* y dos *Classic* (estilo Windows 98).

## Descargar

Ve a la página de [**Releases**](https://github.com/VuloB1/ProjectFoxy/releases/latest) y elige:

| | |
|---|---|
| **`ProjectFoxy-Setup-x.y.z.exe`** | El instalador. No pide permisos de administrador (se instala para tu usuario), crea el acceso directo y, si quieres, deja a Project Foxy en «Abrir con» y en *Aplicaciones predeterminadas* de Windows. |
| **`ProjectFoxy-portable-x.y.z.zip`** | Versión portable: descomprime **toda** la carpeta y abre `ProjectFoxy.exe`. Guarda su configuración al lado del programa y no toca el registro. |

Requisitos: Windows 10 u 11 de 64 bits. Windows puede avisar con «Windows protegió su PC» porque el programa aún
no está firmado: *Más información → Ejecutar de todas formas*. Cada archivo de la release lleva su suma SHA-256.

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
- **Comparador**: de 2 a 10 imágenes a la vez, acomodadas solas según su forma, en una fila o en una columna, con zoom y desplazamiento vinculados que se pueden desvincular.
- **Leyenda (meme)**: franja de color con texto arriba o abajo de la imagen, o el texto sobre ella con contorno.
- **Cuentagotas**: con Alt apretado el cursor pasa a cuentagotas y un clic copia el color en `#RRGGBB`.
- **Zoom suavizado** (opcional, en Configuración) además del escalonado de siempre.
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
├── tests/      QtTest (más de 440 casos): núcleo, el AppController real y la paridad CPU/GPU
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

- Visor, formatos, animaciones, temas, configuración, lote, Ajustes, Filtros, Efectos, Lente, Marco,
  Comparador, GIF/APNG/WebP, Collage, guardado seguro y deshacer completo: **terminados**.
- Pendientes: atajos de teclado, capas de texto y formas, pinceles y máscaras, recetas por lote.
  Ver `docs/DESARROLLO.md`, sección 9.

## Licencia

**GPL-3.0-or-later**, copyright 2026 Vulito (ver [`LICENSE`](LICENSE)). Es la coherente con exiv2 (GPL). Las
licencias de las demás librerías están en [`docs/LICENCIAS.md`](docs/LICENCIAS.md) y en la carpeta `licenses` de cada
descarga. Los perfiles de lente son de [Lensfun](https://lensfun.github.io/) (CC BY-SA 3.0).

## Contribuir

Los errores y las ideas son bienvenidos: abre un *issue* (hay plantillas) o lee [`CONTRIBUTING.md`](CONTRIBUTING.md).

---

## English

**Project Foxy** is a fast, lightweight image viewer for Windows with a full photo editor built in. It opens JPEG, PNG,
WebP, TIFF, HEIC/AVIF, camera RAW and animated GIF/APNG/WebP; edits with adjustments, filters, 40+ effects, lens
correction (Lensfun) and picture frames; creates animated GIF/APNG/WebP and unlimited-photo collages; compares up to
10 images with linked zoom; and saves safely (atomic writes, EXIF/IPTC/XMP kept). The interface is in Spanish.

**Install:** download `ProjectFoxy-Setup-x.y.z.exe` (no administrator rights needed) or the portable `.zip` from
[Releases](https://github.com/VuloB1/ProjectFoxy/releases/latest). **Build:** see [Compilar](#compilar).
**License:** GPL-3.0-or-later.
