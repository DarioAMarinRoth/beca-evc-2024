import sys
import termios
import tty

import rclpy
from rclpy.node import Node

from chacras_interfaces.msg import MotorCmd

INSTRUCTIONS = """
Teleoperacion WASD (skid-steering) - chacras_driver
----------------------------------------------------
  w : adelante
  s : atras
  a : girar en el lugar hacia la izquierda
  d : girar en el lugar hacia la derecha
  espacio : detener los 4 motores
  CTRL+C  : salir
----------------------------------------------------
"""

# Asignacion fisica de motores a ruedas (confirmada por el usuario)
MOTOR_IZQ_DEL = 0
MOTOR_DER_DEL = 1
MOTOR_IZQ_TRAS = 2
MOTOR_DER_TRAS = 3

LADO_IZQUIERDO = (MOTOR_IZQ_DEL, MOTOR_IZQ_TRAS)
LADO_DERECHO = (MOTOR_DER_DEL, MOTOR_DER_TRAS)

# Sentido según el mapa Modbus (0 = horario, 1 = antihorario)
# Se asume que "adelante" del chacras corresponde a sentido=0 para todos los motores.
SENTIDO_ADELANTE = 0
SENTIDO_ATRAS = 1

SPEED = 100  # magnitud de setpoint usada para todos los movimientos


class TeleopKeyboardNode(Node):
    def __init__(self):
        super().__init__('teleop_keyboard_node')

        self._publisher = self.create_publisher(MotorCmd, '/motor_cmd', 10)

        self.get_logger().info('Teleop keyboard WASD iniciado.')

    def _publish_motor(self, motor_id: int, setpoint: int, sentido: int):
        msg = MotorCmd()
        msg.motor = motor_id
        msg.setpoint = setpoint
        msg.sentido = sentido
        self._publisher.publish(msg)

    def _set_side(self, motors, setpoint: int, sentido: int):
        for motor_id in motors:
            self._publish_motor(motor_id, setpoint, sentido)

    def forward(self):
        self._set_side(LADO_IZQUIERDO, SPEED, SENTIDO_ADELANTE)
        self._set_side(LADO_DERECHO, SPEED, SENTIDO_ADELANTE)
        self.get_logger().info('Adelante.')

    def backward(self):
        self._set_side(LADO_IZQUIERDO, SPEED, SENTIDO_ATRAS)
        self._set_side(LADO_DERECHO, SPEED, SENTIDO_ATRAS)
        self.get_logger().info('Atras.')

    def turn_left(self):
        # Giro en el lugar: lado izquierdo atras, lado derecho adelante
        self._set_side(LADO_IZQUIERDO, SPEED, SENTIDO_ATRAS)
        self._set_side(LADO_DERECHO, SPEED, SENTIDO_ADELANTE)
        self.get_logger().info('Girando a la izquierda (en el lugar).')

    def turn_right(self):
        # Giro en el lugar: lado izquierdo adelante, lado derecho atras
        self._set_side(LADO_IZQUIERDO, SPEED, SENTIDO_ADELANTE)
        self._set_side(LADO_DERECHO, SPEED, SENTIDO_ATRAS)
        self.get_logger().info('Girando a la derecha (en el lugar).')

    def stop_all(self):
        for motor_id in (MOTOR_IZQ_DEL, MOTOR_DER_DEL, MOTOR_IZQ_TRAS, MOTOR_DER_TRAS):
            self._publish_motor(motor_id, 0, SENTIDO_ADELANTE)
        self.get_logger().info('Motores detenidos.')

    def handle_key(self, key: str) -> bool:
        """Procesa una tecla. Retorna False si hay que salir."""
        if key == '\x03':  # CTRL+C
            return False

        actions = {
            'w': self.forward,
            's': self.backward,
            'a': self.turn_left,
            'd': self.turn_right,
            ' ': self.stop_all,
        }

        action = actions.get(key)
        if action:
            action()

        return True


def get_key(settings):
    """Lee un solo caracter del stdin sin esperar ENTER."""
    tty.setraw(sys.stdin.fileno())
    key = sys.stdin.read(1)
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
    return key


def main():
    rclpy.init()
    node = TeleopKeyboardNode()

    settings = termios.tcgetattr(sys.stdin)
    print(INSTRUCTIONS)

    try:
        while rclpy.ok():
            key = get_key(settings)
            if not node.handle_key(key):
                break
    except KeyboardInterrupt:
        pass
    finally:
        node.stop_all()
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()