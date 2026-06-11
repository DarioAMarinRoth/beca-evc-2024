import struct
from pymodbus.client import ModbusSerialClient

# Mapa de registros (igual que la blue pill)
# 0-5    Motor 0: setpoint, sentido, vel_H, vel_L, corr_H, corr_L
# 6-11   Motor 1
# 12-17  Motor 2
# 18-23  Motor 3
# 24-25  Corriente batería H/L  (float)
# 26-27  Tensión batería H/L    (float)
# 28     Armado
# 29-31  Reservado

CHACRAS_ADDR  = 1
MOTOR_BASE    = 0
STATUS_BASE   = 24
ARMADO_BASE   = 28
NUM_MOTORS    = 4


def _regs_to_float(hi: int, lo: int) -> float:
    raw = ((hi & 0xFFFF) << 16) | (lo & 0xFFFF)
    return struct.unpack('>f', struct.pack('>I', raw))[0]


class Chacras:
    def __init__(self, port: str = '/dev/ttyV0', baudrate: int = 115200):
        self._client = ModbusSerialClient(
            port=port,
            baudrate=baudrate,
            bytesize=8,
            parity='N',
            stopbits=1,
            timeout=1,
        )
        self._slave = CHACRAS_ADDR

    def connect(self) -> bool:
        return self._client.connect()

    def disconnect(self):
        self._client.close()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.disconnect()

    # ------------------------------------------------------------------ #
    #  Motores                                                           #
    # ------------------------------------------------------------------ #

    def get_motor(self, motor: int) -> dict:
        """Lee el estado completo de un motor (0-3).

        Retorna:
            {
                'setpoint':  int,
                'sentido':   int,
                'velocidad': float,
                'corriente': float,
            }
        """
        if not 0 <= motor <= 3:
            raise ValueError(f"Motor debe ser 0-3, recibido: {motor}")

        base = MOTOR_BASE + motor * 6
        result = self._client.read_holding_registers(base, count=6, slave=self._slave)
        if result.isError():
            raise IOError(f"Error leyendo motor {motor}: {result}")

        r = result.registers
        return {
            'setpoint':  r[0],
            'sentido':   r[1],
            'velocidad': _regs_to_float(r[2], r[3]),
            'corriente': _regs_to_float(r[4], r[5]),
        }

    def set_motor(self, motor: int, setpoint: int, sentido: int):
        """Establece setpoint y sentido de un motor (0-3)."""
        if not 0 <= motor <= 3:
            raise ValueError(f"Motor debe ser 0-3, recibido: {motor}")

        base = MOTOR_BASE + motor * 6
        result = self._client.write_registers(base, [setpoint, sentido], slave=self._slave)
        if result.isError():
            raise IOError(f"Error escribiendo motor {motor}: {result}")

    # ------------------------------------------------------------------ #
    #  Status                                                            #
    # ------------------------------------------------------------------ #

    def get_status(self) -> dict:
        """Lee tensión de batería, corriente total y estado de armado.

        Retorna:
            {
                'ibat':   float,   # corriente total (A)
                'vbat':   float,   # tensión batería (V)
                'armado': int,     # 0 o 1
            }
        """
        result = self._client.read_holding_registers(STATUS_BASE, count=5, slave=self._slave)
        if result.isError():
            raise IOError(f"Error leyendo status: {result}")

        r = result.registers
        return {
            'ibat':   _regs_to_float(r[0], r[1]),
            'vbat':   _regs_to_float(r[2], r[3]),
            'armado': r[4],
        }

    # ------------------------------------------------------------------ #
    #  Armado                                                            #
    # ------------------------------------------------------------------ #

    def set_armado(self, valor: int):
        """Arma o desarma el sistema (1 = armado, 0 = desarmado)."""
        result = self._client.write_registers(ARMADO_BASE, [valor], slave=self._slave)
        if result.isError():
            raise IOError(f"Error escribiendo armado: {result}")


# ------------------------------------------------------------------ #
#  Ejemplo de uso                                                    #
# ------------------------------------------------------------------ #

if __name__ == '__main__':
    with Chacras(port='/tmp/ttyV0') as c:

        # # status
        # status = c.get_status()
        # print(f"Status: {status}")

        # # leer motor 0
        # motor = c.get_motor(0)
        # print(f"Motor 0: {motor}")

        # setear motor 0
        c.set_motor(0, setpoint=100, sentido=1)
        print("Motor 0 seteado: setpoint=100 sentido=1")

        # # armar sistema
        c.set_armado(0)
        print("Sistema armado")