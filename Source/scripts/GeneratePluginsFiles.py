import argparse
import shutil
import re
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

    if not destBinary.exists():
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

def get_project_name(text: str) -> str | None:
    # собираем все set(VAR "value") и set(VAR value) в словарь
    variables = {}
    for m in re.finditer(r'set\(\s*(\w+)\s+"?([^")\s]+)"?\s*\)', text):
        variables[m.group(1)] = m.group(2)

    # ищем сам project(...)
    m = re.search(r'project\(\s*"?([^"\s)]+)"?', text)
    if not m:
        return None

    raw = m.group(1)

    # если это ${VAR} — резолвим через словарь
    var_match = re.match(r'\$\{(\w+)\}', raw)
    if var_match:
        return variables.get(var_match.group(1))

    # иначе это уже готовое имя (строка или голый токен)
    return raw

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

    if not source.exists():
        print(f"[ERROR] source do not exists: {source}")
        exit(1)

    plugins = []

    for p in source.rglob("CMakeLists.txt"):
        if p.is_file():
            symbols = p.read_text()
            modName = get_project_name(symbols)

            if modName != "" and modName != "SoundLink":
                plugins.append(modName)
                print("Found plugin: " + str(modName))

    if plugins.__len__() != 0:
        print("Found " + str(plugins.__len__()) + " plugins. Start copying plugins")

        for plugin in plugins:
            print(f"  [copy] {plugin}  to  ->  {dest}")
            FindAndCopyPluginToDest(dest=dest, source=source, binary=binary, pluginName=plugin)

        print("Copying done")