import geomag

def wrap360(x: float) -> float:
    x = x % 360.0
    return x + 360.0 if x < 0 else x

def declination_deg(lat_deg: float, lon_deg: float) -> float:
    return float(geomag.declination(lat_deg, lon_deg))

def true_az_to_mount_az(az_true_deg: float, decl_deg: float) -> float:
    return wrap360(az_true_deg - decl_deg)
