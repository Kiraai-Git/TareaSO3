# memsim — Simulador de Políticas de Asignación de Memoria

Tarea 3 · Sistemas Operativos

## Descripción

`memsim` simulador de politicas la cual no utiliza memoria real, utiliza bloques (`Block`) que representan segmentos contiguos del espacio de direcciones simulado, clasificados como libres u ocupados.

## Compilación

Abrir una consola en la carpeta donde se encuentran los archivos y ejecutar:

```bash
gcc -Wall -Wextra -std=c17 -o memsim memsim.c
```
## Uso

```bash
./memsim <memoria_bytes> <POLITICA> <archivo_traza>
```

| Parámetro        | Descripción                                              |
|------------------|----------------------------------------------------------|
| `memoria_bytes`  | Tamaño total de la memoria simulada (entero positivo)    |
| `POLITICA`       | `FIRST_FIT`, `BEST_FIT` o `WORST_FIT`                   |
| `archivo_traza`  | Ruta al archivo con la secuencia de operaciones          |

### Formato del archivo de traza

```
ALLOC <PID> <BYTES>   # asignar memoria al proceso PID
FREE <PID>            # liberar todos los bloques del proceso PID
COMPACT               # compactar la memoria (mueve ocupados al inicio)
```

## Ejemplo de ejecución

```bash
./memsim 1024 FIRST_FIT traza1.txt
```

Salida esperada:

```
Procesos asignados: 6
Memoria utilizada: 68.36%
Indice de Fragmentacion Externa: 0.0000
Estado final de la memoria:
[Ocupado P4 150] -> [Ocupado P5 50] -> [Ocupado P3 100] -> [Ocupado P6 400] -> [Libre 324]
```

## Archivos de prueba incluidos

| Archivo                   | Descripción                                                      |
|---------------------------|------------------------------------------------------------------|
| `traza1.txt`              | Caso general: ALLOCs, FREEs, COMPACT y asignación final          |
| `traza_fragmentacion.txt` | Produce fragmentación externa y ALLOC falla sin COMPACT          |
| `traza_coalescencia.txt`  | Verifica que liberar bloques adyacentes los fusiona correctamente |
| `traza_politicas.txt`     | Misma secuencia con las 3 políticas para comparar resultados      |

### Demostrar fragmentación

```bash
./memsim 500 FIRST_FIT traza_fragmentacion.txt
```

Liberar P1 y P3 deja dos huecos de 100 bytes cada uno (200 bytes libres totales), pero P5 necesita 200 bytes **contiguos** → ALLOC falla con `Fext ≈ 0.6667`.

### Demostrar diferencia entre políticas

```bash
./memsim 1024 FIRST_FIT  traza_politicas.txt
./memsim 1024 BEST_FIT   traza_politicas.txt
./memsim 1024 WORST_FIT  traza_politicas.txt
```

`WORST_FIT` ubica los procesos en el bloque más grande disponible, generando una distribución distinta visible en el estado final.

## Estructura de datos

La memoria se representa como una **lista doblemente enlazada** de nodos `Block`:

```
[Libre 100] <-> [Ocupado P1 200] <-> [Libre 724]
```

Cada nodo almacena: tipo (`BLOCK_FREE` / `BLOCK_OCCUPIED`), PID, tamaño y dirección de inicio.

### Coalescencia (FREE)

Al liberar un bloque, se verifica si los vecinos inmediatos también son libres; de ser así, se fusionan en un único bloque contiguo.

### Compactación (COMPACT)

Se recorre la lista, se retiran todos los bloques libres (acumulando el total libre) y se reordenan los ocupados desde la dirección 0. Al final se inserta un único bloque libre con el espacio restante.

### Índice de Fragmentación Externa

```
Fext = 1 − (Blockmax / Mlibre)
```

donde `Bmax` es el bloque libre más grande y `Mlibre` es la suma de todos los bytes libres. Un valor de `0.0` indica que todo el espacio libre es contiguo.
