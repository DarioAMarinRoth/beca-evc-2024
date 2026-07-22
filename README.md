# Integración y puesta a punto de algoritmos de navegación en robots móviles 4WD y 2WD

Este repositorio contiene el trabajo desarrollado en el marco de la Beca de Estímulo a las Vocaciones Científicas (EVC) 2024, otorgada por el Consejo Interuniversitario Nacional (CIN). Contiene el código fuente desarrollado para la integración de una plataforma robótica 4WD con el middleware ROS 2.

## Información general

- **Becario**: Dario Alejandro Marin Roth
- **Universidad**: Universidad Nacional del Comahue (UNCo)
- **Facultad**: Facultad de Ingeniería
- **Carrera**: Ingeniería Electrónica

### Dirección

#### Director

- **Nombre**: Marcelo Leandro Moreyra  
- **Correo electrónico**: [marcelo.moreyra@fiain.uncoma.edu.ar](mailto:marcelo.moreyra@fiain.uncoma.edu.ar)

#### Co-director

- **Nombre**: Rafael Ignacio Zurita  
- **Correo electrónico**: [rafa@fi.uncoma.edu.ar](mailto:rafa@fi.uncoma.edu.ar)

## Resumen del trabajo realizado

El trabajo se concentró en desarrollar un driver de comunicación entre ROS2 y la interfaz Modbus RTU del robot (lectura de estado, comandos de velocidad, armado/desarmado del sistema), validado sobre dos bancos de pruebas sucesivos que replican dicha interfaz sin necesidad del hardware original.

### Paquetes ROS2 desarrollados

#### `chacras_interfaces`

Mensajes personalizados utilizados por el driver:

- `MotorState`: estado individual de un motor (setpoint, sentido, velocidad, corriente).
- `MotorStateArray`: arreglo de tamaño fijo con el estado de los 4 motores.
- `SystemStatus`: tensión y corriente de batería, estado de armado.
- `MotorCmd`: comando de velocidad para un motor determinado.

#### `chacras_driver`

- **`chacras_driver`** (nodo principal): mantiene una única conexión Modbus RTU con el robot (o su banco de pruebas). Publica `MotorStateArray` y `SystemStatus` periódicamente, se suscribe a `MotorCmd` para escribir comandos de velocidad, y expone un servicio (`std_srvs/SetBool`) para armar/desarmar el sistema.
- **`teleop_keyboard`** (nodo de demostración): teleoperación por teclado en modo diferencial (WASD), publicando `MotorCmd` a los 4 motores según la configuración de ruedas del robot (skid-steering).

Parámetros configurables (`config/chacras_params.yaml`): puerto serie, baudrate, período de sondeo.

### Bancos de prueba

Se utilizaron dos plataformas alternativas al robot 4WD, ambas implementando el mismo mapa de registros Modbus definido originalmente en el firmware de referencia (`references/chacras-original-2022`):

1. Una placa STM32F103 ("Blue Pill") idéntica a la presente en el robot.
2. **`bluepill-sim/chacras_slave.c`**: simulador por software que expone el mismo mapa de registros vía Modbus RTU, con un modelo simplificado de la dinámica de velocidad de los motores (respuesta de primer orden) y persistencia de estado entre ejecuciones.

---

## Quick start

### Requisitos del sistema

- **Sistema operativo:** [Ubuntu 24.04 LTS](https://ubuntu.com/download/alternative-downloads)
- **Framework:** ROS2 Jazzy.
- Python 3 con `pymodbus`
- `libmodbus` y `cJSON` (para compilar `bluepill-sim` y `modbus_client`)
- `socat` (opcional si se va a usar el simulador en lugar de la blue pill).

Para la instalación de ROS2 se recomienda seguir los pasos indicados en la [documentación oficial](https://docs.ros.org/en/jazzy/Installation.html).

Para instalar el resto de dependencias:

```bash
sudo apt update && sudo apt install -y \
    build-essential \
    libmodbus-dev \
    libcjson-dev \
    socat \
    python3-pip

pip3 install pymodbus
```

### Compilación

Clonar el repositorio y acceder a la raíz del espacio de trabajo:

```bash
git clone https://github.com/DarioAMarinRoth/beca-evc-2024
cd beca-evc-2024
```

Para compilar los paquetes de ROS, ejecutar el siguiente comando desde la carpeta raíz del proyecto:

``` bash
colcon build
source install/setup.bash
```

Para compilar el simulador y el cliente de referencia en C:

```bash
cd beca-evc-2024/src/bluepill-sim
gcc -o chacras_slave chacras_slave.c -lmodbus -lcjson -lm

cd ../modbus_client
gcc -o chacras chacras.c -lmodbus
```

### Ejecución

1. Conectar la blue pill o levantar el simulador con el script de shell `init_sim.sh` comando:

    ```bash
    cd src/bluepill-sim
    ./init_sim.sh
    ```

    >[!NOTE]
    > En el caso de usar la simulación, en el archivo `src/chacras_driver/config/chacras_params.yaml` debe configurarse como puerto `/tmp/ttyV0`. En el caso de usar la blue pill, debe configurarse con el puerto en el que se haya conectado.

2. En otra terminal, levantar el driver de ROS2:

   ```bash
   source install/setup.bash
   ros2 launch chacras_driver chacras_driver.launch.py
   ```

3. Armar el sistema y enviar comandos:

   ```bash
   ros2 service call /arm_system std_srvs/srv/SetBool "{data: true}"
   ros2 topic pub /motor_cmd chacras_interfaces/msg/MotorCmd "{motor: 0, setpoint: 100, sentido: 0}" --once
   ```

   O bien, usar el nodo de teleoperación:

   ```bash
   ros2 run chacras_driver teleop_keyboard
   ```

4. Para detener el banco de pruebas simulado:

   ```bash
   cd src/bluepill-sim
   ./kill_sim.sh
   ```

## Demo

En `demo/` se incluye el registro de una sesión de prueba completa (armado, comandos de velocidad, lectura de estado, desarmado), grabada con `ros2 bag`, junto con una captura del grafo de nodos/topics del sistema (`rqt_graph`).

Para reproducir la grabación:

```bash
ros2 bag play validacion/beca_evc_demo/
```

## Referencias

- Ortiz, F. G.; Wirth, L. E. (2022). *Desarrollo e implementación del sistema de control de bajo nivel de un robot 4WD para ambientes frutícolas.* Proyecto Integrador Profesional, Facultad de Ingeniería, Universidad Nacional del Comahue.
- Documentación oficial de ROS2 (Robot Operating System 2), disponible en https://docs.ros.org
- Especificación del protocolo Modbus (Modbus Organization), disponible en https://modbus.org
- Documentación de la librería pymodbus, disponible en https://pymodbus.readthedocs.io
- Documentación de la librería libmodbus, disponible en https://libmodbus.org
  
## Licencia

Uso institucional — Universidad Nacional del Comahue. Sin licencia de distribución pública.
