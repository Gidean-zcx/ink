"""天气：地名查询、按城市缓存 30 分钟、数据源出错。只用假数据源，不联网。"""
import pytest

from ink_hub.content.weather import CACHE_S, Weather
from ink_hub.gateway.wire import ProtocolError, response, tokens
from tests.fake_device import FakeDevice

PLACES = {101: {"id": 101, "name": "示例市", "admin1": "示例省", "country": "中国", "latitude": 30.0, "longitude": 120.0},
          102: {"id": 102, "name": "示例市", "admin1": "另一省", "country": "中国", "latitude": 31.0, "longitude": 121.0}}


class FakeSource:
    def __init__(self):
        self.calls = []
        self.fail = False
        self.code = 61

    async def search(self, name):
        self.calls.append(("search", name))
        if self.fail:
            raise OSError("offline")
        return [p for p in PLACES.values() if name in p["name"]]

    async def place(self, city_id):
        self.calls.append(("place", city_id))
        if self.fail:
            raise OSError("offline")
        return PLACES.get(city_id, {})

    async def forecast(self, latitude, longitude):
        self.calls.append(("forecast", latitude))
        if self.fail:
            raise TimeoutError
        return {"current": {"temperature_2m": 12.4, "weather_code": self.code},
                "daily": {"weather_code": [self.code, 2, 0, 71], "temperature_2m_max": [15.2, 16, 18, 3],
                          "temperature_2m_min": [3.4, 5, 7, -2], "precipitation_probability_max": [80, 20, 0, None]}}


class Clock:
    def __init__(self):
        self.now = 1_793_000_000.0

    def __call__(self):
        return self.now


async def test_find_lists_matching_cities():
    weather = Weather(FakeSource())
    found = await weather.find({"name": "示例"})
    assert found == {"cities": [{"id": 101, "name": "示例市", "area": "示例省 · 中国"},
                                {"id": 102, "name": "示例市", "area": "另一省 · 中国"}]}
    assert (await weather.find({"name": "没有"})) == {"cities": []}
    for bad in ({}, {"name": ""}, {"name": 3}, {"name": "示例", "lat": 1}):
        with pytest.raises(ProtocolError):
            await weather.find(bad)


async def test_get_compact_reply_and_cache():
    source, clock = FakeSource(), Clock()
    weather = Weather(source, clock)
    first = await weather.get({"id": 101})
    assert first == {"city": "示例市", "at": 1_793_000_000, "t": 12, "w": "小雨", "i": "rain", "hi": 15, "lo": 3, "p": 80,
                     "days": [["多云", "cloudsun", 16, 5, 20], ["晴", "sun", 18, 7, 0], ["小雪", "snow", 3, -2, -1]]}
    assert tokens(response(2147483647, first)) <= 110
    calls = len(source.calls)
    clock.now += CACHE_S - 1
    source.code = 0
    assert await weather.get({"id": 101}) == first, "within 30 minutes the cached answer"
    assert len(source.calls) == calls
    await weather.get({"id": 102})
    assert len(source.calls) == calls + 2, "the cache is per city"
    clock.now += 2
    fresh = await weather.get({"id": 101})
    assert fresh["w"] == "晴" and fresh["at"] == int(clock.now), "after 30 minutes it asks the source again"


async def test_source_errors_are_not_cached():
    source = FakeSource()
    weather = Weather(source, Clock())
    source.fail = True
    for call, args in ((weather.get, {"id": 101}), (weather.find, {"name": "示例"})):
        with pytest.raises(ProtocolError) as err:
            await call(args)
        assert err.value.code == -32603 and str(err.value) == "天气暂时取不到"
    source.fail = False
    assert (await weather.get({"id": 101}))["t"] == 12, "the next request tries again"
    with pytest.raises(ProtocolError) as err:
        await weather.get({"id": 999})
    assert err.value.code == -32602
    for bad in ({}, {"id": "101"}, {"id": 0}, {"id": 101, "lat": 30}):
        with pytest.raises(ProtocolError):
            await weather.get(bad)


async def test_device_asks_over_the_wire(make_hub):
    source = FakeSource()
    _, client, url = await make_hub(weather_source=source)
    device = FakeDevice(client, url)
    await device.connect()
    found = await device.call("weather/find", {"name": "示例"})
    assert [c["id"] for c in found["result"]["cities"]] == [101, 102]
    got = await device.call("weather/get", {"id": 101})
    assert got["result"]["w"] == "小雨" and len(got["result"]["days"]) == 3
    source.fail = True
    failed = await device.call("weather/get", {"id": 102})
    assert failed["error"]["code"] == -32603
    assert (await device.call("weather/get", {"id": 101}))["result"] == got["result"], "cached while the source is down"
