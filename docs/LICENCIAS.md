# Project Foxy — licencias de las dependencias

> Estado a **2 de octubre de 2026**. Esto es un análisis técnico hecho a partir de lo
> que vcpkg declara para cada paquete y de lo que realmente se instala y se distribuye
> junto al `.exe`. **No es asesoría legal**: antes de distribuir el programa a terceros
> conviene una revisión jurídica.
>
> Los textos completos de cada licencia están en `THIRD_PARTY_NOTICES.txt` (se genera con
> `tools/make_third_party_notices.ps1`).

## 1. Resumen

| Tema | Estado |
|---|---|
| Licencia del propio proyecto | **No elegida todavía.** No hay archivo `LICENSE`. |
| **exiv2** (GPL‑2.0‑or‑later) | **Bloquea distribuir Project Foxy como software cerrado** mientras se use. Opciones en la sección 3. |
| **x265** (GPL‑2.0‑or‑later) | **Resuelto el 2‑oct‑2026**: se distribuía `libx265.dll` sin que hiciera falta (ver sección 4). |
| Qt, libvips, libheif, libde265, LibRaw y el resto | LGPL / MIT / BSD y similares: compatibles con una aplicación cerrada **si se cumplen sus condiciones** (enlace dinámico, avisos, texto de la licencia, derecho a reemplazar la biblioteca). Todas se enlazan como DLL. |
| Patentes HEVC/H.265 | Fuera del alcance de GPL/LGPL; ver sección 5. |

## 2. Componentes que viajan con el programa

Versiones y licencias tal como las declara cada *port* de vcpkg (identificadores SPDX).
Todos se distribuyen como DLL junto a `ProjectFoxy.exe`.

| Componente | Versión | Licencia declarada | Para qué se usa |
|---|---|---|---|
| libvips | 8.18.5 | LGPL‑2.1‑or‑later | JPEG, PNG, WebP, TIFF (decodificar y guardar) |
| LibRaw (`raw_r`) | 0.22.2 | (LGPL‑2.1‑only **OR** CDDL‑1.0) AND BSD‑3‑Clause | RAW de cámara |
| libheif | 1.23.5 | LGPL‑3.0‑only AND MIT | HEIC / HEIF / AVIF |
| libde265 | 1.1.3 | LGPL‑3.0‑only | decodificador HEVC de libheif |
| libaom (`aom`) | 3.15.1 | BSD‑2‑Clause AND BSD‑3‑Clause AND MIT AND ISC | AV1 (AVIF) |
| **exiv2** | 0.28.8 | **GPL‑2.0‑or‑later** | leer metadatos EXIF/IPTC/XMP y copiarlos al guardar (con las funciones `png` → zlib y `xmp` → expat, ambas permisivas, ya presentes en la lista) |
| GLib / GObject / GIO | 2.90.0 | LGPL‑2.1‑or‑later AND (LGPL‑2.1‑only OR MPL‑1.1) | base de libvips |
| gettext‑libintl, libiconv, libffi, PCRE2 | — | LGPL‑2.1+, MIT, BSD‑3 | dependencias de GLib |
| Expat | 2.8.5 | MIT AND CC0‑1.0 | XML (Exiv2, libvips) |
| Little CMS | 2.19.1 | MIT | gestión de color |
| libjpeg‑turbo | 3.2.0 | BSD‑3‑Clause AND IJG | JPEG |
| libpng · libwebp · libtiff · zlib · liblzma | — | libpng‑2.0 · BSD‑3 · libtiff · Zlib · 0BSD | formatos y compresión |
| Base de datos Lensfun (solo los datos) | marca de tiempo 1577948414 | **CC BY‑SA 3.0** | perfiles de cámaras y objetivos de la herramienta *Lente* (sección 7b) |
| **Qt 6.7.3** | 6.7.3 | LGPL‑3.0 / GPL‑3.0 / comercial | interfaz (Core, Gui, Qml, Quick, QuickControls2, QuickDialogs2, Svg, QuickEffects) |

Solo de **compilación** (no se distribuyen): gettext (herramientas, GPL‑3.0‑only),
`qsb` de Qt Shader Tools (el *runtime* es LGPL; la herramienta tiene licencia GPL, y usar
una herramienta GPL para generar un archivo no convierte ese archivo en GPL), pkgconf,
meson y los scripts `vcpkg‑cmake*`.

## 3. El problema principal: exiv2 (GPL)

exiv2 se enlaza dentro de Project Foxy (`MetadataReader.cpp` lo usa para la ventana de
información y `ImageWriter.cpp` para conservar EXIF/IPTC/XMP/ICC al guardar). Una
aplicación que enlaza una biblioteca GPL‑2.0‑or‑later debe distribuirse bajo licencia
compatible con la GPL, con su código fuente. Opciones:

1. **Publicar Project Foxy bajo una licencia compatible con la GPL** (por ejemplo GPL‑3.0),
   con el código fuente disponible. Es lo más simple si el objetivo no es venderlo cerrado.
2. **Sustituir exiv2** por algo con licencia permisiva o LGPL. El uso está aislado en dos
   archivos y detrás de funciones pequeñas (`MetadataReader::read`, `withMetadata` en
   `ImageWriter.cpp`), así que el cambio es acotado. Alternativas: `libexif` (LGPL‑2.1,
   además permitiría activar el soporte EXIF de libvips), un lector/escritor mínimo propio
   del bloque EXIF de JPEG, o los lectores de metadatos de Qt.

   *Vía concreta (comprobada en el manifiesto, no probada en código):* en el
   `builtin-baseline` fijado, el *port* `libvips` 8.18.5 tiene la característica `exif`, que
   añade `libexif` 0.6.26 (**LGPL‑2.1‑or‑later**). Con ella libvips leería y reescribiría
   EXIF por sí mismo (y arreglaría de paso la orientación de JPEG, que hoy se resuelve con
   Qt) y bastaría para quitar `exiv2`. **Aviso (cuarta ronda): esta vía cubre solo EXIF.**
   `libexif` no sabe de XMP ni de IPTC, y desde la cuarta ronda hay pruebas que exigen
   conservar **EXIF + XMP + IPTC en JPEG, EXIF + XMP en PNG y las conversiones entre JPEG,
   PNG y WebP** (`tests/test_image_writer.cpp`, fixtures `meta_rich.jpg/png`). Sustituir
   exiv2 sin perder eso exige además un lector/escritor de XMP (p. ej. Exempi, BSD) y de
   IPTC, o el paso opaco de libvips verificado formato a formato. Esfuerzo estimado: **L**
   (antes se estimó M, cuando esas pruebas no existían: el README prometía algo que el
   build ni siquiera cumplía).
3. Una licencia comercial de exiv2: **hoy el proyecto no la ofrece** (según su propio
   repositorio; no lo he verificado de nuevo).

Si Project Foxy se queda como uso personal, no se distribuye, nada de esto aplica.

## 4. x265 — corregido

Hasta el 2‑oct‑2026 el `vcpkg.json` pedía `libheif` con `features: ["aom"]` pero **sin
desactivar las características por defecto**, y el *port* de libheif trae `hevc` por
defecto, que añade **x265** (codificador HEVC, **GPL‑2.0‑or‑later**). Comprobado en el
árbol instalado: `x265` estaba instalado y `libx265.dll` (5 MB) se copiaba junto al `.exe`,
aunque Project Foxy solo **decodifica** HEIC y nunca lo necesitó.

Corrección: `"default-features": false` en `libheif`. Tras recompilar: x265 desaparece del
árbol instalado y de la carpeta del programa, `libde265` (LGPL‑3.0) sigue haciendo la
decodificación HEVC y las pruebas automáticas siguen abriendo un HEIC (HEVC) y un AVIF reales
(`tests/fixtures/format_sample.heic`, `.avif`).

## 5. Patentes de códec (HEVC / H.265)

Poder usar el código de libde265/libheif no implica una licencia de las patentes de HEVC en
todos los países. Para un producto distribuido comercialmente conviene una consulta
específica. AVIF (AV1) usa un códec con licencia de patentes libre de regalías (Alliance for
Open Media), pero también debe revisarse.

## 6. Qt

- Se usa **enlazado dinámicamente** (las `Qt6*.dll` van junto al programa), que es lo que
  permite la LGPL‑3.0 en una aplicación cerrada.
- Hay que acompañar la distribución con el texto de la LGPL‑3.0, los avisos de Qt y la
  forma de obtener el código fuente de Qt, y no impedir que el usuario sustituya las
  bibliotecas.
- Ninguno de los módulos usados es de los que Qt ofrece solo bajo GPL/comercial (por
  ejemplo Qt Charts o Data Visualization); **confirmarlo en la página de licencias de Qt**
  antes de distribuir, porque lo he deducido, no lo he leído de ahí.

## 7. LibRaw

Ofrece elegir entre LGPL‑2.1 y CDDL‑1.0. Para este proyecto lo natural es **elegir
explícitamente LGPL‑2.1** (y dejarlo anotado en los avisos), además de la parte BSD‑3.

## 7b. Base de datos de lentes (Lensfun)

La herramienta *Lente* trae embebida la base de perfiles del proyecto **Lensfun**
(`resources/lensfun/*.xml`, unos 5 MB, 56 archivos). **Solo los datos**: la biblioteca
Lensfun (LGPL) no se usa; el programa lee los XML con código propio
(`src/core/lens/LensDatabase.cpp`) y aplica las fórmulas publicadas de distorsión,
aberración cromática y viñeteo.

- Licencia de los datos: **Creative Commons Atribución‑CompartirIgual 3.0** (CC BY‑SA 3.0).
  Hay que dar crédito al proyecto Lensfun y a sus colaboradores, indicar la licencia y
  enlazarla. Los archivos van **sin modificar**; si alguien los redistribuye cambiados,
  los cambios quedan también bajo CC BY‑SA 3.0.
- El «compartir igual» alcanza a los archivos de datos (y a las obras derivadas *de esos
  datos*), no al resto del programa: el código de Project Foxy los lee como un recurso,
  no los incorpora a sí mismo. Esto es una lectura técnica mía, **no asesoría legal**; si
  se publica el proyecto como GPL‑3.0‑or‑later, conviene dejar los XML en su carpeta con
  su aviso y confirmarlo en la revisión jurídica de la sección 8.
- El aviso ya está en `THIRD_PARTY_NOTICES.txt` (y en `tools/make_third_party_notices.ps1`,
  para que sobreviva a las regeneraciones).
- Para actualizar la base: copiar de nuevo los XML de `data/db` del repositorio de Lensfun
  y el `timestamp.txt`, y volver a compilar.

## 8. Qué hacer antes de distribuir

1. Decidir la licencia del proyecto (sección 3). Añadir `LICENSE`.
2. Resolver exiv2 (publicar con licencia compatible o sustituirlo).
3. Incluir `THIRD_PARTY_NOTICES.txt` y los textos de LGPL/GPL en el instalador, y un
   «Acerca de» que los muestre.
4. Mantener las bibliotecas LGPL como DLL (no enlazarlas estáticamente) y documentar cómo
   obtener su código fuente. Hoy es así por construcción: el triplet de vcpkg es
   `x64-windows` (dinámico) y está **fijado** en `CMakePresets.json`; `imageviewer_core` es
   una biblioteca estática propia, pero libvips, LibRaw, libheif y exiv2 son DLL junto al
   `.exe`. Un triplet estático obligaría a rehacer este análisis.
5. Regenerar `THIRD_PARTY_NOTICES.txt` en cada versión (si cambia el `builtin-baseline` de
   vcpkg o se añade una dependencia, cambian las versiones y puede cambiar una característica
   opcional: lo que pasó con x265).
6. Consulta jurídica sobre patentes de códec si se distribuye comercialmente.
