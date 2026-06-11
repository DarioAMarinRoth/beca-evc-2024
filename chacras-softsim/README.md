# chacras-softsim

Simulador por software del esclavo Modbus RTU de la blue pill del proyecto Chacras.
Permite usar el binario `master` con exactamente los mismos comandos que con la placa física,
sin necesidad de hardware.

---

## Estructura final del directorio

```
chacras-softsim/
├── chacras.c           # Código original (master)
├── chacras_slave.c     # Esclavo simulador (este repo)
├── start_sim.sh        # Script que levanta todo
├── master              # Binario del cliente (compilado)
├── chacras_slave       # Binario del esclavo (compilado)
└── chacras_state.json  # Estado persistido (se genera solo)
```

---

## 1. Dependencias

```bash
sudo apt install libmodbus-dev libcjson-dev socat
```

---

## 2. Archivos necesarios

Copiá los siguientes archivos dentro de `chacras-softsim/`:

- `chacras.c` — el cliente original
- `chacras_slave.c` — el esclavo simulador
- `start_sim.sh` — el script de arranque

---

## 3. Compilar

```bash
gcc -o master chacras.c -lmodbus
gcc -o chacras_slave chacras_slave.c -lmodbus -lcjson -lm
chmod +x start_sim.sh
```

---

## 4. Correr la simulación

### Terminal 1 — levantar el simulador

```bash
./start_sim.sh
```

Vas a ver algo así:

```
[socat] Creando par virtual: /dev/ttyV0 <-> /dev/ttyV1
[socat] PID: 12345
[socat] Puertos listos: /dev/ttyV0  /dev/ttyV1
[slave] Arrancando ./chacras_slave en /dev/ttyV1 @ 115200
[slave] PID: 12346
[persist] No existe chacras_state.json, arrancando con valores por defecto

=== Simulador listo ===
  Usá chacras con:  --port /dev/ttyV0
  Ejemplo:  ./master --port /dev/ttyV0 --status
```

Dejá esta terminal abierta mientras usás el simulador.

### Terminal 2 — usar el cliente

```bash
# Ver estado de batería y armado
./master -p /dev/ttyV0 -s

# Ver estado de un motor (0 a 3)
./master -p /dev/ttyV0 -m 0

# Establecer setpoint y sentido de un motor
#   ./master -p /dev/ttyV0 -m <motor> <setpoint> <sentido>
./master -p /dev/ttyV0 -m 0 100 1

# Armar el sistema
./master -p /dev/ttyV0 -a 1

# Desarmar
./master -p /dev/ttyV0 -a 0
```

---

## 5. Persistencia

Cada vez que se escribe un registro (setpoint, sentido, armado), el esclavo guarda
el estado en `chacras_state.json` en el mismo directorio.

Al volver a correr `./start_sim.sh`, el esclavo carga ese archivo y retoma
desde donde estaba.

Para resetear el estado a cero simplemente borrá el archivo:

```bash
rm chacras_state.json
```

---

## 6. Apagar el simulador

`Ctrl+C` en la Terminal 1. El script mata el esclavo y socat limpiamente,
y hace un guardado final del estado antes de salir.

---

## Notas

- Los puertos virtuales son `/dev/ttyV0` (cliente) y `/dev/ttyV1` (esclavo).
  socat los crea como symlinks al arrancar y desaparecen al cerrar.
- La corriente por motor se simula como `setpoint × 0.05 A/RPS`.
  La corriente de batería es la suma de los 4 motores.
  La tensión de batería es fija en 24 V nominal.
- Los motores numerados van de **0 a 3**.
