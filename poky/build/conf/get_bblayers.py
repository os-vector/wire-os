import os, sys, re

def getLayerPriority(layerConfPath):
    try:
        with open(layerConfPath, "r") as confFile:
            for line in confFile:
                if line.strip().startswith("BBFILE_PRIORITY"):
                    match = re.search(r"BBFILE_PRIORITY\s*\S*\s*=\s*\"?(\d+)\"?", line)
                    if match:
                        return int(match.group(1))
    except Exception:
        pass
    return 0

def findLayersRecursively(basePath):
    layerList = []
    skipNames = {"meta-skeleton", "meta-selftest", "meta-yocto", "meta-yocto-bsp"}
    ignoreDirs = {"build", "bitbake", ".git", "scripts", "meta/lib", "templates", "tmp-glibc"}
    validLayerPattern = re.compile(r"^meta([-_].+)?$")

    for root, dirs, files in os.walk(basePath):
        dirs[:] = [d for d in dirs if d not in ignoreDirs]
        relparts = os.path.relpath(root, basePath).split(os.sep)
        if any(part in ignoreDirs for part in relparts):
            continue
        if not any(validLayerPattern.match(part) for part in relparts):
            continue

        if "layer.conf" in files:
            relpath = os.path.relpath(root, basePath)
            if any(skip in relpath for skip in skipNames):
                continue
            layerPath = os.path.abspath(os.path.join(root, "..")) if os.path.basename(root) == "conf" else os.path.abspath(root)
            layerList.append((layerPath, getLayerPriority(os.path.join(root, "layer.conf"))))

    return sorted(set(layerList), key=lambda layer: (-layer[1], layer[0]))


def generatePathString(pathList):
    return " ".join(path for path, _ in pathList)

if __name__ == "__main__":
    targetPath = sys.argv[1].strip("\"")
    print("# This configuration file is dynamically generated every time")
    print("# set_bb_env.sh is sourced to set up a workspace.  DO NOT EDIT.")
    print("#--------------------------------------------------------------")
    print("LCONF_VERSION = \"7\"")
    print()
    print("export WORKSPACE := \"${@os.path.abspath(os.path.join(os.path.dirname(d.getVar('FILE', d)),'../../..'))}\"")
    print()
    print("BBPATH = \"${TOPDIR}\"")
    print("BBFILES ?= \"\"")
    print("BBLAYERS = \"" + generatePathString(findLayersRecursively(targetPath)) + "\"")

