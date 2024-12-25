import os
import subprocess

def main():
    os.chdir("..")
    premake_exe = os.path.join("Vendor", "Binaries", "Premake", "Windows", "premake5.exe")

    subprocess.run([premake_exe, "--file=Build.lua", "vs2022"])

if __name__ == "__main__":
    main()
