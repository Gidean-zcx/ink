"""Small fail-closed subset used by the step 1 capability descriptors."""
from ink_hub.gateway.wire import ProtocolError


def validate(value, schema):
    if not isinstance(schema, dict) or "$ref" in schema:
        raise ProtocolError("不支持这个参数定义")
    supported = {"type", "properties", "required", "additionalProperties", "enum", "minLength", "maxLength",
                 "minimum", "maximum", "items", "maxItems", "title", "description", "default"}
    if set(schema) - supported or ("additionalProperties" in schema and type(schema["additionalProperties"]) is not bool):
        raise ProtocolError("参数定义超出本步支持范围")
    kind = schema.get("type")
    types = {"object": lambda v: isinstance(v, dict), "array": lambda v: isinstance(v, list),
             "string": lambda v: isinstance(v, str), "integer": lambda v: type(v) is int,
             "number": lambda v: type(v) in (int, float), "boolean": lambda v: type(v) is bool,
             "null": lambda v: v is None}
    if kind and (kind not in types or not types[kind](value)):
        raise ProtocolError("参数类型不符合定义")
    if "enum" in schema and value not in schema["enum"]:
        raise ProtocolError("参数不在允许范围")
    if isinstance(value, dict):
        properties = schema.get("properties", {})
        if set(schema.get("required", [])) - set(value):
            raise ProtocolError("缺少必要参数")
        if schema.get("additionalProperties") is False and set(value) - set(properties):
            raise ProtocolError("包含未定义参数")
        for key, item in value.items():
            if key in properties:
                validate(item, properties[key])
    if isinstance(value, str) and not schema.get("minLength", 0) <= len(value) <= schema.get("maxLength", float("inf")):
        raise ProtocolError("文字长度超出定义")
    if type(value) in (int, float) and not schema.get("minimum", -float("inf")) <= value <= schema.get("maximum", float("inf")):
        raise ProtocolError("数字超出定义")
    if isinstance(value, list):
        if len(value) > schema.get("maxItems", float("inf")):
            raise ProtocolError("列表超出定义")
        for item in value:
            validate(item, schema.get("items", {}))
