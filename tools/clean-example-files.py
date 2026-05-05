#!/usr/bin/env python3
"""Clean example directories using per-example meta.json files."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, Iterable, List, Set


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1]


def read_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def load_string_list(entry: Dict[str, object], key: str) -> List[str]:
    value = entry.get(key, [])
    if not isinstance(value, list):
        raise ValueError("{0} must be an array in {1}".format(
            key,
            entry.get("name", "<unknown>")
        ))
    return [str(item).strip() for item in value if str(item).strip()]


def tracked_files(entry: Dict[str, object]) -> Set[str]:
    files: Set[str] = {"README.md", "meta.json"}
    for key in ("inputs", "outputs", "doc_images"):
        files.update(load_string_list(entry, key))

    preview = str(entry.get("preview", "")).strip()
    if preview:
        files.add(preview)
    return files


def load_examples(examples_dir: Path) -> List[Dict[str, object]]:
    examples: List[Dict[str, object]] = []
    for meta_path in sorted(examples_dir.glob("*/meta.json")):
        raw = read_json(meta_path)
        if not isinstance(raw, dict):
            raise ValueError("{0} must contain an object".format(meta_path))
        entry = dict(raw)
        entry["name"] = meta_path.parent.name
        entry["dir"] = meta_path.parent
        entry["meta_path"] = meta_path
        examples.append(entry)
    return examples


def iter_removable_files(example_dir: Path, keep: Set[str]) -> Iterable[Path]:
    for path in sorted(example_dir.iterdir()):
        if not path.is_file():
            continue
        if path.name in keep:
            continue
        yield path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Clean example temp files using per-example meta.json."
    )
    parser.add_argument(
        "--filter",
        help="only clean example directories whose name contains this text",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="delete files instead of printing what would be removed",
    )
    args = parser.parse_args()

    repo_root = repo_root_from_script()
    examples_dir = repo_root / "share" / "examples"

    for entry in load_examples(examples_dir):
        name = str(entry["name"])
        if args.filter and args.filter not in name:
            continue

        keep = tracked_files(entry)
        example_dir = Path(entry["dir"])
        removals = list(iter_removable_files(example_dir, keep))
        if not removals:
            continue

        print("[{0}]".format(name))
        for path in removals:
            print(path.name)
            if args.apply:
                path.unlink()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
