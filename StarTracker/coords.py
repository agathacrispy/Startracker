from skyfield.api import load, wgs84, Star
from skyfield.data import hipparcos

class AltAzProvider:
    def __init__(self, lat_deg: float, lon_deg: float, elevation_m: float = 0.0, use_refraction: bool = False):
        self.ts = load.timescale()
        self.eph = load("de421.bsp")
        self.site = wgs84.latlon(latitude_degrees=lat_deg,
                                 longitude_degrees=lon_deg,
                                 elevation_m=elevation_m)
        self.use_refraction = use_refraction

        self._hips = None
        self._star_cache = {}

        self.bodies = {
            "sun": self.eph["sun"],
            "moon": self.eph["moon"],
            "mercury": self.eph["mercury"],
            "venus": self.eph["venus"],
            "mars": self.eph["mars"],
            "jupiter": self.eph["jupiter barycenter"],
            "saturn": self.eph["saturn barycenter"],
            "uranus": self.eph["uranus barycenter"],
            "neptune": self.eph["neptune barycenter"],
        }

        self.named_stars = {
            "polaris": 11767,
            "sirius": 32349,
            "vega": 91262,
            "betelgeuse": 27989,
            "rigel": 24436,
        }

    def _get_star_by_hip(self, hip_id: int) -> Star:
        if hip_id in self._star_cache:
            return self._star_cache[hip_id]
        if self._hips is None:
            with load.open(hipparcos.URL) as f:
                self._hips = hipparcos.load_dataframe(f)
        row = self._hips.loc[hip_id]
        star = Star.from_dataframe(row)
        self._star_cache[hip_id] = star
        return star

    def _altaz(self, target):
        t = self.ts.now()
        observer = self.eph["earth"] + self.site
        astrometric = observer.at(t).observe(target).apparent()
        if self.use_refraction:
            alt, az, _ = astrometric.altaz()
        else:
            alt, az, _ = astrometric.altaz(pressure_mbar=0)
        return az.degrees, alt.degrees

    def get_az_alt(self, obj: str):
        key = str(obj).strip().lower()

        if key in self.bodies:
            return self._altaz(self.bodies[key])

        if key.isdigit():
            hip_id = int(key)
            return self._altaz(self._get_star_by_hip(hip_id))

        if key.startswith("hip:"):
            hip_id = int(key.split(":", 1)[1])
            return self._altaz(self._get_star_by_hip(hip_id))

        if key.startswith("star:"):
            name = key.split(":", 1)[1].strip().lower()
            return self._altaz(self._get_star_by_hip(self.named_stars[name]))

        if key in self.named_stars:
            return self._altaz(self._get_star_by_hip(self.named_stars[key]))

        raise KeyError(key)

