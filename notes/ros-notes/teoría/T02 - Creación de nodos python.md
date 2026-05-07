# Nodos ROS 2 con Python (rclpy)

## 1. Fundamentos: ROS 2 como Middleware Robótico

En la arquitectura de software moderna, no empezamos a programar un robot desde cero. Como arquitectos, entendemos ROS 2 no como un simple sistema operativo, sino como un **middleware**: una capa de abstracción crítica que se sitúa entre el hardware y nuestras aplicaciones inteligentes.

**Middleware:** Es una capa de software que reside entre el sistema operativo y las aplicaciones del usuario. Su función es facilitar la comunicación y la gestión de datos en sistemas distribuidos complejos, permitiendo que diversos componentes (sensores, actuadores y algoritmos) trabajen como un ecosistema integrado y coherente.

Para un desarrollador principiante, es vital abordar ROS 2 desde tres dimensiones complementarias:

- **La Comunidad:** Es el pilar humano y técnico. Gracias a organizaciones como la **Asociación Española de Usuarios de ROS** y eventos como **ROSCon España** (Sevilla 2024, Barcelona 2025), disponemos de una red global que comparte drivers y soluciones. La distribución recomendada actualmente es **Jazzy Jalisco** (Ubuntu 24.04), con soporte extendido (LTS) hasta **mayo de 2029**.
- **El Grafo de Computación:** La dimensión dinámica. Es la red de nodos que intercambian mensajes mediante tópicos, servicios y acciones mientras el robot opera.
- **El Workspace (Espacio de Trabajo):** La dimensión estática. Es la estructura organizada de carpetas donde desarrollamos y compilamos nuestro código antes de que pase a formar parte del grafo.

**Síntesis de Insight: La Arquitectura de Integración** Francisco Martín Rico define la robótica como "el Arte de la Integración". En lugar de escribir protocolos seriales crudos para un robot Kobuki o un Tiago, integramos interfaces estandarizadas. Por ejemplo, en lugar de bits binarios, manejamos un `kobuki_msgs/msg/BumperEvent` para colisiones o un `geometry_msgs/msg/Twist` para el movimiento. Esta abstracción permite que el desarrollador se centre en la lógica de alto nivel.

--------------------------------------------------------------------------------

## 2. Anatomía de un Paquete ROS 2 en Python

Todo desarrollo profesional comienza con una estructura de archivos estricta que permite a las herramientas de ROS 2 (como `colcon` y `ros2 run`) localizar y ejecutar nuestro código.

**Estructura del Workspace y Paquete:**

```text
ros2_ws/ (Workspace)
└── src/ (Dimensión Estática)
    └── br2_basics/ (Paquete de Python)
        ├── br2_basics/
        │   ├── __init__.py
        │   └── logger_node.py
        ├── package.xml
        ├── setup.py
        └── setup.cfg
```

**Componentes Críticos del Paquete:**

|                   |                                                                                                                                                                                                       |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Archivo / Carpeta | Propósito                                                                                                                                                                                             |
| `package.xml`     | Define los metadatos y declara las **dependencias de ejecución** (ej. `rclpy`). Es el manifiesto que el sistema consulta para instalar librerías necesarias.                                          |
| `setup.py`        | Configura la instalación del paquete. Aquí se definen los **Entry Points**, vinculando un comando de consola con una función de Python.                                                               |
| `setup.cfg`       | **Esencial para el Arquitecto:** Indica dónde debe colocar el sistema los scripts ejecutables (normalmente en la carpeta `lib` del prefijo de instalación) para que el OS y `ros2 run` los localicen. |
| `br2_basics/`     | El módulo de Python que contiene la lógica real del nodo.                                                                                                                                             |

--------------------------------------------------------------------------------

## 3. Implementación Orientada a Objetos: La Clase `Node`

En ROS 2, la librería `rclpy` se construye sobre la API de C (`rcl`), garantizando un comportamiento consistente entre lenguajes. Para aprovechar esto, implementamos nodos mediante **Programación Orientada a Objetos (OOP)**, heredando de la clase base `rclpy.node.Node`.

https://docs.ros2.org/foxy/api/rclpy/api/node.html

```python
import rclpy
from rclpy.node import Node

class SimpleLogger(Node):
    def __init__(self):
        # Inicialización de la clase base con el nombre del nodo
        super().__init__('logger_node')
        self.get_logger().info('Nodo Logger inicializado en el grafo.')
```

> [!Note]
> Los nombres en ROS 2 siguen una jerarquía similar a un sistema de archivos:
> - **Nombre Relativo (**`my_topic`**):** Se expande según el namespace del nodo.
> - **Nombre Absoluto (**`/my_topic`**):** Ignora el namespace del nodo, situándose en la raíz.
> - **Nombre Privado (**`~my_topic`**):** Se anida dentro del nombre del nodo (ej. `/logger_node/my_topic`), ideal para parámetros internos.


**Beneficios de OOP:**

1. **Encapsulamiento de Callbacks:** Las funciones que responden a sensores tienen acceso nativo al estado interno del nodo (sensores, odometría, variables).
2. **Reutilización:** Permite instanciar múltiples versiones de un nodo (ej. dos cámaras) con configuraciones distintas sin duplicar código.

--------------------------------------------------------------------------------

## 4. Ciclo de Vida de un Nodo - Inicialización, Spin y Executor

Un nodo no es solo un script; es un proceso gestionado que pasa por tres fases fundamentales:

1. **Inicialización:** Mediante `rclpy.init(args=args)`. Esta función prepara el contexto de comunicación y procesa los argumentos del middleware.
2. **Ciclo de Ejecución (Spin):** La llamada a `rclpy.spin(node)` activa el **Executor**. Este mecanismo es el corazón del nodo; sin él, los _callbacks_ (de timers o suscripciones) nunca se procesarían. El nodo "vive" en el Grafo de Computación gracias a que el _spin_ coordina la llegada de eventos.
3. **Apagado:** Una vez finalizada la tarea o recibido un _SIGINT_ (Ctrl+C), se deben liberar recursos con `node.destroy_node()` y cerrar el contexto con `rclpy.shutdown()`.

**Modelos de Ejecución:**

- **Iterativa (Basada en Frecuencia):** Ideal para control de movimiento (ej. publicar comandos de velocidad a 20 Hz constantes).
- **Orientada a Eventos:** Basada en la llegada de información (ej. procesar una imagen de una cámara RGBD solo cuando el driver la publica).

--------------------------------------------------------------------------------

## 5. Registro del Script: Entry Points y Compilación

Para que un script se convierta en una herramienta del sistema, debemos registrarlo en el bloque `entry_points` de `setup.py`:

```python
entry_points={
    'console_scripts': [
        'logger_exec = br2_basics.logger_node:main',
    ],
},
```

_(Sintaxis: 'nombre_ejecutable = paquete.archivo:función_main')_

Tras el registro, el flujo de trabajo es:

1. **Compilación:** `colcon build --packages-select br2_basics` en la raíz del workspace.
2. **Activación:** `source install/setup.bash`. Este paso es crítico para que la terminal actual conozca la ubicación de los nuevos binarios y el sistema de autocompletado funcione.

--------------------------------------------------------------------------------

## 6. Ejemplo Práctico: Nodo de Log Profesional (Template)

Este código sigue los estándares del paquete `br2_basics` mencionado en el ecosistema de Francisco Martín Rico.

```python
import rclpy
from rclpy.node import Node

class LoggerNode(Node):
    def __init__(self):
        # Nombre único en el Grafo de Computación
        super().__init__('br2_logger_node')
        # Timer para ejecución iterativa (1.0 Hz)
        self.timer = self.create_timer(1.0, self.timer_callback)
        self.get_logger().info('LoggerNode activo. Enviando datos a /rosout.')

    def timer_callback(self):
        # get_logger() permite niveles: info, warn, error
        # Estos mensajes pueden monitorearse en tiempo real con rqt_console
        self.get_logger().info('Procesando ciclo de control...')

def main(args=None):
    # 1. Inicializar middleware
    rclpy.init(args=args)
    
    # 2. Instanciar el Nodo (Arquitectura OOP)
    node = LoggerNode()

    try:
        # 3. Spin: El Executor toma el control de los callbacks
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.get_logger().warn('Interrupción por teclado detectada.')
    finally:
        # 4. Limpieza garantizada de recursos
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
```

**Claves de Robustez:**

- **Niveles de Log:** El uso de `info`, `warn` y `error` permite filtrar información crítica en herramientas como `rqt_console`.
- **Gestión de Excepciones:** El bloque `try/finally` asegura que el nodo se cierre correctamente incluso ante errores inesperados, evitando procesos "zombis" en el middleware.

--------------------------------------------------------------------------------

## 7. Conclusión y Mejores Prácticas

Crear un nodo es el primer paso para dominar la arquitectura de un robot. Antes de lanzar su código, asegúrese de cumplir con los estándares de calidad.

### Lista de Verificación de Éxito

- [ ] **Validación de Espacios de Nombres:** ¿El nombre del nodo es descriptivo y evita colisiones en el grafo?
- [ ] **Declaración de Dependencias:** ¿Está `rclpy` incluido en el `package.xml`?
- [ ] **Visibilidad del Ejecutable:** ¿El `setup.cfg` está configurado y el Entry Point en `setup.py` es correcto?
- [ ] **Configuración de QoS:** ¿Se han considerado las políticas de calidad de servicio si el nodo opera en redes inestables?
- [ ] **Depuración:** ¿Se visualizan los logs correctamente en `rqt_console`?

Dominar estas herramientas es solo el comienzo. Como señala Francisco Martín Rico en su obra de 2025: **"The true robotics surge when utilizing this infrastructure to give life to intelligent behaviors."** (La verdadera robótica surge cuando se utiliza esta infraestructura para dar vida a comportamientos inteligentes). Bienvenido al fascinante camino de la integración robótica.