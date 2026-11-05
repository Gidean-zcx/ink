FROM python:3.11.15-slim-bookworm@sha256:d29f48a31a8b408ed19272ca1e7b10ebae13b240a27e862d3d4217c528e2e0c3
WORKDIR /app
COPY requirements.lock .
RUN python -m pip install --no-cache-dir --require-hashes -r requirements.lock
COPY ink_hub ink_hub
USER 10001:10001
ENV PYTHONUNBUFFERED=1
CMD ["python", "-m", "ink_hub", "serve", "--data", "/data"]
