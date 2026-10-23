# contar_instrucciones.gdb — Cuenta las instrucciones que ejecuta una función (Fase 3, para el CPI).
#
# Uso (con la sonda conectada y el firmware de la prueba de ciclos cargado, PRUEBA_ACTIVA = PRUEBA_CICLOS):
#   arm-none-eabi-gdb build/detector_color.elf -x tools/contar_instrucciones.gdb
#   (gdb) target extended-remote localhost:3333      # OpenOCD en marcha
#   (gdb) monitor reset halt
#   (gdb) contar bench_una_a
#   (gdb) contar bench_una_b
#   (gdb) contar bench_una_c
#
# Para en la entrada de la función, recorre instrucción por instrucción (stepi) hasta que vuelve a su
# llamador, incluidas las funciones a las que llama, y muestra el total. Se enmascaran las
# interrupciones para que no se cuele la ISR del USB. Cada paso es lento (unos cientos por segundo).

define contar
  tbreak $arg0
  continue
  set $primask = 1
  set $n = 0
  set $retorno = $lr & ~1
  while (($pc & ~1) != $retorno)
    stepi
    set $n = $n + 1
  end
  set $primask = 0
  printf "%s: %d instrucciones\n", "$arg0", $n
end
document contar
Cuenta las instrucciones ejecutadas por la funcion indicada (hasta que retorna).
end
