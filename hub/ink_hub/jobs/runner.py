import asyncio
import hashlib


async def example_hash(scope, payload):
    if payload != {"text": "synthetic example"}:
        raise ValueError("example jobs only")
    return {"sha256": hashlib.sha256(payload["text"].encode()).hexdigest()}


class Runner:
    def __init__(self, queue):
        self.queue = queue
        self.wake = asyncio.Event()
        self.changed = asyncio.Condition()

    async def submit(self, scope, key, kind, payload):
        jid = await asyncio.to_thread(self.queue.enqueue, scope, key, kind, payload)
        self.wake.set()
        return jid

    async def wait(self, scope, jid):
        async with self.changed:
            while True:
                row = await asyncio.to_thread(self.queue.get, scope, jid)
                if not row or row["state"] in ("done", "unknown", "cancelled"):
                    return row
                await self.changed.wait()

    async def run(self):
        while True:
            self.wake.clear()
            await asyncio.to_thread(self.queue.recover)
            async with self.changed:
                self.changed.notify_all()
            while await self.queue.run_one({"example.hash": example_hash}):
                async with self.changed:
                    self.changed.notify_all()
            # Lease recovery timer, also interruptible by a newly submitted job.
            timer = asyncio.get_running_loop().call_later(1, self.wake.set)
            try:
                await self.wake.wait()
            finally:
                timer.cancel()
