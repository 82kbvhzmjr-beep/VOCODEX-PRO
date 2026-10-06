# VOCODEX PRO 2.4 (macOS · VST3 + AU)

Plugin de voz hecho con JUCE 8. Se instala **junto a Voxora** sin sustituirlo
(nombre `Vocodex Pro`, bundle id `com.vocodex.pro`, códigos `Vcdx/Vcpr`).

## Compilar en GitHub (sin instalar nada)
1. Sube **todo el contenido de esta carpeta** a la raíz de tu repo (borra antes el código viejo).
2. Pestaña **Actions** → "Build Vocodex Pro (macOS)" → se lanza solo con cada push (o *Run workflow*).
3. Cuando termine en verde, abajo del todo en **Artifacts** descarga `VocodexPro-Mac`.
4. Descomprime, doble clic en `Instalar.command` (si macOS lo bloquea: clic derecho → Abrir).
5. FL Studio → Options → Manage plugins → *Find installed plugins*.

Si el workflow falla, abre el paso en rojo y copia las últimas ~30 líneas del log.

## Compilar en tu Mac
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel 4

## Estructura
- `Source/Dsp.h` – todo el procesamiento (pitch, EQ, comp, de-esser, saturación, air, mic, reverb, delay). C++ puro.
- `Source/PluginProcessor.*` – parámetros, presets y categorías.
- `Source/PluginEditor.*` – interfaz (negro / blanco / números dorados).
- `test/test_dsp.cpp` – prueba del motor sin JUCE: `g++ -std=c++17 -O2 test/test_dsp.cpp -o t && ./t`

## Notas
- Latencia fija de ~15 ms (la reporta al host; FL Studio la compensa).
- Los micrófonos son perfiles de carácter (EQ + saturación), no modelado 1:1.
- La afinación usa detección YIN + cambio de tono por granos: va bien para correcciones
  pequeñas, no es el motor de Pitcher.
