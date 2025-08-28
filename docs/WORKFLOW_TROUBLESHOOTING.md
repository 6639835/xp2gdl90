# Workflow Troubleshooting Guide

This document provides solutions for common workflow issues in the XP2GDL90 project.

## Table of Contents

- [CI/CD Pipeline Issues](#cicd-pipeline-issues)
- [Performance Testing Issues](#performance-testing-issues)
- [Documentation Generation Issues](#documentation-generation-issues)  
- [Security Scanning Issues](#security-scanning-issues)
- [Local Development Issues](#local-development-issues)

## CI/CD Pipeline Issues

### GitHub Actions Permission Denied

**Problem**: `Resource not accessible by integration` error when creating issues

**Solution**: Ensure the workflow has the correct permissions:

```yaml
permissions:
  actions: read
  contents: read
  security-events: write
  pull-requests: write
  issues: write  # Add this line
```

### Build Failures on Different Platforms

**Problem**: Platform-specific build failures

**Solution**: Use conditional dependencies:

```yaml
- name: Install dependencies (macOS)
  if: runner.os == 'macOS'
  run: |
    # Handle cmake conflicts gracefully
    if brew list cmake &>/dev/null; then
      echo "CMake already installed, skipping..."
    else
      brew install cmake
    fi
    brew install google-benchmark
```

## Performance Testing Issues

### Benchmark Build Failures - Missing Mock Library

**Problem**: `cannot find -lxplane_mock: No such file or directory`

**Root Cause**: Benchmarks trying to link against mock library that's only built for tests

**Solution**: Remove mock dependencies from benchmarks:

```cmake
# In benchmarks/CMakeLists.txt
# Link against Google Benchmark only (remove xplane_mock)
target_link_libraries(xp2gdl90_benchmarks 
    benchmark::benchmark
    benchmark::benchmark_main
)
```

**Fix Benchmark Code**: Make benchmarks self-contained:

```cpp
// Replace XPLMMock dependencies with internal mock classes
class MockDataStore {
    std::map<std::string, double> doubleValues;
    std::map<std::string, float> floatValues;
public:
    double getDouble(const std::string& name) { return doubleValues[name]; }
    float getFloat(const std::string& name) { return floatValues[name]; }
};
```

### Compiler Warnings in Benchmarks

**Problem**: Narrowing conversion warnings

**Solution**: Add explicit casts:

```cpp
// Fix narrowing conversions
150.0f + static_cast<float>(sin(t * 4 * M_PI) * 50.0)  // Instead of sin() * 50.0f

// Add missing includes
#include <cmath>  // For M_PI, sin, cos functions
```

### Missing Benchmark Results

**Problem**: Benchmark artifacts not found in analysis step

**Solution**: Add error handling in workflow:

```yaml
- name: Download all benchmark results
  uses: actions/download-artifact@v4
  with:
    path: benchmark-artifacts/
  continue-on-error: true

- name: Check for benchmark artifacts  
  run: |
    if [ ! -d "benchmark-artifacts" ]; then
      echo "Creating benchmark-artifacts directory (no artifacts found)"
      mkdir -p benchmark-artifacts
    fi
```

## Documentation Generation Issues

### Doxygen Configuration Warnings

**Problem**: Obsolete Doxygen tags causing warnings

**Root Cause**: Doxygen configuration contains deprecated settings

**Solution**: Remove obsolete tags from Doxyfile:

```
# Remove these obsolete lines:
HTML_TIMESTAMP         = YES
FORMULA_TRANSPARENT    = YES
CLASS_DIAGRAMS         = YES
MSCGEN_PATH            = 
DOT_FONTNAME           = Helvetica
DOT_FONTSIZE           = 10
DOT_TRANSPARENT        = NO
TCL_SUBST              = 
```

### Output Directory Errors

**Problem**: `Output directory 'docs/generated' does not exist and cannot be created`

**Solution**: Update configuration and create directory:

```
# In Doxyfile
OUTPUT_DIRECTORY       = build/docs

# In workflow
- name: Generate API documentation with Doxygen
  run: |
    mkdir -p build/docs
    doxygen Doxyfile
```

### Missing Python Dependencies

**Problem**: `ModuleNotFoundError: No module named 'markdown'`

**Solution**: Add missing dependencies:

```yaml
- name: Install documentation tools
  run: |
    sudo apt-get update
    sudo apt-get install -y doxygen graphviz
    pip install sphinx breathe sphinx-rtd-theme myst-parser markdown
```

## Security Scanning Issues

### False Positives from Third-Party Code

**Problem**: High severity issues reported in ImGui/third-party libraries

**Solution**: Configure suppressions:

```bash
# Create suppressions file
cat > cppcheck-suppressions.txt << 'EOF'
# Suppress all issues in third-party libraries
*:src/imgui/*
*:src/imgwindow/*
# Suppress common false positives
missingIncludeSystem
unusedFunction
unmatchedSuppression
EOF

# Run with suppressions
cppcheck \
  --enable=warning,style,performance,portability \
  --suppressions-list=cppcheck-suppressions.txt \
  --xml \
  --output-file=cppcheck-results.xml \
  src/xp2gdl90.cpp src/ui/ tests/unit/ tests/integration/
```

### Flawfinder False Positives

**Solution**: Exclude third-party directories:

```bash
flawfinder \
  --quiet \
  --minlevel 2 \
  --exclude src/imgui/ \
  --exclude src/imgwindow/ \
  src/xp2gdl90.cpp src/ui/ tests/unit/ tests/integration/
```

## Local Development Issues

### Pre-commit Hook Failures

**Problem**: Pre-commit hooks failing due to missing tools

**Solution**: Run setup script:

```bash
./dev-setup.sh  # Installs all required tools
```

**Manual Installation**:

```bash
# macOS
brew install cmake pre-commit clang-format cppcheck

# Linux
sudo apt install cmake build-essential pre-commit clang-format cppcheck

# Install hooks
pre-commit install --install-hooks
```

### Build Configuration Issues

**Problem**: CMake configuration fails

**Solution**: Use proper build options:

```bash
# Basic build
cmake -DCMAKE_BUILD_TYPE=Release ..

# With testing
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON ..

# With benchmarks (requires Google Benchmark)
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON ..

# With documentation (requires Doxygen)
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=ON ..
```

### Missing Dependencies

**Problem**: Build fails due to missing libraries

**Solution**: Install platform-specific dependencies:

```bash
# macOS
brew install cmake google-benchmark doxygen graphviz

# Linux
sudo apt-get install cmake build-essential libbenchmark-dev doxygen graphviz

# Windows (chocolatey)
choco install cmake
vcpkg install benchmark:x64-windows
```

## Testing Workflow Components

Use the provided test script to validate your setup:

```bash
# Test workflow components locally
./scripts/test-workflow.sh

# Check specific components
cmake --build build --target test       # Run tests
cmake --build build --target coverage   # Generate coverage
cmake --build build --target docs       # Generate docs
```

## Getting Help

If you encounter issues not covered here:

1. **Check the workflow logs** in GitHub Actions for detailed error messages
2. **Run locally first** using `./scripts/test-workflow.sh`
3. **Search existing issues** in the GitHub repository
4. **Create a detailed issue** with:
   - Operating system and version
   - Complete error message
   - Steps to reproduce
   - Local vs CI environment details

## Contributing

If you find and fix a workflow issue, please:

1. Update this troubleshooting guide
2. Add the fix to the appropriate workflow file
3. Test both locally and in CI
4. Submit a pull request with clear description

---

*This troubleshooting guide is maintained as part of the XP2GDL90 project's comprehensive development workflow.*
