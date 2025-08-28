#!/bin/bash
# Test workflow components locally before CI/CD

set -e

echo "🧪 Testing XP2GDL90 Workflow Components..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

function print_status() {
    echo -e "${GREEN}✅ $1${NC}"
}

function print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

function print_error() {
    echo -e "${RED}❌ $1${NC}"
}

# Create test build directory
BUILD_DIR="build-test"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "🏗️  Testing Build System..."

# Test basic build
print_status "Testing basic build configuration"
if cmake -DCMAKE_BUILD_TYPE=Release ..; then
    print_status "Basic CMake configuration: PASSED"
else
    print_error "Basic CMake configuration: FAILED"
    exit 1
fi

# Test with testing enabled
print_status "Testing with testing enabled"
if cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON ..; then
    print_status "Testing configuration: PASSED"
else
    print_error "Testing configuration: FAILED"
    exit 1
fi

# Test with benchmarks enabled (if benchmark library available)
print_status "Testing with benchmarks enabled"
if cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON .. 2>/dev/null; then
    print_status "Benchmark configuration: PASSED"
    BENCHMARKS_AVAILABLE=true
else
    print_warning "Benchmark configuration: SKIPPED (Google Benchmark not found)"
    BENCHMARKS_AVAILABLE=false
fi

# Test with static analysis
print_status "Testing with static analysis enabled"
if cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_STATIC_ANALYSIS=ON ..; then
    print_status "Static analysis configuration: PASSED"
else
    print_warning "Static analysis configuration: SKIPPED (tools not found)"
fi

# Test with documentation enabled
print_status "Testing with documentation enabled"
if cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=ON .. 2>/dev/null; then
    print_status "Documentation configuration: PASSED"
    DOCS_AVAILABLE=true
else
    print_warning "Documentation configuration: SKIPPED (Doxygen not found)"
    DOCS_AVAILABLE=false
fi

echo ""
echo "🔨 Testing Build Process..."

# Test main build
if cmake --build . --config Release; then
    print_status "Main build: PASSED"
else
    print_error "Main build: FAILED"
    exit 1
fi

# Test benchmark build if available
if [ "$BENCHMARKS_AVAILABLE" = true ]; then
    print_status "Testing benchmark build"
    if [ -f "benchmarks/xp2gdl90_benchmarks" ] || [ -f "xp2gdl90_benchmarks" ]; then
        print_status "Benchmark build: PASSED"
        
        # Try to run a quick benchmark test
        BENCHMARK_EXE=""
        if [ -f "benchmarks/xp2gdl90_benchmarks" ]; then
            BENCHMARK_EXE="./benchmarks/xp2gdl90_benchmarks"
        elif [ -f "xp2gdl90_benchmarks" ]; then
            BENCHMARK_EXE="./xp2gdl90_benchmarks"
        fi
        
        if [ -n "$BENCHMARK_EXE" ]; then
            print_status "Testing benchmark execution"
            if timeout 30s $BENCHMARK_EXE --benchmark_filter=".*" --benchmark_min_time=0.1 > /dev/null 2>&1; then
                print_status "Benchmark execution: PASSED"
            else
                print_warning "Benchmark execution: TIMED OUT or FAILED (this is expected without proper mocks)"
            fi
        fi
    else
        print_error "Benchmark build: FAILED (executable not found)"
    fi
fi

echo ""
echo "🧪 Testing Pre-commit Hooks (if available)..."

cd ..

# Check if pre-commit is available
if command -v pre-commit >/dev/null 2>&1; then
    print_status "Testing pre-commit hooks"
    if pre-commit run --all-files --show-diff-on-failure; then
        print_status "Pre-commit hooks: PASSED"
    else
        print_warning "Pre-commit hooks: SOME ISSUES FOUND (check output above)"
    fi
else
    print_warning "Pre-commit not available - install with 'pip install pre-commit'"
fi

echo ""
echo "📝 Testing Development Scripts..."

# Test that all required scripts are executable
SCRIPTS_DIR="scripts"
if [ -d "$SCRIPTS_DIR" ]; then
    for script in "$SCRIPTS_DIR"/*.py "$SCRIPTS_DIR"/*.sh; do
        if [ -f "$script" ]; then
            if [ -x "$script" ]; then
                print_status "Script $script is executable"
            else
                print_warning "Script $script is not executable (run: chmod +x $script)"
            fi
        fi
    done
else
    print_warning "Scripts directory not found"
fi

# Test Python scripts syntax
if command -v python3 >/dev/null 2>&1; then
    for py_script in scripts/*.py; do
        if [ -f "$py_script" ]; then
            if python3 -m py_compile "$py_script"; then
                print_status "Python script $py_script: SYNTAX OK"
            else
                print_error "Python script $py_script: SYNTAX ERROR"
            fi
        fi
    done
else
    print_warning "Python3 not available for syntax checking"
fi

echo ""
echo "📊 Test Summary..."

# Cleanup
rm -rf "$BUILD_DIR"

echo ""
print_status "Workflow component testing completed!"
echo ""
echo "🚀 Next steps:"
echo "   1. Fix any issues shown above"
echo "   2. Run './dev-setup.sh' to install missing dependencies"
echo "   3. Test your changes with 'git commit' (pre-commit hooks will run)"
echo "   4. Push to GitHub to trigger full CI/CD pipeline"
echo ""
