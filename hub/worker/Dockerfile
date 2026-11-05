FROM python:3.11.15-slim-bookworm
COPY main.py /worker/main.py
USER 10001:10001
CMD ["python", "/worker/main.py", "--input", "/work/input", "--output", "/work/output"]
