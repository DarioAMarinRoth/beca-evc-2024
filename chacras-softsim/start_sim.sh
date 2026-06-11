#!/bin/bash
# start_sim.sh — levanta socat + esclavo simulado

SLAVE_PORT=/dev/ttyV1
MASTER_PORT=/dev/ttyV0
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
sudo socat -d -d \
    pty,raw,echo=0,link=$MASTER_PORT \
    pty,raw,echo=0,link=$SLAVE_PORT \
    &
SOCAT_PID=$!
echo "[socat] PID: $SOCAT_PID"

# esperar a que socat cree los symlinks
for i in $(seq 1 20); do
    [ -L $MASTER_PORT ] && [ -L $SLAVE_PORT ] && break
    sleep 0.1
done
sudo chmod g+rw $(readlink -f $MASTER_PORT) $(readlink -f $SLAVE_PORT)
sudo chmod g+rw root:dialout $(readlink -f $MASTER_PORT) $(readlink -f $SLAVE_PORT)
if [ ! -L $MASTER_PORT ] || [ ! -L $SLAVE_PORT ]; then
    echo "[error] socat no creó los puertos virtuales"
    kill $SOCAT_PID 2>/dev/null
    exit 1
fi

echo "[socat] Puertos listos: $MASTER_PORT  $SLAVE_PORT"

# ---------- lanzar esclavo ----------
echo "[slave] Arrancando $SLAVE_BIN en $SLAVE_PORT @ $BAUD"
$SLAVE_BIN $SLAVE_PORT $BAUD &
SLAVE_PID=$!
echo "[slave] PID: $SLAVE_PID"

echo ""
echo "=== Simulador listo ==="
echo "  Usá chacras con:  --port $MASTER_PORT"
echo "  Ejemplo:  ./chacras --port $MASTER_PORT --status"
echo ""
echo "  Ctrl+C para terminar"

# ---------- esperar y limpiar al salir ----------
trap "echo; echo '[cleanup] Terminando...'; kill $SLAVE_PID $SOCAT_PID 2>/dev/null; exit 0" INT TERM

wait $SLAVE_PID
kill $SOCAT_PID 2>/dev/null
