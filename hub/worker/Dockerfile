FROM python:3.11.15-slim-bookworm@sha256:d29f48a31a8b408ed19272ca1e7b10ebae13b240a27e862d3d4217c528e2e0c3
COPY main.py /worker/main.py
USER 10001:10001
CMD ["python", "/worker/main.py", "--input", "/work/input", "--output", "/work/output"]
