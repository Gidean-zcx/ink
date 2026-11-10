"""天气（D-125）：设备只说城市编号，中枢查数据源并按城市缓存 30 分钟。

数据源可替换：任何有 search、place、forecast 三个异步方法、返回 Open-Meteo 形状数据的对象都行。
Open-Meteo 免费接口只许非商业使用，要署名（CC BY 4.0）；商用要换付费接口或别的源。
"""
import time

from ink_hub.gateway.wire import ProtocolError

CACHE_S = 30 * 60
DAYS = 3  # after today
# WMO weather codes -> (text, icon). Icons: sun, cloudsun, cloud, rain, snow.
CODES = {0: ("晴", "sun"), 1: ("晴间多云", "cloudsun"), 2: ("多云", "cloudsun"), 3: ("阴", "cloud"),
         45: ("雾", "cloud"), 48: ("雾", "cloud"), 51: ("毛毛雨", "rain"), 53: ("毛毛雨", "rain"),
         55: ("毛毛雨", "rain"), 56: ("冻雨", "rain"), 57: ("冻雨", "rain"), 61: ("小雨", "rain"),
         63: ("中雨", "rain"), 65: ("大雨", "rain"), 66: ("冻雨", "rain"), 67: ("冻雨", "rain"),
         71: ("小雪", "snow"), 73: ("中雪", "snow"), 75: ("大雪", "snow"), 77: ("雪粒", "snow"),
         80: ("阵雨", "rain"), 81: ("阵雨", "rain"), 82: ("强阵雨", "rain"), 85: ("阵雪", "snow"),
         86: ("阵雪", "snow"), 95: ("雷阵雨", "rain"), 96: ("雷阵雨", "rain"), 99: ("雷阵雨", "rain")}


def describe(code):
    return CODES.get(code, ("未知", "cloud"))


class OpenMeteo:
    """真实数据源，联网；CI 不跑。"""
    GEO = "https://geocoding-api.open-meteo.com/v1"
    FORECAST = "https://api.open-meteo.com/v1/forecast"

    async def _get(self, url, params):
        from aiohttp import ClientSession, ClientTimeout
        async with ClientSession(timeout=ClientTimeout(total=10)) as session:
            async with session.get(url, params=params) as reply:
                reply.raise_for_status()
                return await reply.json()

    async def search(self, name):
        data = await self._get(self.GEO + "/search", {"name": name, "count": 5, "language": "zh", "format": "json"})
        return data.get("results", [])

    async def place(self, city_id):
        return await self._get(self.GEO + "/get", {"id": city_id, "language": "zh"})

    async def forecast(self, latitude, longitude):
        return await self._get(self.FORECAST, {
            "latitude": latitude, "longitude": longitude, "current": "temperature_2m,weather_code",
            "daily": "weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max",
            "timezone": "auto", "forecast_days": DAYS + 1})


class Weather:
    def __init__(self, source=None, clock=time.time):
        self.source, self.clock = source or OpenMeteo(), clock
        self.cache = {}

    async def _ask(self, call, *args):
        try:
            return await call(*args)
        except ProtocolError:
            raise
        except Exception:
            raise ProtocolError("天气暂时取不到", -32603) from None

    async def find(self, params):
        name = params.get("name")
        if set(params) != {"name"} or not isinstance(name, str) or not 1 <= len(name) <= 20:
            raise ProtocolError("需要城市名")
        cities = []
        for r in (await self._ask(self.source.search, name))[:5]:
            area = " · ".join(x for x in (r.get("admin1"), r.get("country")) if x and x != r.get("name"))
            cities.append({"id": int(r["id"]), "name": str(r["name"])[:10], "area": area[:16]})
        return {"cities": cities}

    async def get(self, params):
        city_id = params.get("id")
        if set(params) != {"id"} or type(city_id) is not int or city_id <= 0:
            raise ProtocolError("需要城市编号")
        now = self.clock()
        hit = self.cache.get(city_id)
        if hit and now - hit["at"] < CACHE_S:
            return hit
        place = await self._ask(self.source.place, city_id)
        if not place or "latitude" not in place:
            raise ProtocolError("没有这个城市")
        raw = await self._ask(self.source.forecast, place["latitude"], place["longitude"])
        try:
            current, daily = raw["current"], raw["daily"]
            def day(i):
                text, icon = describe(daily["weather_code"][i])
                rain = (daily.get("precipitation_probability_max") or [None] * (DAYS + 1))[i]
                return [text, icon, round(daily["temperature_2m_max"][i]), round(daily["temperature_2m_min"][i]),
                        -1 if rain is None else int(rain)]
            text, icon = describe(current["weather_code"])
            today = day(0)
            result = {"city": str(place.get("name", ""))[:10], "at": int(now), "t": round(current["temperature_2m"]),
                      "w": text, "i": icon, "hi": today[2], "lo": today[3], "p": today[4],
                      "days": [day(i) for i in range(1, min(DAYS + 1, len(daily["weather_code"])))]}
        except (KeyError, IndexError, TypeError, ValueError):
            raise ProtocolError("天气暂时取不到", -32603) from None
        if len(self.cache) >= 64:
            self.cache.clear()
        self.cache[city_id] = result
        return result
