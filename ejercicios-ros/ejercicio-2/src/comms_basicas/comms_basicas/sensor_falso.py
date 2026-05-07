import rclpy
import random

from rclpy.node import Node
from std_msgs.msg import Float32


class Sensor(Node):

    def __init__(self, node_name, freq=10):
        super().__init__(node_name)
        qos = 10 # número máximo de mensajes que se almacenan en el buffer
        self._publisher = self.create_publisher(Float32, "/voltaje_bateria", qos)
        self._freq = freq
        self._period = 1 / freq
        self.create_timer(self._period, self.publish)

    def publish(self):
        msg = Float32()
        msg.data = round(random.random() * 24, 1)
        self._publisher.publish(msg)

def main() -> None:
    rclpy.init()
    sensor = Sensor("sensor_falso")
    rclpy.spin(sensor)
    sensor.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
