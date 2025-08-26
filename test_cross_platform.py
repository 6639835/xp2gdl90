#!/usr/bin/env python3
"""
XP2GDL90 Plugin Cross-Platform Test Script
Validates the compilation configuration correctness for Windows, macOS, and Linux platforms

Usage:
    python test_cross_platform.py
"""

import os
import sys
import shutil
import subprocess
import json
from pathlib import Path

def run_command(command, cwd=None, capture_output=True):
    """Execute command and return result"""
    try:
        result = subprocess.run(
            command, 
            shell=True, 
            cwd=cwd,
            capture_output=capture_output,
            text=True
        )
        return {
            'success': result.returncode == 0,
            'returncode': result.returncode,
            'stdout': result.stdout if capture_output else '',
            'stderr': result.stderr if capture_output else ''
        }
    except Exception as e:
        return {
            'success': False,
            'returncode': -1,
            'stdout': '',
            'stderr': str(e)
        }

def test_file_existence():
    """Test if required files exist"""
    print("[CHECK] Testing file existence...")
    
    required_files = [
        'CMakeLists.txt',
        'mac_exports.txt',
        'win_exports.def',
        'linux_exports.txt',
        'build.sh',
        'build_windows.bat',
        'build_linux.sh',
        'build_all.py',
        'SDK/Libraries/Mac/XPLM.framework',
        'SDK/Libraries/Win/XPLM_64.lib',
        'SDK/Libraries/Win/XPWidgets_64.lib',
        'SDK/Libraries/Lin/XPLM_64.so',
        'SDK/Libraries/Lin/XPWidgets_64.so',
        'src/xp2gdl90.cpp'
    ]
    
    missing_files = []
    for file_path in required_files:
        if not Path(file_path).exists():
            missing_files.append(file_path)
    
    if missing_files:
        print(f"[ERROR] Missing files: {missing_files}")
        return False
    else:
        print("[SUCCESS] All required files exist")
        return True

def test_symbol_export_files():
    """Test symbol export file formats"""
    print("\n[CHECK] Testing symbol export files...")
    
    tests = []
    
    # Test Mac export file
    with open('mac_exports.txt', 'r') as f:
        mac_exports = f.read()
        if all(symbol in mac_exports for symbol in ['XPluginStart', 'XPluginStop', 'XPluginEnable', 'XPluginDisable', 'XPluginReceiveMessage']):
            tests.append(("Mac exports", True))
        else:
            tests.append(("Mac exports", False))
    
    # Test Windows export file
    with open('win_exports.def', 'r') as f:
        win_exports = f.read()
        if 'EXPORTS' in win_exports and all(symbol in win_exports for symbol in ['XPluginStart', 'XPluginStop']):
            tests.append(("Windows exports", True))
        else:
            tests.append(("Windows exports", False))
    
    # Test Linux export file
    with open('linux_exports.txt', 'r') as f:
        linux_exports = f.read()
        if 'global:' in linux_exports and 'local:' in linux_exports and 'XPluginStart;' in linux_exports:
            tests.append(("Linux exports", True))
        else:
            tests.append(("Linux exports", False))
    
    all_passed = True
    for test_name, passed in tests:
        if passed:
            print(f"[SUCCESS] {test_name}: Format correct")
        else:
            print(f"[ERROR] {test_name}: Format incorrect")
            all_passed = False
    
    return all_passed

def test_cmake_configuration(platform):
    """Test platform-specific CMake configuration"""
    print(f"\n[CHECK] Testing {platform} platform CMake configuration...")
    
    test_dir = f"test_builds/{platform}"
    if Path(test_dir).exists():
        shutil.rmtree(test_dir)
    Path(test_dir).mkdir(parents=True)
    
    # Set platform-specific CMake parameters
    if platform == 'linux':
        cmake_cmd = f"cmake -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_BUILD_TYPE=Release ../.."
    elif platform == 'windows':
        cmake_cmd = f"cmake -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_BUILD_TYPE=Release ../.."
    else:  # mac
        cmake_cmd = f"cmake -DCMAKE_BUILD_TYPE=Release ../.."
    
    result = run_command(cmake_cmd, cwd=test_dir)
    
    if not result['success']:
        print(f"[ERROR] {platform}: CMake configuration failed")
        print(f"Error: {result['stderr']}")
        return False
    
    # Check generated files
    build_files = list(Path(test_dir).glob('*'))
    if len(build_files) < 3:  # Should have at least Makefile, CMakeCache.txt, etc
        print(f"[ERROR] {platform}: Too few build files generated")
        return False
    
    # Check specific configuration items
    expected_output = {
        'linux': 'lin_x64.xpl',
        'windows': 'win_x64.xpl',
        'mac': 'mac_x64.xpl'
    }
    
    expected_libs = {
        'linux': ['XPLM_64.so', 'XPWidgets_64.so'],
        'windows': ['XPLM_64.lib', 'XPWidgets_64.lib'],
        'mac': ['XPLM.framework', 'XPWidgets.framework']
    }
    
    # Search for key information in configuration files
    config_found = False
    for file_path in Path(test_dir).rglob('*'):
        if file_path.is_file():
            try:
                content = file_path.read_text()
                if expected_output[platform] in content:
                    config_found = True
                    break
            except:
                continue
    
    if config_found:
        print(f"[SUCCESS] {platform}: CMake configuration correct, output file set to {expected_output[platform]}")
        return True
    else:
        print(f"[ERROR] {platform}: Expected output file configuration not found")
        return False

def test_build_scripts():
    """Test build script syntax"""
    print("\n[CHECK] Testing build script syntax...")
    
    tests = []
    
    # Test bash script syntax
    for script in ['build.sh', 'build_linux.sh']:
        result = run_command(f"bash -n {script}")
        tests.append((script, result['success']))
    
    # Test Python script syntax
    result = run_command("python3 -m py_compile build_all.py")
    tests.append(("build_all.py", result['success']))
    
    # Test Python script help function
    result = run_command("python3 build_all.py --help")
    tests.append(("build_all.py --help", result['success']))
    
    all_passed = True
    for script_name, passed in tests:
        if passed:
            print(f"[SUCCESS] {script_name}: Syntax correct")
        else:
            print(f"[ERROR] {script_name}: Syntax error")
            all_passed = False
    
    return all_passed

def test_library_files():
    """Test SDK library files"""
    print("\n[CHECK] Testing SDK library files...")
    
    lib_tests = [
        ('Win/XPLM_64.lib', 'current ar archive'),
        ('Win/XPWidgets_64.lib', 'current ar archive'),
        ('Lin/XPLM_64.so', 'ELF 64-bit LSB shared object'),
        ('Mac/XPLM.framework/XPLM', 'Mach-O')
    ]
    
    all_passed = True
    for lib_path, expected_type in lib_tests:
        full_path = f"SDK/Libraries/{lib_path}"
        if Path(full_path).exists():
            result = run_command(f"file '{full_path}'")
            if result['success'] and expected_type in result['stdout']:
                print(f"[SUCCESS] {lib_path}: Format correct ({expected_type})")
            else:
                print(f"[ERROR] {lib_path}: Format incorrect or cannot detect")
                all_passed = False
        else:
            print(f"[ERROR] {lib_path}: File does not exist")
            all_passed = False
    
    return all_passed

def main():
    print("[TEST] XP2GDL90 Plugin Cross-Platform Compilation Configuration Test")
    print("=" * 70)
    
    # Check if in project root directory
    if not Path('CMakeLists.txt').exists():
        print("[ERROR] Please run this script in the project root directory")
        return 1
    
    # Run all tests
    tests = [
        ("File Existence", test_file_existence),
        ("Symbol Export Files", test_symbol_export_files),
        ("Build Script Syntax", test_build_scripts),
        ("SDK Library Files", test_library_files),
    ]
    
    # CMake configuration tests
    cmake_tests = [
        ("macOS CMake Configuration", lambda: test_cmake_configuration('mac')),
        ("Linux CMake Configuration", lambda: test_cmake_configuration('linux')),
        ("Windows CMake Configuration", lambda: test_cmake_configuration('windows')),
    ]
    
    all_tests = tests + cmake_tests
    passed_tests = 0
    total_tests = len(all_tests)
    
    for test_name, test_func in all_tests:
        try:
            if test_func():
                passed_tests += 1
            else:
                print(f"\n[WARNING] {test_name} test failed")
        except Exception as e:
            print(f"\n[ERROR] {test_name} test error: {e}")
    
    # Cleanup test files
    if Path('test_builds').exists():
        shutil.rmtree('test_builds')
    
    print(f"\n" + "=" * 70)
    print(f"[RESULT] Test Results: {passed_tests}/{total_tests} passed")
    
    if passed_tests == total_tests:
        print("[SUCCESS] All tests passed! Cross-platform compilation configuration is correct.")
        print("\n[INFO] Next steps:")
        print("   • On Windows: run build_windows.bat")
        print("   • On macOS: run ./build.sh")
        print("   • On Linux: run ./build_linux.sh")
        print("   • Or use unified script: python3 build_all.py")
        print("\n[PLUGIN INFO] XP2GDL90 Plugin Features:")
        print("   • Reads flight data directly from X-Plane SDK")
        print("   • Broadcasts GDL-90 data to FDPRO on port 4000")
        print("   • Supports ownship position and traffic targets")
        print("   • No X-Plane Data Output configuration required")
        return 0
    else:
        print(f"[WARNING] {total_tests - passed_tests} tests failed, please check configuration.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
