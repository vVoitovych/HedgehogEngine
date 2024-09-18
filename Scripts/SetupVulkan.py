import os
import urllib.request
import subprocess
import platform

def is_vulkan_sdk_installed():
    return 'VULKAN_SDK' in os.environ

def download_vulkan_sdk(installer_url, save_path):
    print("Downloading Vulkan SDK...")
    command = ["curl", "-O", installer_url]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    print("Output:", result.stdout)
    print("Error:", result.stderr)
    print(f"Vulkan SDK downloaded to {save_path}")

def install_vulkan_sdk(installer_path):
    print("Installing Vulkan SDK...")
    os.startfile(os.path.abspath(installer_path))

def main():
    if is_vulkan_sdk_installed():
        print("Vulkan SDK is already installed.")
    else:
        print("Vulkan SDK not found.")
        
        installer_url = "https://sdk.lunarg.com/sdk/download/1.3.231.1/windows/VulkanSDK-1.3.231.1-Installer.exe"
        save_path = "VulkanSDK-1.3.231.1-Installer.exe"
        
        download_vulkan_sdk(installer_url, save_path)
        install_vulkan_sdk(save_path)
        
if __name__ == "__main__":
    main()
