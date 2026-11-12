"""More than 128 capabilities: the current app's own and the everyday system ones must survive the cut."""
from ink_hub.voice.flow import TOOL_LIMIT, rank_tools


def catalogue():
    names = [f"filler{i}.op" for i in range(200)] + ["settings.set", "notify.list", "reader.next_page", "reader.toc"]
    return {name: {"title": name, "inputSchema": {"type": "object"}} for name in names}


def test_page_app_and_core_domains_survive():
    tools = catalogue()
    kept = rank_tools(tools, {"app": "reader", "actions": list(tools)})
    assert len(kept) == TOOL_LIMIT
    assert {"reader.next_page", "reader.toc", "settings.set", "notify.list"} <= set(kept)


def test_order_is_device_order_within_a_rank():
    tools = catalogue()
    kept = list(rank_tools(tools, {"app": "", "actions": list(tools)}))
    assert kept[:2] == ["settings.set", "notify.list"] and kept[2] == "filler0.op"


def test_capabilities_named_on_the_page_come_first():
    tools = catalogue()
    snapshot = {"app": "book", "text": "第一章 正文…… 动作：reader.next_page；reader.toc；", "actions": list(tools)}
    kept = list(rank_tools(tools, snapshot))
    assert kept[:2] == ["reader.next_page", "reader.toc"]


def test_reader_app_registry_id_maps_to_its_capabilities():
    tools = catalogue()
    kept = rank_tools(tools, {"app": "book", "text": "正文，没有动作段", "actions": list(tools)})
    assert {"reader.next_page", "reader.toc"} <= set(kept)
