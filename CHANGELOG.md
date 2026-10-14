# Changelog

All versions of Project Foxy. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
*(Español más abajo / Spanish below.)*

## [Unreleased]

- Windows: fixed a crash when clicking the maximise button (0.2.8). The program wrote to a null pointer that Windows
  passes for that click.
- The window now remembers its size, position and whether it was maximised, and opens the same way next time. If the
  saved size is larger than the screen now connected (for example it was saved on a 1440p monitor and you open it on a
  1080p one), it is brought down to fit, and it is never left off-screen.

## [0.2.8] — The title bar is the toolbar

- The toolbar is now part of the title bar: the file name and the tools (Edit, Animated GIF, Collage, Compare, More tools,
  Settings) sit next to the window buttons, and everything that is not a button is still grab-and-drag space. As the
  window narrows, Animated GIF, Collage and Compare move into the "More tools" menu (Edit and Settings always stay in
  the bar), and the file name never takes more than half of the bar.
- Windows: minimising and restoring from the taskbar play the system animation again.
- The picture still opens fitted above the bottom bar, but zoomed in it now also shows beside the bar instead of leaving
  dark gaps there.

## [0.2.7] — Neutral Spanish

- The Spanish text of the interface no longer uses the Argentine *voseo* («elegí», «arrastrá»…): it is now neutral
  Spanish addressing the user as *tú* («elige», «arrastra»…). The other languages are unchanged.

## [0.2.6] — Window edges and V-Sync

- Windows: the window with its own title bar snaps to the screen edges again. Drag it to the top to maximise it, or to
  the sides to tile it; Windows 11's snap layouts appear on the maximise button; the system does the resizing from the
  edges and corners. Maximised, it fills the work area without covering the taskbar.
- New setting **Sync with the screen (V-Sync)** (Settings, Appearance, Windows), on by default. On some AMD graphics
  cards (seen on a Radeon RX 6750 XT) the window froze for 100 ms to 2 s while it was resized with the Direct3D 11
  renderer; turning V-Sync off avoids that. It may show tearing on screens without FreeSync or G-Sync. It applies the next
  time the program is opened.
- For developers: a local control channel and tools to measure what happens while a window is resized (off by default
  and not part of the releases), and the findings in `docs/DESARROLLO.md`.

## [0.2.5] — A new look

- The window has its own title bar, in the program's theme (rounded corners and a shadow on Windows).
- A bottom bar: the position and colour of the pixel under the mouse (a click with Alt copies the colour and says
  "Copied" next to the cursor), the picture's size and zoom, the information button, and in the middle a raised piece
  fused with the bar that holds every browse and edit button, as icons. The comparer puts its controls there too, and
  the picture fits above it.
- The tab of the thumbnail bar is always there, with a rounded join to the bar; the "With tab" mode is now "Manual".
- Settings: taller, the chosen page tinted, the same wheel speed as the thumbnail bar, and a new **About** page (the
  program, its author, links, acknowledgements and licences). The top bar's icons have thinner lines.
- The animated-GIF and collage studios: title and icon on top of the right panel, buttons as icons, and the button that
  opens a studio closes it. If something made has not been saved, it asks before leaving. In the GIF studio, reversing
  the order and adding the whole folder moved to the strip of frames, and "Remove all" sits apart and asks first.
- Batch export and batch rename follow the same design (header and cards).
- **One window**: opening a picture from Explorer uses the window that is already open (it can be switched off in
  Settings).
- Removed: the slideshow, and the "pixel readout" setting (the readout is always in the bottom bar).
- Fixed: a WebP animation whose header claimed a gigantic canvas could ask for gigabytes of memory; closing the program
  while thumbnails were loading could crash.
- Under the hood: the controller of the interface was split into smaller pieces, GIF/APNG/WebP decoding is fuzz-tested
  in CI, and 12 tests drive the real interface.

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

## [Sin publicar]

- Windows: arreglado un cierre inesperado al hacer clic en el botón de maximizar (0.2.8). El programa escribía en un
  puntero nulo que Windows entrega en ese clic.
- La ventana recuerda su tamaño, su posición y si estaba maximizada, y se abre igual la próxima vez. Si el tamaño
  guardado es mayor que la pantalla conectada ahora (por ejemplo, se guardó en un monitor de 1440p y la abres en uno de
  1080p), se reduce para que quepa y nunca queda fuera de la pantalla.

## [0.2.8] — La barra de título es la barra de herramientas

- La barra de herramientas pasa a formar parte de la barra de título: el nombre del archivo y las herramientas (Editar,
  GIF animado, Collage, Comparar, Más herramientas, Configuración) quedan junto a los botones de la ventana, y todo lo
  que no es un botón sigue siendo zona para arrastrar. Al estrecharse la ventana, GIF animado, Collage y Comparar pasan
  al menú «Más herramientas» (Editar y Configuración siempre quedan a la vista) y el nombre del archivo nunca ocupa más
  de la mitad de la barra.
- Windows: minimizar y restaurar desde la barra de tareas vuelven a mostrar la animación del sistema.
- La imagen se sigue abriendo ajustada por encima de la barra inferior, pero con zoom también se ve a los lados de la
  barra, en lugar de dejar huecos oscuros.

## [0.2.7] — Español neutro

- Los textos de la interfaz en español dejan de usar voseo («elegí», «arrastrá»…): ahora están en español neutro,
  tuteando («elige», «arrastra»…). Los demás idiomas no cambian.

## [0.2.6] — Bordes de la ventana y V-Sync

- Windows: la ventana con barra de título propia vuelve a ajustarse a los bordes de la pantalla. Arrástrala arriba para
  maximizarla o a los costados para acomodarla; los diseños de ajuste de Windows 11 aparecen en el botón de maximizar; y
  el sistema se encarga de redimensionar desde los bordes y las esquinas. Maximizada, ocupa el área de trabajo sin tapar
  la barra de tareas.
- Ajuste nuevo: **Sincronizar con la pantalla (V-Sync)** (Configuración, Apariencia, Ventanas), activado por defecto. En
  algunas tarjetas AMD (visto en una Radeon RX 6750 XT) la ventana se congelaba de 100 ms a 2 s al redimensionarla con el
  motor Direct3D 11; apagar el V-Sync lo evita. Puede mostrar cortes en la imagen en pantallas sin FreeSync ni G-Sync. Se
  aplica la próxima vez que abras el programa.
- Para desarrolladores: un canal de control local y herramientas para medir qué pasa al redimensionar la ventana
  (apagados por defecto y fuera de las versiones publicadas), y lo que se encontró en `docs/DESARROLLO.md`.

## [0.2.5] — Una nueva imagen

- La ventana tiene su propia barra de título, con el tema del programa (esquinas redondeadas y sombra en Windows).
- Una barra inferior: la posición y el color del píxel bajo el mouse (un clic con Alt copia el color y dice «Copiado»
  junto al cursor), el tamaño y el zoom de la imagen, el botón de información y, en el medio, una pieza más alta fusionada
  con la barra que lleva todos los botones de ver y de editar, como íconos. El comparador pone sus controles ahí también
  y la imagen se ajusta por encima.
- La pestaña de la barra de miniaturas está siempre; se une a la barra con un empalme redondeado y el modo «Con pestaña»
  ahora es «Manual».
- Configuración: más alta, la página elegida teñida, la misma velocidad de rueda que la barra de miniaturas y una página
  nueva, **Acerca de** (el programa, su autor, enlaces, agradecimientos y licencias). Los íconos de la barra superior
  tienen líneas más finas.
- Los estudios de GIF animado y de collage: el título y el ícono arriba del panel derecho, botones como íconos, y el botón
  que abre un estudio lo cierra. Si hay algo armado sin guardar, pregunta antes de salir. En el estudio de GIF, invertir
  el orden y agregar toda la carpeta pasaron a la tira de fotogramas, y «Quitar todas» queda aparte y pregunta antes.
- Exportar por lote y renombrar por lote siguen el mismo diseño (encabezado y tarjetas).
- **Una sola ventana**: al abrir una imagen desde el Explorador se usa la ventana que ya está abierta (se puede apagar en
  Configuración).
- Se quitó la presentación de diapositivas y el ajuste «Lectura del píxel» (la lectura está siempre en la barra inferior).
- Arreglado: una animación WebP cuyo encabezado decía tener un lienzo gigantesco podía pedir gigabytes de memoria; cerrar
  el programa mientras se cargaban miniaturas podía hacerlo caer.
- Por dentro: el controlador de la interfaz se dividió en piezas más chicas, la lectura de GIF/APNG/WebP se prueba con
  fuzzing en el CI y 12 pruebas manejan la interfaz real.

## [0.2.4] — Más liviano con fotos grandes

- Las fotos grandes (de unos 21 megapíxeles en adelante) usan mucha menos memoria gráfica: mientras una se ve ajustada a
  la ventana, o con solo ediciones de tono y color, se dibuja una copia reducida en vez de la imagen entera (con una foto
  de prueba de 37 megapíxeles, la memoria gráfica en el visor pasó de 236 MB a 93 MB, y con un ajuste de tono de 616 MB a
  196 MB). Al acercar el zoom o usar nitidez, claridad o grano se pasa a la imagen completa, así que lo que ves es lo que
  guardas.

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
