import time
from arduino_comm import ArduinoMount
from coords import AltAzProvider
from magnetic import declination_deg, true_az_to_mount_az

PORT = "COM3"

LAT_DEG = 43.88
LON_DEG = -78.94
ELEV_M  = 100.0

DEFAULT_HZ = 2.0

def _is_number(s: str) -> bool:
    try:
        float(s)
        return True
    except:
        return False

def _goto_any(mount, astro, decl, args):
    if len(args) == 2 and _is_number(args[0]) and _is_number(args[1]):
        az_true = float(args[0])
        alt = float(args[1])
        az_mount = true_az_to_mount_az(az_true, decl)
        return mount.goto(az_mount, alt)

    if len(args) == 1:
        tok = args[0].strip()
        az_true, alt = astro.get_az_alt(tok)
        az_mount = true_az_to_mount_az(az_true, decl)
        return mount.goto(az_mount, alt)

    return ""

def main():
    mount = ArduinoMount(PORT)
    astro = AltAzProvider(LAT_DEG, LON_DEG, ELEV_M, use_refraction=False)
    decl = declination_deg(LAT_DEG, LON_DEG)

    try:
        while True:
            s = input("> ").strip()
            if not s:
                continue
            parts = s.split()
            cmd = parts[0].lower()
            args = parts[1:]

            if cmd in ("quit", "exit"):
                break

            if cmd == "sync":
                print(mount.sync())
                continue

            if cmd == "where":
                print(mount.where())
                continue

            if cmd == "sethome":
                print(mount.sethome())
                continue

            if cmd == "gohome":
                print(mount.gohome())
                continue

            if cmd == "goto":
                r = _goto_any(mount, astro, decl, args)
                if r:
                    print(r)
                continue

            if cmd == "slew":
                r = _goto_any(mount, astro, decl, args)
                if r:
                    print(r)
                continue

            if cmd == "track":
                obj = args[0]
                hz = float(args[1]) if len(args) >= 2 else DEFAULT_HZ
                dt = 1.0 / hz
                try:
                    while True:
                        az_true, alt = astro.get_az_alt(obj)
                        az_mount = true_az_to_mount_az(az_true, decl)
                        mount.goto(az_mount, alt)
                        time.sleep(dt)
                except KeyboardInterrupt:
                    continue

            else:
                pass

    finally:
        mount.close()

if __name__ == "__main__":
    main()
