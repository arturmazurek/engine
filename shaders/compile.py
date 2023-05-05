
import subprocess

if __name__ == "__main__":
    subprocess.call(["glslc.exe", "shader.vert", "-o", "shader.vert.spv"])
    subprocess.call(["glslc.exe", "shader.frag", "-o", "shader.frag.spv"])

    print("Press Enter to continue")
    input()