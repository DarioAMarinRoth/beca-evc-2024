#!/bin/bash
# start_sim.sh — levanta socat + esclavo simulado sin sudo

# 1. Cambiar rutas a /tmp/ (espacio con permisos de escritura para el usuario)
SLAVE_PORT=/tmp/ttyV1
MASTER_PORT=/tmp/ttyV0
BAUD=115200
SLAVE_BIN=./chacras_slave

# ---------- verificar dependencias ----------
for cmd in socat $SLAVE_BIN; do
    if ! command -v $cmd &>/dev/null && [ ! -x "$cmd" ]; then
        echo "[error] No se encuentra: $cmd"
        exit 1
    fi
done

# ---------- limpiar symlinks viejos ----------
[ -L $SLAVE_PORT ] && rm -f $SLAVE_PORT
[ -L $MASTER_PORT ] && rm -f $MASTER_PORT

# ---------- crear par serial virtual ----------
echo "[socat] Creando par virtual: $MASTER_PORT <-> $SLAVE_PORT"

# 2. Ejecutar socat sin sudo
socat -d -d \
    pty,raw,echo=0,link=$MASTER_PORT \
    pty,raw,echo=0,link=$SLAVE_PORT \
    &
SOCAT_PID=$!
echo "[socat] PID: $SOCAT_PID"

# ---------- esperar a que socat cree los symlinks ----------
for i in $(seq 1 20); do
    [ -L $MASTER_PORT ] && [ -L $SLAVE_PORT ] && break
    sleep 0.1
done

if [ ! -L $MASTER_PORT ] || [ ! -L $SLAVE_PORT ]; then
    echo "[error] socat no creó los puertos virtuales"
    kill $SOCAT_PID 2>/dev/null
    exit 1
fi

# ---------- resolver rutas reales ----------
MASTER_REAL=$(readlink -f $MASTER_PORT)
SLAVE_REAL=$(readlink -f $SLAVE_PORT)

# 3. La sección de chmod ha sido eliminada. El usuario ya es propietario de los PTYs.
echo "[socat] Puertos listos: $MASTER_PORT -> $MASTER_REAL  |  $SLAVE_PORT -> $SLAVE_REAL"

# ---------- lanzar esclavo ----------
echo "[slave] Arrancando $SLAVE_BIN en $SLAVE_PORT @ $BAUD"
$SLAVE_BIN $SLAVE_PORT $BAUD &
SLAVE_PID=$!

echo ""
echo "=== Simulador listo ==="
echo "  Usá chacras con:  --port $MASTER_PORT"
echo "  Ejemplo:  ./master -s"
echo ""
echo "  Ctrl+C para terminar"

# ---------- esperar y limpiar al salir ----------
trap "echo; echo '[cleanup] Terminando...'; kill $SLAVE_PID $SOCAT_PID 2>/dev/null; exit 0" INT TERM

wait $SLAVE_PID
kill $SOCAT_PID 2>/dev/null