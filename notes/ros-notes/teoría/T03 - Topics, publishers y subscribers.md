# Guía de Implementación de Topics, Publishers y Subscribers en ROS 2 con Python (rclpy)

El desarrollo de aplicaciones robóticas en ROS 2 se fundamenta en el **Grafo de Computación**, una red dinámica de nodos que colaboran entre sí. La piedra angular de esta colaboración es el paradigma de **Publicación/Suscripción**, un modelo de comunicación asíncrona que permite el intercambio de datos a través de canales denominados **topics**.

## 1. Arquitectura y Archivos del Proceso de Comunicación

Para implementar un modelo de comunicación en ROS 2 utilizando Python, intervienen diversos componentes y archivos estructurados dentro del espacio de trabajo (_Workspace_):

- **Paquete (Package):** El contenedor de software. Para Python, utiliza el sistema de construcción `ament_python`.
- **Carpeta** `**msg/**`**:** Directorio crítico ubicado en la raíz del paquete donde se definen las interfaces personalizadas. Contiene los archivos `.msg`.
- **Archivos** `**.msg**`**:** Archivos de texto simple que utilizan el Lenguaje de Definición de Interfaces (IDL) para describir los campos de un mensaje.
- **Scripts de Python:** Ubicados usualmente en la carpeta con el nombre del paquete, contienen la lógica de los nodos utilizando la librería `rclpy`.
- `**package.xml**`**:** Define las dependencias del paquete, incluyendo las necesarias para generar mensajes (`rosidl_default_generators`) y las dependencias de ejecución (`rosidl_default_runtime`).

## 2. Definición de Interfaces: La Carpeta `msg/` y la Sintaxis `.msg`

Los mensajes son la forma en que los nodos envían datos por la red sin esperar una respuesta. Se definen estrictamente en archivos con extensión `.msg`.

### Estructura de la Carpeta

La normativa de ROS 2 exige que todos los archivos de mensajes residan en una carpeta denominada exactamente `msg/` dentro del paquete. Por ejemplo: `mi_paquete/msg/MiMensaje.msg`

### Sintaxis Estricta de Archivos `.msg`

Cada línea en un archivo `.msg` define un campo compuesto por un **tipo** y un **nombre**, separados por un espacio.

#### Tipos de Datos Soportados

|   |   |   |
|---|---|---|
|Categoría|Tipos|Ejemplo|
|**Numéricos**|`int8`, `int16`, `int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64`, `float32`, `float64`|`int32 contador`|
|**Lógicos/Texto**|`bool`, `string`, `byte`, `char`|`string datos`|
|**Anidados**|Nombres de otras descripciones de mensajes|`geometry_msgs/Pose pose`|
|**Arreglos Dinámicos**|`tipo[]`|`int32[] lista_numeros`|
|**Arreglos Fijos**|`tipo[N]`|`float32[3] coordenadas`|
|**Tipos Acotados**|`string<=N`, `tipo[<=N]`|`string<=10 corto`|

#### Reglas de Nomenclatura de Campos

- Deben ser caracteres alfanuméricos en **minúsculas** con guiones bajos para separar palabras.
- Deben comenzar con un carácter alfabético.
- No pueden terminar con un guion bajo ni tener dos guiones bajos consecutivos.

### Valores por Defecto y Constantes

Es posible asignar valores iniciales o definir valores inalterables:

- **Valores por defecto:** Se añade un tercer elemento a la línea.
    - Ejemplo: `uint8 x 42`
    - Ejemplo: `string mensaje "hola"` (las cadenas deben usar comillas simples o dobles).
- **Constantes:** Se utiliza el signo `=` y el nombre debe ser en **MAYÚSCULAS**.
    - Ejemplo: `int32 X=123`

## 3. Implementación de Nodos con `rclpy`

La librería `rclpy` es la interfaz idiomática de Python para ROS 2. Construida sobre la API de C (`rcl`), permite manejar el modelo de ejecución mediante hilos y objetos nativos de Python.

### Definición de un Nodo (Clase Node)

En ROS 2, un nodo es generalmente un objeto de la clase `Node`.

```python
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class MiNodo(Node):
    def __init__(self):
        super().__init__('nombre_del_nodo')
        # Configuración del nodo
```

### Implementación del Publisher

Un publicador envía mensajes a un topic específico a una frecuencia determinada (ejecución iterativa) o ante un evento.

**Sintaxis Estricta:** `self.create_publisher(msg_type, topic_name, qos_profile)`

**Ejemplo Práctico:**

```python
class Talker(Node):
    def __init__(self):
        super().__init__('talker')
        # Crea el publicador: Tipo String, Topic '/chatter', Profundidad de cola 10
        self.publisher_ = self.create_publisher(String, 'chatter', 10)
        
        # Define una frecuencia de ejecución (1 Hz)
        timer_period = 1.0  
        self.timer = self.create_timer(timer_period, self.timer_callback)
        self.i = 0

    def timer_callback(self):
        msg = String()
        msg.data = f'Hello World: {self.i}'
        self.publisher_.publish(msg)
        self.get_logger().info(f'Publicando: "{msg.data}"')
        self.i += 1
```

### Implementación del Subscriber

Un suscriptor recibe mensajes asíncronamente. Su ejecución está orientada a eventos (la llegada de un mensaje).

**Sintaxis Estricta:** `self.create_subscription(msg_type, topic_name, callback, qos_profile)`

**Ejemplo Práctico:**

```python
class Listener(Node):
    def __init__(self):
        super().__init__('listener')
        # Crea la suscripción: Tipo String, Topic '/chatter', Función callback, Cola 10
        self.subscription = self.create_subscription(
            String,
            'chatter',
            self.listener_callback,
            10)

    def listener_callback(self, msg):
        # Esta función se ejecuta cada vez que llega un mensaje
        self.get_logger().info(f'He escuchado: "{msg.data}"')
```

## 4. Convenciones de Nombres y Espacios de Nombres

Los nombres de los recursos (nodos, topics) siguen convenciones similares a los sistemas de archivos de Unix:

- **Relativo:** `mi_topic` (se expande según el namespace del nodo).
- **Absoluto:** `/mi_topic` (ignora el namespace del nodo).
- **Privado:** `~mi_topic` (se expande incluyendo el nombre del nodo: `/nombre_nodo/mi_topic`).

Los **Namespaces** son fundamentales para aplicaciones multi-robot, permitiendo aislar recursos agregando un prefijo al nombre del nodo y de sus topics.

## 5. Modelos de Ejecución en `rclpy`

Al diseñar el publicador o suscriptor, se debe elegir un modelo de ejecución:

1. **Ejecución Iterativa:** Ideal para control. El nodo ejecuta un ciclo a una frecuencia específica (ej. 20 Hz) usando un `timer`. Esto asegura que el flujo de salida sea constante independientemente de las entradas.
2. **Ejecución Orientada a Eventos:** La ejecución se dispara por la llegada de mensajes. El nodo permanece inactivo hasta que el middleware recibe un dato en el topic suscrito, ejecutando entonces la función _callback_.

## 6. Resumen de Flujo de Datos

En el Grafo de Computación, el proceso de comunicación sigue este orden:

1. **Definición:** Se crea el `.msg` con la sintaxis `tipo nombre`.
2. **Instanciación:** El nodo publicador crea un objeto del mensaje, llena sus campos y llama a `publish()`.
3. **Transporte:** El middleware (DDS) transporta el mensaje de forma asíncrona.
4. **Recepción:** El nodo suscriptor detecta el mensaje y dispara su _callback_, recibiendo el objeto del mensaje como argumento para procesarlo.

Este modelo permite que N nodos publiquen en un topic y M nodos se suscriban a él, facilitando la integración de sensores (como cámaras o láseres) con nodos de percepción y control.