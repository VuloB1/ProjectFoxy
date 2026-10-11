# Changelog

All versions of Project Foxy. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
*(Español más abajo / Spanish below.)*

## [0.2.4] — Lighter on large photos

- Large photos (about 21 megapixels and up) use much less graphics memory: while one is shown fitted to the window,
  or with only tone and colour edits, a reduced copy is drawn instead of the whole image (on a 37-megapixel test photo
  the graphics memory in the viewer went from 236 MB to 93 MB, and with a tone edit from 616 MB to 196 MB). Zooming in
  or using sharpness, clarity or grain switches to the full picture, so what you see is what you save.

## [0.2.3] — Safer and smoother

- Animated GIF, APNG and WebP files are opened within memory limits (file size, picture size, total decoded size and
  number of frames). A file over the limits is refused with a message; an animation too long for them plays the frames
  that fit.
- HEIC/AVIF previews (the thumbnail strip) use the thumbnail stored in the file when it is big enough, instead of
  decoding the whole picture.
- Linux: setting the wallpaper no longer freezes the window; the real answer of the desktop portal is awaited and a
  notice says whether it worked (Windows shows the notice too).
- The folder is listed in the background, so a big or slow folder does not block the window, and the thumbnail strip
  follows files added, removed or renamed by other programs, staying on the picture you are viewing.

## [0.2.2] — Robustness

- Closing the program while a batch export, an animation or a collage is being written (or while the lens database
  loads) now waits for that work to stop instead of risking a crash.
- Previews of animated GIF/APNG/WebP files are made at the size asked for, not from the whole first frame.
- Linux: the thumbnail store is the one the freedesktop specification names (`XDG_CACHE_HOME`; the host's inside a
  Flatpak), and a stored thumbnail is checked against the file's URI and size too. Deleting through the portal passes a
  descriptor it can write to.
- Releases: building one now fails if the version asked for differs from the one in `CMakeLists.txt`, `vcpkg.json` or
  `resources/app.rc`. The CI checks the AppStream metadata.

## [0.2.1] — Faster browsing

- Moving from one picture to the next in a folder no longer lists, sorts and rebuilds the whole folder each time
  (it showed as a pause of a few hundred milliseconds on big folders). The folder is only read again when it changes.
- The thumbnail strip first looks for a thumbnail the system already made (Windows: Explorer's thumbnail cache;
  Linux: `~/.cache/thumbnails`) and only decodes the original when there is none. Thumbnails also load at low
  priority and are skipped when they scroll out of view, so they do not slow down opening a picture.

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

## [0.2.4] — Más liviano con fotos grandes

- Las fotos grandes (de unos 21 megapíxeles en adelante) usan mucha menos memoria gráfica: mientras una se ve ajustada a
  la ventana, o con solo ediciones de tono y color, se dibuja una copia reducida en vez de la imagen entera (con una foto
  de prueba de 37 megapíxeles, la memoria gráfica en el visor pasó de 236 MB a 93 MB, y con un ajuste de tono de 616 MB a
  196 MB). Al acercar el zoom o usar nitidez, claridad o grano se pasa a la imagen completa, así que lo que ves es lo que
  guardás.

## [0.2.3] — Más seguro y fluido

- Los GIF, APNG y WebP animados se abren dentro de límites de memoria (tamaño del archivo, de la imagen, total
  decodificado y cantidad de fotogramas). Un archivo que los pasa se rechaza con un mensaje; una animación demasiado
  larga reproduce los fotogramas que caben.
- Las vistas previas de HEIC/AVIF (la barra de miniaturas) usan la miniatura guardada en el archivo cuando es lo bastante
  grande, en vez de decodificar toda la imagen.
- Linux: poner el fondo de escritorio ya no congela la ventana; se espera la respuesta real del portal del escritorio y
  un aviso dice si funcionó (Windows también muestra el aviso).
- La carpeta se lista en segundo plano, así una carpeta grande o lenta no bloquea la ventana, y la barra de miniaturas
  sigue los archivos agregados, quitados o renombrados por otros programas, sin salirse de la imagen que estás viendo.

## [0.2.2] — Robustez

- Cerrar el programa mientras se escribe una exportación por lote, una animación o un collage (o mientras carga la base
  de datos de lentes) ahora espera a que ese trabajo se detenga, en vez de arriesgar un fallo.
- Las vistas previas de GIF/APNG/WebP animados se hacen al tamaño pedido, no a partir del primer fotograma completo.
- Linux: el almacén de miniaturas es el que nombra la especificación freedesktop (`XDG_CACHE_HOME`; el del anfitrión
  dentro de un Flatpak), y una miniatura guardada se comprueba también contra la URI y el tamaño del archivo. Eliminar
  por el portal pasa un descriptor en el que se puede escribir.
- Versiones: compilar una release falla si la versión pedida difiere de la de `CMakeLists.txt`, `vcpkg.json` o
  `resources/app.rc`. El CI revisa los metadatos de AppStream.

## [0.2.1] — Navegación más rápida

- Pasar de una imagen a la siguiente dentro de una carpeta ya no lista, ordena y reconstruye la carpeta entera cada
  vez (se notaba como una pausa de unos cientos de milisegundos en carpetas grandes). La carpeta solo se vuelve a leer
  cuando cambia.
- La barra de miniaturas busca primero una miniatura que el sistema ya hizo (Windows: la caché de miniaturas del
  Explorador; Linux: `~/.cache/thumbnails`) y solo decodifica el original si no hay ninguna. Además las miniaturas
  cargan con prioridad baja y se saltean si salen de pantalla, para que no frenen la apertura de una imagen.

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
