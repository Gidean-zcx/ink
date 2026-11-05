FROM python:3.11.15-slim-bookworm
WORKDIR /app
COPY requirements.lock .
RUN python -m pip install --no-cache-dir --require-hashes -r requirements.lock
COPY ink_hub ink_hub
USER 10001:10001
ENV PYTHONUNBUFFERED=1
CMD ["python", "-m", "ink_hub", "serve", "--data", "/data"]
