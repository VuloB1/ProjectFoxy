# Fuzzing

Los lectores de formatos abren archivos que llegan de internet o de otras personas: es la parte del programa más
expuesta. Aquí se alimenta a `core::AnimatedDecoder` (GIF, APNG y WebP animado; el APNG tiene un lector de trozos
escrito a mano) con miles de archivos corruptos por segundo, con **AddressSanitizer** (accesos fuera de rango, uso tras
liberar) y **UndefinedBehaviorSanitizer** (desbordes, desplazamientos inválidos) activados.

Un proyecto CMake aparte (`CMakeLists.txt`, no entra en la compilación normal): necesita Clang y solo compila lo que
fuzzea (`AnimatedDecoder.cpp` + Qt + libwebp), así que compila en un minuto y no necesita vcpkg.

```bash
cmake -S tests/fuzz -B build/fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH=<ruta de Qt>
cmake --build build/fuzz

mkdir -p /tmp/work/webp && cp tests/fuzz/corpus/webp/* /tmp/work/webp/
build/fuzz/fuzz_animated_webp -max_total_time=300 -rss_limit_mb=2048 -timeout=10 -max_len=65536 /tmp/work/webp
```

Tres ejecutables, uno por formato: `fuzz_animated_gif`, `fuzz_animated_apng` y `fuzz_animated_webp`. `corpus/<formato>/`
trae las semillas (archivos válidos pequeños y los casos que ya encontraron algo). Los límites de memoria del decodificador
se bajan en el arnés para que un archivo hostil cueste poco probarlo.

- **En CI** corre un minuto por formato en cada PR (job `fuzz`): si encuentra una caída, falla y sube el archivo que la
  provoca.
- **Qt no está instrumentado** (solo nuestro código): un fallo *dentro* de Qt aparece como caída, no como informe del
  sanitizer.
- **Clang 14 + kernels recientes** (p. ej. WSL2): ASan puede caerse al arrancar por la aleatorización de direcciones; se
  evita con `setarch x86_64 -R <ejecutable> ...`.
- **Un hallazgo** se convierte en prueba de regresión: el archivo va a `tests/fixtures/fuzz/` con un caso en
  `test_animated_decoder.cpp`, y a `corpus/` para que el fuzzer lo siga ejerciendo.

## Hallazgos

| Fecha | Formato | Qué pasaba | Arreglo |
|---|---|---|---|
| 12‑oct‑2026 | WebP animado | Un archivo de **188 bytes** con un lienzo declarado de 48 × 16 252 703 (780 MP) hacía que `WebPAnimDecoderNew` reservara **~3 GB** *antes* de que el programa comprobara el tamaño | El tamaño se comprueba con `WebPGetFeatures` (solo la cabecera) **antes** de crear el decodificador |
