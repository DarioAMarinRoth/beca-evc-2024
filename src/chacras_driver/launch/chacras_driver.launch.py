from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('chacras_driver'),
        'config',
        'chacras_params.yaml'
    )
    return LaunchDescription([
        Node(
            package='chacras_driver',
            executable='chacras_driver',
            name='chacras_driver',
            parameters=[config],
        ),
    ])