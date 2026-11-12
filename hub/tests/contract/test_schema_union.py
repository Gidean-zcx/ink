"""settings.set exports value.type as a list of types; validation must accept any listed type."""
import pytest

from ink_hub.bus.schema import validate
from ink_hub.gateway.wire import ProtocolError

SETTINGS_SET = {"type": "object", "required": ["id", "value"], "additionalProperties": False,
                "properties": {"id": {"type": "string"}, "value": {"type": ["integer", "boolean", "string"]}}}


@pytest.mark.parametrize("value", [30, True, "warm"])
def test_union_accepts_each_listed_type(value):
    validate({"id": "display.light", "value": value}, SETTINGS_SET)


@pytest.mark.parametrize("value", [1.5, None, [1], {"a": 1}])
def test_union_rejects_unlisted_types(value):
    with pytest.raises(ProtocolError):
        validate({"id": "display.light", "value": value}, SETTINGS_SET)


def test_unknown_type_name_fails_closed():
    with pytest.raises(ProtocolError):
        validate(1, {"type": ["integer", "decimal"]})
