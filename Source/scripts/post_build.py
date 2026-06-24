import argparse
import shutil
from pathlib import Path

def find_and_copy_dlls(source_dir: Path, dest_dir: Path) -> None:
    dlls = list(source_dir.rglob("*.dll"))

    if not dlls:
        print(f"DLL'ки не найдены в {source_dir}")
        return

    dest_dir.mkdir(parents=True, exist_ok=True)

    copied = 0
    skipped = 0

    for dll in dlls:
        dest = dest_dir / dll.name

        if dest.exists():
            if dll.stat().st_mtime <= dest.stat().st_mtime:
                print(f"  [skip]  {dll.name}")
                skipped += 1
                continue

        shutil.copy2(dll, dest)
        print(f"  [copy]  {dll.relative_to(source_dir)}  ->  {dest}")
        copied += 1

    print(f"\nDone: copied {copied}, skipped {skipped}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Рекурсивно копирует DLL из source в dest")
    parser.add_argument("--source", required=True, help="Папка откуда искать DLL")
    parser.add_argument("--dest",   required=True, help="Папка куда копировать")
    args = parser.parse_args()

    source = Path(args.source)
    dest   = Path(args.dest)

    if not source.exists():
        print(f"[ERROR] source не существует: {source}")
        exit(1)

    find_and_copy_dlls(source, dest)