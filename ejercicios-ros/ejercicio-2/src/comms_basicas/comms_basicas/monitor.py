import rclpy
from rclpy.node import Node
from std_msgs.msg import Float32

class Monitor(Node):

    def __init__(self, node_name):
        super().__init__(node_name)
        qos = 10
        self.create_subscription(Float32, "/voltaje_bateria", self.log_voltage, qos)

    def log_voltage(self, msg):
        print(f"Voltaje recibido: {msg.data:.2f}")

def main() -> None:
    rclpy.init()
    monitor = Monitor("monitor")
    rclpy.spin(monitor)
    monitor.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()