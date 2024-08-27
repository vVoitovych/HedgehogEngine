import subprocess

def update_submodules():
    try:
        subprocess.run(['git', 'submodule', 'init'], check=True)
        subprocess.run(['git', 'submodule', 'update', '--recursive', '--remote'], check=True)
        print("Submodules updated successfully.")
    except subprocess.CalledProcessError as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    update_submodules()


