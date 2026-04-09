import argparse
import shutil
from pathlib import Path

def FindAndCopyPluginToDest(source: Path, dest: Path, binary: Path, pluginName: str) -> None:

    dllsDir = [p for p in binary.rglob("*" + pluginName) if "Plugins" not in p.parts]

    if not dllsDir:
        print(f"Fail to find {pluginName} directory")
        return

    dlls = list(dllsDir[0].glob("*.dll"))

    print(dllsDir[0])

    if not dlls:
        print(f"Fail to find {pluginName} dlls")
        return

    destPath = dest / pluginName
    destBinary = destPath / "Binaries"

    destBinary.mkdir(parents=True, exist_ok=True)

    for dll in dlls:
        destDir = destPath / "Binaries" / dll.name

        print(f"  [copy] {dll}  to  ->  {destDir}")

        if destDir.exists():
            if dll.stat().st_mtime <= destDir.stat().st_mtime:
                print(f"  [skip]  {dll.name}")
                continue

        shutil.copy2(dll, destDir)

    print(f"\nGeneration done")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generating plugins files")
    parser.add_argument("--destDir", required=True)
    parser.add_argument("--sourceDir", required=True)
    parser.add_argument("--binaryDir", required=True)
    parser.add_argument("--plugins", nargs="+", required=True)
    args = parser.parse_args()

    source = Path(args.sourceDir)
    dest   = Path(args.destDir)
    binary = Path(args.binaryDir)
    plugins = args.plugins

    if not source.exists():
        print(f"[ERROR] source do not exists: {source}")
        exit(1)

    for plugin in plugins:
        FindAndCopyPluginToDest(dest=dest, source=source, binary=binary, pluginName=plugin)

