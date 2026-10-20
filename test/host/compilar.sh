#!/usr/bin/env bash
# Compila y ejecuta en el PC (no en la Pico) el código REAL de la interfaz y del eje,
# contra una pantalla, un motor y unos finales de carrera simulados.
#
# Uso:  test/host/compilar.sh [carpeta_bin_de_w64devkit]      (por defecto /d/dev/w64devkit/bin)
# Salida: test/host/build/ui/*.ppm + montajes PNG, y el informe de la simulación del eje.
set -euo pipefail

export PATH="${1:-/d/dev/w64devkit/bin}:$PATH"
RAIZ="$(cd "$(dirname "$0")/../.." && pwd)"
H="$RAIZ/test/host"
OUT="$H/build"
mkdir -p "$OUT/ui"
cd "$RAIZ"

CFLAGS="-std=c11 -Wall -Wextra -g -O1 -I$H/mock -Isrc"

echo "== Interfaz (pantalla simulada) =="
gcc $CFLAGS -o "$OUT/harness_ui.exe" "$H/harness_ui.c" "$H/mock_st7789.c" \
    src/ui/interfaz.c src/graficos/texto.c src/graficos/fuente5x7.c \
    src/dominio/color.c src/dominio/escala.c src/dominio/historial.c src/dominio/selector.c
"$OUT/harness_ui.exe" "$OUT/ui"
python "$H/montaje.py" "$OUT/ui" || echo "(sin Python/Pillow: quedan los .ppm sin montar)"

echo
echo "== Eje (motor y finales simulados) =="
# RUIDO = variación relativa de la velocidad en cada viaje (0.03 = ±3 %)
gcc $CFLAGS -DRUIDO="${RUIDO:-0.03}" -o "$OUT/harness_eje.exe" "$H/harness_eje.c" \
    "$H/mock_motor.c" "$H/mock_finales.c" \
    src/aplicacion/carro.c src/aplicacion/eje.c src/dominio/posicion.c src/dominio/limites.c -lm
"$OUT/harness_eje.exe"

echo
echo "== Autoreferenciar al iniciar =="
gcc $CFLAGS -Wno-unused-function -DSOLO_AUTOREF -o "$OUT/test_autoref.exe" "$H/test_sistema.c" "$H/mock_motor.c" "$H/mock_finales.c"     "$H/mock_adc.c" "$H/mock_tcs3200.c"     src/aplicacion/sistema.c src/aplicacion/carro.c src/aplicacion/eje.c src/aplicacion/deteccion.c     src/dominio/posicion.c src/dominio/limites.c src/dominio/escala.c src/dominio/selector.c src/dominio/color.c     src/dominio/historial.c src/dominio/proximidad.c src/drivers/proximidad.c -lm
"$OUT/test_autoref.exe"

echo
echo "== Protocolo serie =="
gcc $CFLAGS -Wno-unused-function -DSOLO_PROTOCOLO -DSISTEMA_AUTOREFERENCIAR=0 -o "$OUT/test_protocolo.exe" "$H/test_sistema.c" "$H/mock_motor.c" "$H/mock_finales.c"     "$H/mock_adc.c" "$H/mock_tcs3200.c"     src/aplicacion/sistema.c src/aplicacion/carro.c src/aplicacion/eje.c src/aplicacion/deteccion.c src/aplicacion/protocolo.c     src/dominio/posicion.c src/dominio/limites.c src/dominio/escala.c src/dominio/selector.c src/dominio/color.c     src/dominio/historial.c src/dominio/proximidad.c src/drivers/proximidad.c -lm
"$OUT/test_protocolo.exe"

echo
echo "== Máquina de estados (sistema + eje + IR de proximidad + sensor de color simulados) =="
gcc $CFLAGS -DSISTEMA_AUTOREFERENCIAR=0 -o "$OUT/test_sistema.exe" "$H/test_sistema.c" "$H/mock_motor.c" "$H/mock_finales.c"     "$H/mock_adc.c" "$H/mock_tcs3200.c"     src/aplicacion/sistema.c src/aplicacion/carro.c src/aplicacion/eje.c src/aplicacion/deteccion.c     src/dominio/posicion.c src/dominio/limites.c src/dominio/escala.c src/dominio/selector.c src/dominio/color.c     src/dominio/historial.c src/dominio/proximidad.c src/drivers/proximidad.c -lm
"$OUT/test_sistema.exe"

