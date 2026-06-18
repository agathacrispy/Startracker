import serial
import time

def _clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v

class ArduinoMount:
    def __init__(self, port: str, baud: int = 115200, timeout: float = 1.0, write_timeout: float = 1.0):
        self.ser = serial.Serial(port, baud, timeout=timeout, write_timeout=write_timeout)
        self.ser.setDTR(False)
        self.ser.setRTS(False)
        time.sleep(0.2)
        try:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
        except:
            pass

    def close(self):
        self.ser.close()

    def _txrx(self, line: str) -> str:
        if not line.endswith("\n"):
            line += "\n"
        self.ser.write(line.encode("ascii"))
        self.ser.flush()
        return self.ser.readline().decode(errors="ignore").strip()

    def sync(self) -> str:
        return self._txrx("SYNC")

    def where(self) -> str:
        return self._txrx("WHERE")

    def sethome(self) -> str:
        return self._txrx("SETHOME")

    def gohome(self) -> str:
        return self._txrx("GOHOME")

    def goto(self, az_deg: float, alt_deg: float) -> str:
        alt_deg = _clamp(alt_deg, 0.0, 90.0)
        return self._txrx(f"G,{az_deg:.6f},{alt_deg:.6f}")
