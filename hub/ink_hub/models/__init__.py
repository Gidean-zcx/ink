"""Environment-selected providers; no credentials are stored on disk."""
import os


def from_environment():
    provider = os.environ.get("INK_HUB_MODEL_PROVIDER", "fake")
    if provider == "fake":
        return None
    if provider not in ("openai", "anthropic"):
        raise RuntimeError("模型接法无效")
    prefix = "INK_HUB_" + provider.upper()
    if not os.environ.get(prefix + "_API_KEY"):
        return None
    from .remote import RemoteModel
    return RemoteModel(provider, prefix)
