import rclpy
from std_srvs.srv import SetBool
from rclpy.node import Node
from .modbus_client import ModbusClient
from chacras_interfaces.msg import MotorState, MotorStateArray, MotorCmd, SystemStatus



class ChacrasDriver(Node):

    def __init__(self, node_name: str = 'chacras_driver'):
        super().__init__(node_name)

        # Parameters
        self.declare_parameter('port', '/dev/ttyUSB0')
        self.declare_parameter('baudrate', 115200)
        self.declare_parameter('refresh_period_ms', 100)
        port = self.get_parameter('port').value
        baudrate = self.get_parameter('baudrate').value
        refresh_period_ms = self.get_parameter('refresh_period_ms').value


        self.get_logger().info(f'Node {node_name} initialized.')

        # Modbus connection
        self._client = ModbusClient(port=port, baudrate=baudrate)
        success = self._client.connect()
        if success:
            self.get_logger().info("Conexión Modbus establecida.")
        else:
            self.get_logger().error("Error al conectar con el cliente Modbus.")

        # Publishers
        qos = 10
        self._motors_publisher = self.create_publisher(MotorStateArray, '/motor_states', qos)
        self._status_publisher = self.create_publisher(SystemStatus, '/system_status', qos)
        self.create_timer(refresh_period_ms / 1000.0, self._publish)

        # Subscribers
        self.create_subscription(MotorCmd, '/motor_cmd', self._set_motor, qos)

        # Services
        self.create_service(SetBool, 'arm_system', self._set_armed_state)

    def _publish(self):
        self._publish_status()
        self._publish_motor_states()

    def _publish_motor_states(self):
        states = MotorStateArray()

        for i in range(4):
            motor_state = MotorState()
            motor_state.motor_id = i
            states.motors[i] = motor_state
            try:
                data = self._client.get_motor(i)
                motor_state.sentido = data['sentido']
                motor_state.setpoint = data['setpoint']
                motor_state.velocidad = data['velocidad']
                motor_state.corriente = data['corriente']
            except Exception as e:
                self.get_logger().error(f"Error reading motor {i}: {e}")
        self._motors_publisher.publish(states)

    def _publish_status(self):
        status = SystemStatus()
        try:
            data = self._client.get_status()
            status.corriente_bateria = data['ibat']
            status.tension_bateria = data['vbat']
            status.armado = data['armado'] == 1
        except Exception as e:
            self.get_logger().error(f"Error reading status: {e}")
        self._status_publisher.publish(status)

    def _set_armed_state(self, request, response):
        response.success = False
        arm_flag = request.data

        try:
            if arm_flag:
                self._client.set_armado(1)
                response.message = "Sistema armado."
                self.get_logger().info("Sistema armado.")
            else:
                self._client.set_armado(0)
                response.message = "Sistema desarmado."
                self.get_logger().info("Sistema desarmado.")
            response.success = True
        except Exception as e:
            self.get_logger().error(f"Error armando el sistema: {e}")
            response.success = False
            response.message = f"Error armando el sistema: {e}"
        return response

    def _set_motor(self, command: MotorCmd):
        try:
            self._client.set_motor(command.motor, command.setpoint, command.sentido)
        except Exception as e:
            self.get_logger().error(f"Error setting motor: {e}")

    def destroy_node(self):
        self._client.disconnect()
        super().destroy_node()


def main():
    rclpy.init()
    driver = ChacrasDriver()
    rclpy.spin(driver)
    driver.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
