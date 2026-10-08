# Changelog

All versions of Project Foxy. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
*(Español más abajo / Spanish below.)*

## [0.2.0] — Linux

### Linux
- **AppImage** (no installation, X11 and native Wayland; glibc 2.35 or newer) and **Flatpak** (carries its own runtime:
  also works on older distributions). Tested on Ubuntu 24.04, Debian 13, Fedora 43, Arch and AlmaLinux 9.
- Deleting sends the file to the real trash and the wallpaper is set through the desktop (GNOME, KDE, XFCE, Cinnamon,
  MATE and the desktop portals, which is what Flatpak uses).
- Fonts that only exist on Windows (Segoe UI, Arial, Impact...) fall back to free equivalents.
- File names are case-sensitive on Linux, and `PHOTO.JPG` is found like `photo.jpg`.

### Fixes
- A caption wider than the picture is shrunk until it really fits (it could stay a pixel or two too wide).
- Interface: the language option says "the system's language" instead of "Windows'".

### Under the hood
- The CI builds and tests on Windows and Linux (and builds the AppImage and the Flatpak) on every change; a release
  publishes the installer, the portable zip, the AppImage and the Flatpak with one checksum file. The program also
  passes its tests on Qt 6.10.

## [0.1.0] — First public version

### Viewer
- JPEG (CMYK too), PNG, WebP, TIFF, BMP, GIF, ICO, camera RAW, HEIC/HEIF and AVIF; animated GIF, APNG and WebP with real
  playback. EXIF orientation, file names in any alphabet and correct color (Display P3 and AdobeRGB are converted to
  sRGB).
- Thumbnail bar, GPU zoom (stepped or **smooth**), slideshow, before/after compare, copy and paste, desktop wallpaper,
  recycle bin, EXIF information with histogram.
- **Eyedropper**: with Alt held, a click copies the color as `#RRGGBB`. Pixel mode for sprites and icons.
- **Compare view** for 2 to 10 images (automatic, in a row or in a column) with linked zoom and pan.
- **Six interface languages** (Español, English, Português, 한국어, 中文, 日本語), switchable instantly in Settings.

### Editor
- Crop (with **Frame**: shapes, corners, outline, shadow and background), Size (Lanczos‑3), Adjustments (18 controls,
  levels, curves), Filters (38 looks), **Effects** (40+, including the **meme caption**), **Lens** (Lensfun profiles).
- Full undo and redo; atomic, asynchronous saving that keeps EXIF, IPTC and XMP.

### Create
- **Animated GIF**, APNG and animated WebP with transitions, and an effect and text (or meme caption) per frame.
- **Collage** of 1 to 12 photos with adjustable divider lines, free layout and your own templates.

### Other
- Batch export and rename. Four themes. Portable version and installer.

---

# Cambios (Español)

Todas las versiones de Project Foxy. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).

## [0.2.0] — Linux

### Linux
- **AppImage** (sin instalar, X11 y Wayland nativo; glibc 2.35 o más nueva) y **Flatpak** (lleva su propio runtime:
  funciona también en distribuciones más viejas). Probados en Ubuntu 24.04, Debian 13, Fedora 43, Arch y AlmaLinux 9.
- Eliminar manda el archivo a la papelera real y el fondo de pantalla se cambia a través del escritorio (GNOME, KDE,
  XFCE, Cinnamon, MATE y los portales de escritorio, que es lo que usa Flatpak).
- Las fuentes que solo existen en Windows (Segoe UI, Arial, Impact...) se reemplazan por equivalentes libres.
- Los nombres de archivo distinguen mayúsculas en Linux, y `FOTO.JPG` se encuentra igual que `foto.jpg`.

### Correcciones
- Una leyenda más ancha que la imagen se encoge hasta que cabe de verdad (podía quedar uno o dos píxeles de más).
- Interfaz: la opción de idioma dice «el idioma del sistema» en vez de «el de Windows».

### Por dentro
- El CI compila y prueba en Windows y Linux (y construye el AppImage y el Flatpak) en cada cambio; una release publica
  el instalador, el zip portable, el AppImage y el Flatpak con un único archivo de sumas. El programa también pasa sus
  pruebas con Qt 6.10.

## [0.1.0] — Primera versión pública

### Visor
- JPEG (también CMYK), PNG, WebP, TIFF, BMP, GIF, ICO, RAW de cámara, HEIC/HEIF y AVIF; GIF, APNG y WebP animados con
  reproducción real. Orientación EXIF, nombres de archivo con cualquier alfabeto y color correcto (Display P3 y AdobeRGB
  se convierten a sRGB).
- Barra de miniaturas, zoom por GPU (escalonado o **suavizado**), presentación, comparar antes/después, copiar y pegar,
  fondo de escritorio, papelera, información EXIF con histograma.
- **Cuentagotas**: con Alt apretado, un clic copia el color en `#RRGGBB`. Modo pixel para sprites e íconos.
- **Comparador** de 2 a 10 imágenes (automático, en fila o en columna) con zoom y desplazamiento vinculados.

### Editor
- Recortar (con **Marco**: formas, esquinas, contorno, sombra y fondo), Tamaño (Lanczos‑3), Ajustes (18 controles, niveles,
  curvas), Filtros (38 *looks*), **Efectos** (más de 40, incluida la **leyenda de meme**), **Lente** (perfiles de Lensfun).
- Deshacer y rehacer completos; guardado atómico y asíncrono que conserva EXIF, IPTC y XMP.

### Crear
- **GIF animado**, APNG y WebP animado con transiciones, efecto y texto (o leyenda de meme) por fotograma.
- **Collage** de 1 a 12 fotos con líneas divisorias ajustables, diseño libre y plantillas propias.

### Otros
- Exportar y renombrar por lote. Cuatro temas. Versión portable e instalador.
