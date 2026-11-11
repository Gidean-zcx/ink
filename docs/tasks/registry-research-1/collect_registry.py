#!/usr/bin/env python3
"""Read public registry metadata only; never request firmware files or credentials."""
import argparse
import datetime
import hashlib
import json
import re
import urllib.parse
import urllib.request

BASE = "https://admin.metalio.world/xiaozhi/api/firmwares"


def read(params):
    url = BASE + "?" + urllib.parse.urlencode(params)
    request = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(request, timeout=40) as response:
        raw = response.read(5 * 1024 * 1024 + 1)
        status = response.status
    if len(raw) > 5 * 1024 * 1024:
        raise ValueError("metadata response exceeds 5 MiB")
    result = json.loads(raw)
    if result.get("code") != 0:
        raise ValueError("public metadata request failed: " + str(result.get("code")))
    return result["data"], {
        "url": url, "http": status, "code": result["code"],
        "sha256": hashlib.sha256(raw).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    requests, records = [], []
    page = 1
    while True:
        data, evidence = read({"page": page, "limit": 100, "sourceType": "all"})
        evidence.update(total=data["total"], count=len(data["list"]))
        requests.append(evidence)
        records.extend(data["list"])
        if not data["list"]:
            break
        page += 1
    if len(records) != requests[0]["total"]:
        raise ValueError("registry changed during collection; inspect before accepting")
    ids = {row["id"] for row in records}
    if len(ids) != len(records):
        raise ValueError("duplicate firmware IDs")
    boards = {row["hardwareModelId"]: row["hardwareModelName"] for row in records}
    matrix = []
    for board, name in sorted(boards.items()):
        for channel in ("featured", "official", "community", "all"):
            params = {"page": 1, "limit": 100, "hardwareModelId": board,
                      "sourceType": "all" if channel == "featured" else channel}
            if channel == "featured":
                params["featured"] = "true"
            data, evidence = read(params)
            expected = {r["id"] for r in records if r["hardwareModelId"] == board
                        and (channel == "all" or
                             (channel == "featured" and r.get("featured")) or
                             r["sourceType"] == channel)}
            actual = {r["id"] for r in data["list"]}
            if actual != expected or data["total"] != len(expected):
                raise ValueError("filter mismatch: " + board + "/" + channel)
            evidence.update(total=data["total"], count=len(data["list"]))
            requests.append(evidence)
            matrix.append({"board_id": board, "board": name, "filter": channel,
                           "total": data["total"], "ids": sorted(actual)})
    clean = []
    for row in records:
        author = row.get("authorName", "")
        if author.isdigit():
            author = "数字昵称（已隐去）"
        versions = [{k: v.get(k) for k in ("version", "versionStatus", "createDate", "updateDate")}
                    for v in row.get("versions", [])]
        texts = [row.get("githubUrl") or "", row.get("giteeUrl") or "",
                 row.get("description") or ""]
        texts += [(v.get("remark") or "") + " " + (v.get("githubUrl") or "")
                  for v in row.get("versions", [])]
        # Keep source repository URLs only; drop asset, firmware, profile and signed URLs.
        repos = sorted(set(re.findall(
            r"https://(?:github|gitee)\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+",
            " ".join(texts))))
        clean.append({"id": row["id"], "name": row["firmwareName"], "author": author,
                      "board_id": row["hardwareModelId"], "board": row["hardwareModelName"],
                      "source": row["sourceType"], "featured": row.get("featured", False),
                      "updated": row.get("updateDate"), "version_total": row["versionTotal"],
                      "versions": versions, "source_repositories": repos})
    result = {"retrieved_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "scope": "Public unauthenticated GET metadata; no firmware downloads",
              "total": len(clean), "requests": requests, "filters": matrix, "firmwares": clean}
    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(f"Registry: {len(clean)} firmwares, {len(boards)} board IDs, "
          f"{len(matrix)} filters verified, {sum(len(r['versions']) for r in clean)} versions")


if __name__ == "__main__":
    main()
