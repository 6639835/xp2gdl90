#!/usr/bin/env python3
"""
XP2GDL90 Plugin Cross-Platform Build Script
Supports automatic detection and building for Windows, macOS, and Linux platforms

Usage:
    python build_all.py           # Build current platform
    python build_all.py clean     # Clean and build current platform
    python build_all.py --help    # Show help information
"""

import os
import sys
import platform
import subprocess
import argparse
from pathlib import Path

def get_platform():
    """Detect current platform"""
    system = platform.system().lower()
    if system == 'windows':
        return 'windows'
    elif system == 'darwin':
        return 'mac'
    elif system == 'linux':
        return 'linux'
    else:
        raise RuntimeError(f"Unsupported platform: {system}")

def run_command(command, shell=False):
    """Execute command and handle output"""
    try:
        print(f"Executing command: {' '.join(command) if isinstance(command, list) else command}")
        result = subprocess.run(command, shell=shell, check=True, 
                              capture_output=False, text=True)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"Command failed with exit code {e.returncode}: {e}")
        return False
    except FileNotFoundError as e:
        print(f"Command not found: {e}")
        return False
    except Exception as e:
        print(f"Unexpected error executing command: {e}")
        return False

def build_windows(clean=False):
    """Build Windows version"""
    script_path = Path(__file__).parent / "build_windows.bat"
    if not script_path.exists():
        print("Error: Cannot find build_windows.bat script")
        return False
    
    args = [str(script_path)]
    if clean:
        args.append("clean")
    
    return run_command(args, shell=True)

def build_mac(clean=False):
    """Build macOS version"""
    script_path = Path(__file__).parent / "build.sh"
    if not script_path.exists():
        print("Error: Cannot find build.sh script")
        return False
    
    # Ensure script has execute permission
    os.chmod(script_path, 0o755)
    
    args = [str(script_path)]
    if clean:
        args.append("clean")
    
    return run_command(args)

def build_linux(clean=False):
    """Build Linux version"""
    script_path = Path(__file__).parent / "build_linux.sh"
    if not script_path.exists():
        print("Error: Cannot find build_linux.sh script")
        return False
    
    # Ensure script has execute permission
    os.chmod(script_path, 0o755)
    
    args = [str(script_path)]
    if clean:
        args.append("clean")
    
    return run_command(args)

def main():
    parser = argparse.ArgumentParser(
        description='XP2GDL90 Plugin Cross-Platform Build Script',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    python build_all.py           # Build current platform
    python build_all.py clean     # Clean and build current platform
    python build_all.py --platform windows  # Build specified platform
        '''
    )
    
    parser.add_argument('action', nargs='?', default='build',
                       choices=['build', 'clean'],
                       help='Operation type: build (default) or clean')
    
    parser.add_argument('--platform', 
                       choices=['windows', 'mac', 'linux', 'auto'],
                       default='auto',
                       help='Target platform (default: auto - auto detect)')
    
    args = parser.parse_args()
    
    # Detect platform
    if args.platform == 'auto':
        try:
            target_platform = get_platform()
        except RuntimeError as e:
            print(f"Platform detection failed: {e}")
            return 1
    else:
        target_platform = args.platform
    
    print(f"=== XP2GDL90 Plugin Build Script ===")
    print(f"Target platform: {target_platform}")
    print(f"Operation type: {args.action}")
    print()
    
    # Check project environment
    project_dir = Path(__file__).parent
    if not (project_dir / "CMakeLists.txt").exists():
        print("Error: Current directory is not a valid project directory (CMakeLists.txt not found)")
        return 1
    
    if not (project_dir / "SDK").exists():
        print("Error: SDK directory not found")
        return 1
    
    # Execute build
    clean = (args.action == 'clean')
    
    if target_platform == 'windows':
        success = build_windows(clean)
        expected_output = "win.xpl"
    elif target_platform == 'mac':
        success = build_mac(clean)
        expected_output = "mac.xpl"
    elif target_platform == 'linux':
        success = build_linux(clean)
        expected_output = "lin.xpl"
    else:
        print(f"Error: Unsupported platform {target_platform}")
        return 1
    
    if success:
        # Check output file
        output_file = project_dir / "build" / expected_output
        if output_file.exists():
            file_size = output_file.stat().st_size
            print(f"\n[SUCCESS] Build successful!")
            print(f"Output file: {output_file}")
            print(f"File size: {file_size / 1024:.1f} KB")
            print(f"Build date: {output_file.stat().st_mtime}")
            print(f"\n[INSTALLATION] Copy {expected_output} to:")
            print(f"  X-Plane/Resources/plugins/xp2gdl90/{expected_output}")
            print(f"  No additional files needed - plugin is self-contained")
            
            # Basic sanity check - plugin should be at least 10KB
            if file_size < 10240:
                print(f"\n[WARNING] Plugin file is unusually small ({file_size} bytes)")
                print("This might indicate a build issue.")
        else:
            print(f"\n[WARNING] Build seems successful, but output file not found: {expected_output}")
            print("Expected location: build/" + expected_output)
            # List actual build directory contents
            build_dir = project_dir / "build"
            if build_dir.exists():
                print("Build directory contents:")
                for item in build_dir.iterdir():
                    print(f"  {item.name}")
            return 1
    else:
        print(f"\n[ERROR] Build failed for {target_platform} platform")
        print("Check the build output above for specific error messages.")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
