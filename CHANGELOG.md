# Changelog

All versions of Project Foxy. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
*(Español más abajo / Spanish below.)*

## [Unreleased]

- **Fixed: "Start with Windows" did nothing.** The program wrote nothing to Windows' list of programs that start with it (two slips in the
  code that writes the entry), so it never started by itself. It does now, with or without the window (*Start minimized in the tray*).
  If you turned it on before, turn it off and on again.
- **GIF studio, crop:** *Recorte* chooses the part of the pictures that is kept (the same one for all of them, with the usual proportions
  or free); the animation takes its proportions from it. As in the video page.
- **GIF studio, lighter animations:** *Fotogramas que se usan* keeps one picture of every 2, 3, 4 or 6; the time of the ones left out is
  added to the one that stays, so the animation lasts the same and weighs less.
- **Colours:** the eyedropper copies the colour as digits only (`0B27E3`), without the `#`. The readout in the bar still shows it with `#`.
- **Pictures and their icons:** Windows shows, for each file, the icon of the program that opens it, and Foxy can only be offered, not
  imposed (that is why `.webp` shows the fox and `.jpg` shows another program's). Settings > Windows > *Abrir las imágenes con Project Foxy*
  takes you to the page of Windows where you choose it for each format.

## [0.3.8] — Wheel on the frame strip

- **GIF studio:** the mouse wheel scrolls the strip of frames sideways (it runs horizontally, so the wheel had nothing to move).

## [0.3.7] — Comic reader, collage and GIF studio

- **Reader:** a book (.cbz, .zip, .pdf or a folder) can be dropped on the reader to open it, and the page you are on shows at the top
  ("Page 20-21 of 186"), with a thin line along the edge that fills as you read.
- **Collage maker and GIF studio:** files dragged from any folder are taken anywhere on the studio, and while they are over it the
  window says so (in the collage the cell they would replace is marked). The mouse moves that drag a cell, a handle or a dividing line
  are now added up once per frame, so a fast mouse no longer redraws the collage hundreds of times a second.
- **Menus of the studios:** each section has an icon before its title, the long explanations moved into a small "i" with a tip, and the
  fold arrow is on the right. The size fields (GIF, collage, video) have no more up and down arrows: click and drag left or right to
  change the value, or click to type it.
- **Collage, zoom:** each step of the zoom no longer decodes the whole photo again at a new size (it took a second or more for a big
  one): the preview asks for a few fixed sizes and keeps them, and the wheel steps that arrive between two frames are applied once.
  The saved file is not limited by this.
- **Collage, moving a zoomed picture:** the limit of how far a picture could be moved was the same whatever the zoom, so a zoomed
  picture stopped short of its edges. It now grows with the picture (and is still at least what it was).

## [0.3.6] — Text in pictures

- **Read the text of a picture** (Windows only, with the recogniser Windows itself uses; nothing is downloaded and the picture never leaves the
  computer). The *Reconocer texto* button in the dock (or Ctrl+T, or right click) darkens what is not text and leaves each line,
  and the lines of a paragraph together, as they are, with the corners of the theme. Drag to select, double click for a line, triple
  click for a paragraph, Ctrl+A for everything, Ctrl+C or right click to copy. It reads the languages installed in Windows.
- **The same on Linux, with Tesseract** (Apache-2.0), in the AppImage and the Flatpak. English and Spanish come with the program; Settings >
  *Reconocimiento de texto* lists 30 more that are downloaded when asked (1-4 MB each, to your data folder) and can be removed, and
  which languages are read is chosen there. The Flatpak gets network access only for that download.
- The *Reconocer texto* icon is now three lines inside the corners.

## [0.3.5] — Stay in the tray and start with Windows

- **Stay open in the tray** (Settings > Appearance > Windows, Windows only, off by default). Closing the window only hides it and leaves an
  icon next to the clock: click it, or open a picture from Explorer, and the window is back in about 0.1 s. Right click: *Abrir
  Project Foxy* / *Salir* (quit; an unsaved edit still asks first). While hidden it frees its graphics and gives the memory back to
  Windows: about 3 MB in Task Manager and no GPU memory, against about 130 MB and 50 MB with a picture open.
- **Start with Windows**, with the option **Start minimized in the tray**: it starts without a window (about 1-2 MB) and opens
  instantly when needed. It writes the per-user *Run* entry (the portable version too, when you turn it on).

## [0.3.4] — Videos that did not open

- Fixed: some MP4s (Twitter downloads, for one) stayed on "Abriendo el vídeo…" for ever, or lost frames when turned into a GIF. They
  report every frame as lasting 1.7 ms, so no position fell inside a frame; a frame now lasts at least one frame period.

## [0.3.3] — Holiday decorations and video to GIF

- **Discreet decorations for the holidays**, on by default and only on the modern themes: a thin garland along the top
  edge of the picture area and a small piece in the bottom corner of the thumbnail bar (Halloween also gets a cobweb in
  the corner of the picture). Nothing takes the mouse, and the thumbnails never go under the piece: the bar simply
  leaves room for it. Halloween 1-31 Oct, Day of the Dead 1-3 Nov, Christmas 1-26 Dec, New Year 27 Dec-2 Jan, Three
  Kings' Eve 3-6 Jan, Valentine's Day 7-15 Feb, St. Patrick's Day 12-17 Mar and Easter (the 7 days before Easter Sunday
  and the Monday after it; it wins over St. Patrick's Day when they overlap).
- Settings > Appearance > *Adornos festivos*: a switch for all of them, one for each holiday, a switch to hide the piece
  in the thumbnail bar (the garland stays), and a list to look at any holiday out of season (it goes back to
  "by date" when the program restarts).
- The pictures are drawn by `tools/make_festive_art.js` (plain SVG, `resources/festive/`); the dates are
  `core/Festivity` (tested in `test_festivity`).

- **Make an animation from a video.** In the animation studio (Create > Animated GIF) use the same *Agregar imágenes o vídeo…* button as for
  pictures (or drop the video on the studio): pick the piece to keep on a timeline, **cut out parts in the middle** (cut at the cursor, then
  leave out the pieces you do not want), **crop the picture** (1:1, 16:9, 4:3, 3:4, 9:16 or free: drag the frame and its
  corners), and choose the frames per second (5-20) and the width. The frames go into the studio's strip, where the usual
  tools (text, effects, size, speed, GIF/APNG/WebP) apply. Limits: 600 frames; the page warns over 30 s or about 25 MB.
- Reads MP4 (H.264/H.265), MOV and MKV (Qt Multimedia; WebM/VP9/AV1 are not available with Qt's Windows build). A 36 s
  1440p60 clip becomes 358 frames in about 9 s: the video is played at 4x and only the frames needed are kept.
- **One Open for everything.** The Open button (and Ctrl+O, drag and drop, "Open with" and the command line) takes
  pictures, comics and videos: a video goes straight to the animation studio's video page.
- **GIF studio, easier and lighter.** The duration of the frames is one value for all of them (20 to 300 ms, with
  buttons for the usual ones; it used to be 20 ms to 5 s and one frame at a time, and new pictures stay 100 ms), and the
  effect and text are also for the whole animation. Dropdowns of a few options (transitions, repetitions, kind of text,
  effect presets, sizes) are now buttons. A **Compression** bar sets the colours of the GIF (256 down to 16) and the studio
  works out the file's weight in the background ("It will weigh about 1.2 MB"). The video page offers more small widths
  (240, 160 and 120 px). While one studio is open the other one's button is greyed out, and "Remove all" is a red cross.
- **Faster start** (the picture is on screen about 0.65 s after launching instead of 0.9 s with a 24-megapixel JPEG;
  0.54 s instead of 0.8 s without a file): the fonts of the interface (and their fallbacks for Asian scripts and emoji)
  and the graphics driver are loaded on a helper thread while the window is built, the dialogs of the title bar (Settings,
  batch tools, Open) are built when first used, and the video player is only created when a video is opened.
- Adds Qt Multimedia to the program (the installer grows by about 6 MB).

## [0.3.2] — Updates from inside the program

- **A notice when a newer version is published.** At start (at most once a day, five seconds after the window opens) the
  program asks GitHub's public API which is the latest release and, if it is newer than the one you run, shows a small
  card at the top right: *Update now*, *What is new* and *Skip this version*. The only thing sent is that one HTTPS
  request, with the program's name and version in the User-Agent. It is on by default and can be turned off, or run by
  hand, in Settings > About.
- **One-click update on Windows.** When the program was installed with its installer, *Update now* downloads the
  installer, checks it against the release's `SHA256SUMS.txt` (a download that does not match is deleted, and a release
  without sums is never run) and, with *Restart and install*, runs it silently and starts the program again. It will not
  restart while the open picture has unsaved edits. The portable zip, Linux and Flatpak only get the notice (they
  update by their own means).
- The installer starts the program again after a silent update (`/update=1`).

## [0.3.1] — Faster opening

- **Next and previous pictures open instantly.** While you look at a picture, the ones before and after it are decoded
  in the background (up to 4 pictures or half a gigabyte), so the arrow keys no longer wait for the decoder: about
  20 ms instead of 300 ms with 24-megapixel photos. A file that changes on disk is read again.
- **A reduced look at big JPEGs, at once.** A JPEG of a megabyte or more shows a reduced version (up to 2560 pixels)
  while the whole picture finishes decoding; the open document does not change until the whole one is there, and if it
  fails to load the screen goes back to what it was.
- **The picture on the command line starts decoding earlier**, while the window is still being built, and libvips is
  warmed up in the background. Opening a 24-megapixel PNG from Explorer went from 660 ms to 340 ms.
- Big pictures are reduced for display off the interface thread, and libvips' buffer is used as it is (no copy of
  tens of megabytes).
- **No more white flash when the window opens** (Windows paints a new window white until its first frame is ready):
  the window stays hidden until that frame exists and its background is the theme's colour.
- **Installer:** no licence page, the Project Foxy logo in the wizard and its own picture on the "installation complete"
  page (made by `tools/make_installer_images.py`).
- Developer tool `bench_load` times each stage of opening a picture.

## [0.3.0] — Comic and manga reader

- **Comic and manga reader.** Open a `.cbz`, `.zip` or `.pdf` file like any picture, or press the new book button in the
  title bar to read the folder of the picture you have open. Pages are read right to left by default (the manga way) or
  left to right, one at a time or two side by side, with the cover standing alone and landscape pages (already a spread)
  shown by themselves. Left and right (or the click zones at both sides) turn the page, the wheel scrolls what does not
  fit and then turns it, and the dock of the bottom bar has a page slider and the switches for direction, double page, cover and
  fit (screen, height or width). The book remembers the page you were on. Keys: **D** double page, **R** direction,
  **C** cover alone, **1 2 3** fit, **Home/End** first/last page, **Esc** closes it. Next and previous pages are
  unpacked and decoded ahead, so turning the page does not wait. First version: CBZ, PDF and folders (RAR and 7z
  later).
- **FSR CAS sharpening.** The sharpening is now AMD's FidelityFX CAS (the pass FSR 1 uses): it adapts to the local
  contrast, so it adds crispness where there is room for it and leaves flat areas and noise alone, with far fewer
  halos than the old unsharp mask. It replaces the *Sharpness* slider of Adjustments (same preview and saved file) and
  the sharpening after enlarging a picture in *Size*. New button at the bottom right of the window (a small triangle,
  lit while it is on): a menu with a 0-100 % bar that sharpens what you see on screen while you read a comic or manga (the button
  appears only then). 0 % is off; it never changes the file. Remembered between runs.
- Windows: fixed a crash when clicking the maximise button (0.2.8). The program wrote to a null pointer that Windows
  passes for that click.
- The window now remembers its size, position and whether it was maximised, and opens the same way next time. If the
  saved size is larger than the screen now connected (for example it was saved on a 1440p monitor and you open it on a
  1080p one), it is brought down to fit, and it is never left off-screen.
- Long file names (without spaces) now wrap inside the "unsaved changes", "replace file" and delete dialogs instead of
  running out of the box.

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

- **Corregido: «Iniciar con Windows» no hacía nada.** El programa no escribía nada en la lista de programas que arrancan con Windows (dos
  descuidos en el código que escribe la entrada), así que nunca arrancaba solo. Ahora sí, con o sin ventana (*Iniciar minimizado en la
  bandeja*). Si lo activaste antes, apágalo y vuelve a encenderlo.
- **Estudio de GIF, recorte:** *Recorte* elige la parte de las imágenes que se queda (la misma para todas, con las proporciones de siempre
  o libre); la animación toma sus proporciones de él. Como en la página de vídeo.
- **Estudio de GIF, animaciones más ligeras:** *Fotogramas que se usan* deja una imagen de cada 2, 3, 4 o 6; el tiempo de las que se saltan
  se suma a la que queda, así la animación dura lo mismo y pesa menos.
- **Colores:** el cuentagotas copia el color solo con los dígitos (`0B27E3`), sin el `#`. La lectura de la barra lo sigue mostrando con `#`.
- **Imágenes y sus iconos:** Windows muestra, en cada archivo, el icono del programa que lo abre, y Foxy solo puede ofrecerse, no
  imponerse (por eso `.webp` sale con el zorro y `.jpg` con el de otro programa). Ajustes > Ventanas > *Abrir las imágenes con Project Foxy*
  te lleva a la página de Windows donde lo eliges para cada formato.

## [0.3.8] — Rueda en la tira de fotogramas

- **Estudio de GIF:** la rueda del ratón desplaza a los lados la tira de fotogramas (es horizontal, y la rueda no tenía nada que mover).

## [0.3.7] — Lector, collage y estudio de GIF

- **Lector:** se puede soltar un libro (.cbz, .zip, .pdf o una carpeta) sobre el lector para abrirlo, y arriba se ve la página en la que estás
  («Página 20-21 de 186»), con una línea fina en el borde que se llena a medida que lees.
- **Collage y estudio de GIF:** los archivos arrastrados desde cualquier carpeta se aceptan en cualquier punto del estudio, y mientras
  están encima la ventana lo avisa (en el collage se marca la celda que sustituirían). Los movimientos del ratón que arrastran una celda,
  un tirador o una línea de división se suman y se aplican una vez por fotograma, así un ratón rápido ya no redibuja el collage cientos de
  veces por segundo.
- **Menús de los estudios:** cada sección lleva un icono antes del título, las explicaciones largas pasaron a una pequeña «i» con su aviso y
  la flecha de plegar está a la derecha. Los campos de tamaño (GIF, collage, vídeo) ya no tienen flechitas arriba y abajo: arrastra a izquierda o
  derecha para cambiar el valor, o haz clic para escribirlo.
- **Collage, zoom:** cada paso del zoom ya no decodifica la foto entera otra vez a un tamaño nuevo (a una foto grande le costaba un segundo o
  más): la vista previa pide unos pocos tamaños fijos y los conserva, y los pasos de la rueda que llegan entre dos fotogramas se aplican de una vez.
  El archivo guardado no tiene ese límite.
- **Collage, mover una foto con zoom:** el límite de cuánto se podía mover una foto era el mismo con cualquier zoom, así que una foto ampliada se
  quedaba corta antes de sus bordes. Ahora crece con la foto (y nunca es menos de lo que era).

## [0.3.6] — Texto en las imágenes

- **Leer el texto de una imagen** (solo Windows, con el reconocedor del propio Windows; no se descarga nada y la imagen no sale del equipo).
  El botón *Reconocer texto* del dock (o Ctrl+T, o clic derecho) oscurece lo que no es texto y deja cada línea, y las líneas de un
  párrafo juntas, tal cual, con las esquinas del tema. Arrastra para seleccionar, doble clic para una línea, triple clic para un
  párrafo, Ctrl+A para todo, Ctrl+C o clic derecho para copiar. Lee los idiomas que tengas instalados en Windows.
- **Lo mismo en Linux, con Tesseract** (Apache-2.0), en el AppImage y en el Flatpak. El inglés y el español vienen con el programa; Ajustes >
  *Reconocimiento de texto* ofrece 30 más que se descargan cuando los pides (1-4 MB cada uno, a tu carpeta de datos) y se pueden quitar, y
  ahí mismo eliges qué idiomas se leen. El Flatpak recibe acceso a la red solo para esa descarga.
- El icono de *Reconocer texto* ahora son tres líneas dentro de las esquinas.

## [0.3.5] — Quedarse en la bandeja e iniciar con Windows

- **Seguir abierto en la bandeja** (Ajustes > Apariencia > Ventanas, solo en Windows, apagado por defecto). Al cerrar, la ventana solo se
  oculta y deja un icono junto al reloj: haz clic en él, o abre una imagen desde el Explorador, y la ventana vuelve en unos 0,1 s.
  Clic derecho: *Abrir Project Foxy* / *Salir* (un cambio sin guardar sigue preguntando antes). Oculto libera su parte gráfica y
  devuelve la memoria a Windows: unos 3 MB en el Administrador de tareas y nada de memoria de GPU, frente a unos 130 MB y 50 MB con
  una imagen abierta.
- **Iniciar con Windows**, con la opción **Iniciar minimizado en la bandeja**: arranca sin ventana (1-2 MB) y se abre al instante cuando
  lo necesites. Escribe la entrada *Run* del usuario (también la versión portable, si lo activas).

## [0.3.4] — Vídeos que no se abrían

- Corregido: algunos MP4 (por ejemplo, descargados de Twitter) se quedaban en «Abriendo el vídeo…» para siempre o perdían fotogramas al
  pasarlos a GIF. Declaran que cada fotograma dura 1,7 ms, así que ninguna posición caía dentro de uno; ahora un fotograma dura como
  mínimo un periodo de fotograma.

## [0.3.3] — Adornos festivos y vídeo a GIF

- **Adornos discretos en las fechas señaladas**, activados por defecto y solo en los temas modernos: una guirnalda fina
  en el borde superior de la zona de la imagen y una pieza pequeña en la esquina inferior de la barra de miniaturas
  (Halloween añade también una telaraña en la esquina de la imagen). Nada captura el ratón, y las miniaturas nunca
  quedan debajo de la pieza: la barra le deja sitio. Halloween del 1 al 31 de octubre, Día de los Muertos del 1 al 3 de
  noviembre, Navidad del 1 al 26 de diciembre, Año Nuevo del 27 de diciembre al 2 de enero, Noche de Reyes del 3 al 6 de
  enero, San Valentín del 7 al 15 de febrero, San Patricio del 12 al 17 de marzo y Pascua (los 7 días antes del Domingo
  de Resurrección y el lunes siguiente; gana a San Patricio si coinciden).
- Ajustes > Apariencia > *Adornos festivos*: un interruptor general, uno por fiesta, otro para ocultar la pieza de la
  barra de miniaturas (la guirnalda se queda) y una lista para ver cualquier fiesta fuera de fecha (al
  reiniciar el programa vuelve a «Según la fecha»).
- Los dibujos los genera `tools/make_festive_art.js` (SVG sencillo, `resources/festive/`); las fechas son
  `core/Festivity` (probadas en `test_festivity`).

- **Crear una animación desde un vídeo.** En el estudio de animación (Crear > GIF animado) usa el mismo botón *Agregar imágenes o vídeo…* que
  para las imágenes (o suelta el vídeo en el estudio): marca en una línea de tiempo el trozo que quieres, **corta partes en medio** (cortar
  en el cursor y quitar los tramos que no quieras), **recorta la imagen** (1:1, 16:9, 4:3, 3:4, 9:16 o libre: arrastra
  el marco y sus esquinas) y elige los fotogramas por segundo (5-20) y el ancho. Los fotogramas entran en la tira del
  estudio, donde sirven las herramientas de siempre (texto, efectos, tamaño, velocidad, GIF/APNG/WebP). Límites: 600
  fotogramas; la pantalla avisa por encima de 30 s o de unos 25 MB.
- Lee MP4 (H.264/H.265), MOV y MKV (Qt Multimedia; WebM/VP9/AV1 no están en el Qt de Windows). Un clip de 36 s en
  1440p a 60 fps da 358 fotogramas en unos 9 s: el vídeo se reproduce a 4x y solo se guardan los fotogramas necesarios.
- **Un solo Abrir para todo.** El botón Abrir (y Ctrl+O, arrastrar y soltar, «Abrir con» y la línea de órdenes) admite
  imágenes, cómics y vídeos: un vídeo va directo a la pantalla de vídeo del estudio de animación.
- **Estudio de GIF más cómodo y ligero.** La duración de los fotogramas es un solo valor para todos (de 20 a 300 ms, con
  botones para los habituales; antes iba de 20 ms a 5 s y de uno en uno, y las imágenes nuevas duran 100 ms), y el efecto
  y el texto también valen para toda la animación. Los menús de pocas opciones (transiciones, repeticiones, tipo de
  texto, ajustes de un efecto, tamaños) ahora son botones. Una barra de **Compresión** fija los colores del GIF (de 256 a
  16) y el estudio calcula el peso del archivo en segundo plano («Pesará unos 1,2 MB»). La pantalla de vídeo ofrece más
  anchos pequeños (240, 160 y 120 px). Mientras hay un estudio abierto el botón del otro queda apagado, y «Quitar
  todas» es una cruz roja.
- **Arranque más rápido** (la imagen está en pantalla unos 0,65 s después de abrir el programa en vez de 0,9 s con un
  JPEG de 24 megapíxeles; 0,54 s en vez de 0,8 s sin archivo): las fuentes de la interfaz (y sus alternativas para
  escrituras asiáticas y emoji) y el controlador gráfico se cargan en un hilo auxiliar mientras se construye la ventana,
  los diálogos de la barra de título (Ajustes, procesos por lote, Abrir) se crean la primera vez que se usan y el
  reproductor de vídeo solo se crea al abrir un vídeo.
- El programa lleva Qt Multimedia (el instalador crece unos 6 MB).

## [0.3.2] — Actualizaciones desde el programa

- **Un aviso cuando se publica una versión más nueva.** Al arrancar (como mucho una vez al día, cinco segundos después de
  abrir la ventana) el programa pregunta a la API pública de GitHub cuál es la última versión y, si es posterior a la que
  usas, muestra una tarjeta arriba a la derecha: *Actualizar ahora*, *Novedades* y *Omitir esta versión*. Lo único que se
  envía es esa petición HTTPS, con el nombre y la versión del programa en el User-Agent. Viene activado y se puede
  apagar, o lanzar a mano, en Ajustes > Acerca de.
- **Actualización con un clic en Windows.** Cuando el programa se instaló con su instalador, *Actualizar ahora* descarga
  el instalador, lo comprueba con el `SHA256SUMS.txt` de la versión (una descarga que no coincide se borra, y una versión
  sin sumas nunca se ejecuta) y, con *Reiniciar e instalar*, lo ejecuta en silencio y vuelve a abrir el programa. No
  reinicia mientras la imagen abierta tenga cambios sin guardar. El zip portátil, Linux y Flatpak solo reciben el aviso
  (se actualizan por sus propios medios).
- El instalador vuelve a abrir el programa tras una actualización silenciosa (`/update=1`).

## [0.3.1] — Apertura más rápida

- **La imagen anterior y la siguiente se abren al instante.** Mientras miras una imagen, las de antes y después se
  decodifican en segundo plano (hasta 4 imágenes o medio gigabyte), así que las flechas ya no esperan al decodificador:
  unos 20 ms en lugar de 300 ms con fotos de 24 megapíxeles. Si un archivo cambia en el disco, se vuelve a leer.
- **Un primer vistazo reducido de los JPEG grandes, al momento.** Un JPEG de un megabyte o más muestra una versión
  reducida (hasta 2560 píxeles) mientras termina de decodificarse la imagen completa; el documento abierto no cambia
  hasta que llega la completa, y si falla al cargar, la pantalla vuelve a lo que había.
- **La imagen de la línea de órdenes empieza a decodificarse antes**, mientras se construye la ventana, y libvips se
  prepara en segundo plano. Abrir un PNG de 24 megapíxeles desde el Explorador pasó de 660 ms a 340 ms.
- Las imágenes grandes se reducen para mostrarlas fuera del hilo de la interfaz, y el búfer de libvips se usa tal cual
  (sin copiar decenas de megabytes).
- **Se acabó el destello blanco al abrir la ventana** (Windows pinta de blanco una ventana nueva hasta que su primer
  fotograma está listo): la ventana permanece oculta hasta que ese fotograma existe y su fondo es el color del tema.
- **Instalador:** sin página de licencia, con el logo de Project Foxy en el asistente y una imagen propia en la página
  de «instalación completada» (la genera `tools/make_installer_images.py`).
- La herramienta de desarrollo `bench_load` mide cada fase de la apertura de una imagen.

## [0.3.0] — Lector de cómics y manga

- **Lector de cómics y manga.** Abre un archivo `.cbz`, `.zip` o `.pdf` como cualquier imagen, o pulsa el nuevo botón del libro
  en la barra de título para leer la carpeta de la imagen que tienes abierta. Las páginas se leen de derecha a izquierda
  por defecto (como el manga) o de izquierda a derecha, de una en una o de dos en dos, con la portada sola y las
  páginas apaisadas (que ya son una doble) a solas. Izquierda y derecha (o las zonas de clic de los lados) pasan la
  página, la rueda desplaza lo que no cabe y luego la pasa, y el dock de la barra inferior trae el deslizador de páginas y los
  interruptores de dirección, página doble, portada y ajuste (pantalla, alto o ancho). El libro recuerda la página en
  que te quedaste. Teclas: **D** página doble, **R** dirección, **C** portada sola, **1 2 3** ajuste, **Inicio/Fin**
  primera y última página, **Esc** lo cierra. Las páginas siguientes y la anterior se descomprimen y decodifican por
  adelantado, así que pasar de página no espera. Primera versión: CBZ, PDF y carpetas (RAR y 7z después).
- **Nitidez FSR CAS.** El enfoque ahora es el CAS de AMD FidelityFX (el paso que usa FSR 1): se adapta al contraste
  local, así que aporta nitidez donde hay margen y deja en paz las zonas planas y el ruido, con muchos menos halos que
  la máscara de enfoque de antes. Sustituye al control *Nitidez* de Ajustes (misma vista previa y mismo archivo
  guardado) y al enfoque tras ampliar una imagen en *Tamaño*. Botón nuevo abajo a la derecha de la ventana (un
  triángulo pequeño, encendido mientras está activa): un menú con una barra de 0 a 100 % que afila lo que ves en pantalla
  mientras lees un cómic o manga (el botón solo aparece entonces). 0 % es apagado; nunca cambia el archivo. Se recuerda entre sesiones.
- Windows: arreglado un cierre inesperado al hacer clic en el botón de maximizar (0.2.8). El programa escribía en un
  puntero nulo que Windows entrega en ese clic.
- La ventana recuerda su tamaño, su posición y si estaba maximizada, y se abre igual la próxima vez. Si el tamaño
  guardado es mayor que la pantalla conectada ahora (por ejemplo, se guardó en un monitor de 1440p y la abres en uno de
  1080p), se reduce para que quepa y nunca queda fuera de la pantalla.
- Los nombres de archivo largos (sin espacios) ahora se parten dentro de los diálogos de «cambios sin guardar»,
  «reemplazar archivo» y eliminar, en lugar de salirse del cuadro.

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
