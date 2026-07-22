import os
from glob import glob

from setuptools import find_packages, setup

package_name = 'chacras_driver'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
         ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'config'), glob('config/*.yaml')),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='dario',
    maintainer_email='dariomarinroth@gmail.com',
    description='Driver ROS2 para comunicación Modbus RTU con el robot 4WD (lectura de estado, comandos de velocidad, armado/desarmado).',
    license='Uso institucional — Universidad Nacional del Comahue (sin licencia de distribución pública)',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'chacras_driver = chacras_driver.chacras_driver:main',
            'teleop_keyboard = chacras_driver.teleop_keyboard:main'
        ],
    },
)
