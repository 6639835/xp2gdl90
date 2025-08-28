#!/bin/bash

# XP2GDL90 Development Environment Setup Script
# 
# Automatically sets up development environment with all required tools
# and dependencies for cross-platform X-Plane plugin development

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script info
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_NAME="XP2GDL90"

echo -e "${BLUE}🚀 ${PROJECT_NAME} Development Environment Setup${NC}"
echo "=================================================="
echo "Project Directory: $SCRIPT_DIR"
echo "Detected OS: $(uname -s)"
echo ""

# Check if running with appropriate permissions
check_permissions() {
    if [[ "$OSTYPE" == "darwin"* ]] && ! command -v brew &> /dev/null; then
        echo -e "${YELLOW}⚠️  This script will install Homebrew if not present${NC}"
        echo -e "${YELLOW}   You may be prompted for your password${NC}"
        echo ""
    fi
}

# Install system dependencies
install_system_dependencies() {
    echo -e "${BLUE}📦 Installing system dependencies...${NC}"
    
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        if ! command -v brew &> /dev/null; then
            echo "Installing Homebrew..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        
        echo "Installing development tools via Homebrew..."
        brew update
        brew install cmake
        brew install llvm
        brew install pre-commit
        brew install cppcheck
        brew install doxygen
        brew install graphviz
        brew install python3
        brew install git
        
        # Install clang-format if not available
        if ! command -v clang-format &> /dev/null; then
            echo "Installing clang-format..."
            brew install clang-format
        fi
        
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux
        echo "Detecting Linux distribution..."
        
        if command -v apt-get &> /dev/null; then
            # Debian/Ubuntu
            sudo apt-get update
            sudo apt-get install -y \
                cmake \
                build-essential \
                clang \
                clang-format \
                cppcheck \
                doxygen \
                graphviz \
                python3 \
                python3-pip \
                git \
                pkg-config \
                libopengl-dev \
                libgl1-mesa-dev
                
        elif command -v yum &> /dev/null; then
            # RedHat/CentOS
            sudo yum groupinstall -y "Development Tools"
            sudo yum install -y \
                cmake \
                clang \
                clang-tools-extra \
                cppcheck \
                doxygen \
                graphviz \
                python3 \
                python3-pip \
                git \
                mesa-libGL-devel
                
        elif command -v pacman &> /dev/null; then
            # Arch Linux
            sudo pacman -S --needed \
                cmake \
                base-devel \
                clang \
                cppcheck \
                doxygen \
                graphviz \
                python \
                python-pip \
                git \
                mesa
        else
            echo -e "${RED}❌ Unsupported Linux distribution${NC}"
            echo "Please install dependencies manually:"
            echo "- cmake (3.10+)"
            echo "- clang/gcc compiler"
            echo "- clang-format"
            echo "- cppcheck"
            echo "- doxygen"
            echo "- python3"
            exit 1
        fi
        
        # Install pre-commit via pip
        python3 -m pip install --user pre-commit
        
    else
        echo -e "${RED}❌ Unsupported operating system: $OSTYPE${NC}"
        echo "Please install dependencies manually and use build_windows.bat on Windows"
        exit 1
    fi
    
    echo -e "${GREEN}✅ System dependencies installed${NC}"
}

# Set up Python virtual environment for development tools
setup_python_environment() {
    echo -e "${BLUE}🐍 Setting up Python development environment...${NC}"
    
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}❌ Python3 not found. Please install Python 3.7+ first.${NC}"
        exit 1
    fi
    
    # Create virtual environment for development tools
    if [ ! -d "$SCRIPT_DIR/.venv" ]; then
        echo "Creating Python virtual environment..."
        python3 -m venv "$SCRIPT_DIR/.venv"
    fi
    
    # Activate virtual environment
    source "$SCRIPT_DIR/.venv/bin/activate"
    
    # Upgrade pip
    pip install --upgrade pip
    
    # Install development tools
    pip install \
        pre-commit \
        black \
        isort \
        flake8 \
        mypy \
        pytest \
        pytest-cov \
        sphinx \
        breathe \
        cmake-format
    
    echo -e "${GREEN}✅ Python environment set up${NC}"
}

# Set up pre-commit hooks
setup_precommit_hooks() {
    echo -e "${BLUE}🪝 Setting up pre-commit hooks...${NC}"
    
    if [ ! -f "$SCRIPT_DIR/.pre-commit-config.yaml" ]; then
        echo -e "${RED}❌ .pre-commit-config.yaml not found${NC}"
        echo "Please ensure the pre-commit configuration exists"
        return 1
    fi
    
    # Install pre-commit hooks
    cd "$SCRIPT_DIR"
    pre-commit install
    pre-commit install --hook-type commit-msg
    
    # Run pre-commit on all files to ensure everything works
    echo "Testing pre-commit setup on existing files..."
    pre-commit run --all-files || {
        echo -e "${YELLOW}⚠️  Pre-commit found issues. Please fix them and run 'pre-commit run --all-files' again.${NC}"
    }
    
    echo -e "${GREEN}✅ Pre-commit hooks installed${NC}"
}

# Create development directories
create_dev_directories() {
    echo -e "${BLUE}📁 Creating development directories...${NC}"
    
    cd "$SCRIPT_DIR"
    
    # Create standard directories
    mkdir -p build-debug
    mkdir -p build-release
    mkdir -p docs/generated
    mkdir -p tools
    mkdir -p scripts/dev
    
    # Create .gitignore entries for build directories if not exists
    if [ -f .gitignore ]; then
        if ! grep -q "build-debug" .gitignore; then
            echo "build-debug/" >> .gitignore
        fi
        if ! grep -q "build-release" .gitignore; then
            echo "build-release/" >> .gitignore
        fi
        if ! grep -q "docs/generated" .gitignore; then
            echo "docs/generated/" >> .gitignore
        fi
    fi
    
    echo -e "${GREEN}✅ Development directories created${NC}"
}

# Set up IDE configuration
setup_ide_configuration() {
    echo -e "${BLUE}💻 Setting up IDE configuration...${NC}"
    
    cd "$SCRIPT_DIR"
    
    # VS Code configuration
    mkdir -p .vscode
    
    # Create VS Code settings
    cat > .vscode/settings.json << 'EOF'
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "C_Cpp.default.cppStandard": "c++17",
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/src/**",
        "${workspaceFolder}/tests/**",
        "${workspaceFolder}/SDK/CHeaders/**"
    ],
    "cmake.buildDirectory": "${workspaceFolder}/build-${buildType}",
    "cmake.generator": "Unix Makefiles",
    "files.associations": {
        "*.h": "c",
        "*.hpp": "cpp",
        "*.cpp": "cpp",
        "*.c": "c"
    },
    "editor.formatOnSave": true,
    "editor.rulers": [100],
    "editor.tabSize": 4,
    "editor.insertSpaces": true,
    "files.trimTrailingWhitespace": true,
    "files.insertFinalNewline": true,
    "python.defaultInterpreterPath": "./.venv/bin/python",
    "python.terminal.activateEnvironment": true
}
EOF

    # Create VS Code tasks
    cat > .vscode/tasks.json << 'EOF'
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build Debug",
            "type": "shell",
            "command": "./build.sh",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Build Release",
            "type": "shell",
            "command": "python",
            "args": ["build_all.py"],
            "group": "build",
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Run Tests",
            "type": "shell",
            "command": "cd build-debug && ctest --output-on-failure",
            "group": "test",
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            },
            "dependsOn": "Build Debug"
        },
        {
            "label": "Format Code",
            "type": "shell",
            "command": "pre-commit",
            "args": ["run", "clang-format", "--all-files"],
            "group": "build",
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            }
        }
    ]
}
EOF

    # Create VS Code launch configuration
    cat > .vscode/launch.json << 'EOF'
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug Tests",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build-debug/tests/xp2gdl90_tests",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "lldb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        }
    ]
}
EOF

    # CLion configuration hint
    if [ ! -f .idea/.gitignore ]; then
        mkdir -p .idea
        echo "*" > .idea/.gitignore
        echo "!.gitignore" >> .idea/.gitignore
    fi
    
    echo -e "${GREEN}✅ IDE configuration created${NC}"
}

# Build initial debug version
build_initial_version() {
    echo -e "${BLUE}🔨 Building initial debug version...${NC}"
    
    cd "$SCRIPT_DIR"
    
    # Create debug build
    mkdir -p build-debug
    cd build-debug
    
    cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON -DENABLE_COVERAGE=ON ..
    cmake --build . --config Debug
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Debug build successful${NC}"
        
        # Run tests if available
        if [ -f "tests/xp2gdl90_tests" ]; then
            echo "Running initial test suite..."
            ctest --output-on-failure
        fi
    else
        echo -e "${YELLOW}⚠️  Debug build failed - this may be expected if source files are incomplete${NC}"
    fi
    
    cd "$SCRIPT_DIR"
}

# Create useful development scripts
create_dev_scripts() {
    echo -e "${BLUE}📜 Creating development utility scripts...${NC}"
    
    cd "$SCRIPT_DIR"
    mkdir -p scripts/dev
    
    # Quick test runner
    cat > scripts/dev/test.sh << 'EOF'
#!/bin/bash
# Quick test runner for development

set -e

cd "$(dirname "$0")/../.."

echo "🧪 Running XP2GDL90 Test Suite"
echo "=============================="

# Build debug version if needed
if [ ! -d build-debug ]; then
    echo "Creating debug build..."
    mkdir -p build-debug
    cd build-debug
    cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON ..
    cd ..
fi

# Build and run tests
cd build-debug
cmake --build . --config Debug
echo ""
echo "Running tests..."
ctest --output-on-failure -V

# Generate coverage report if available
if command -v lcov &> /dev/null; then
    echo ""
    echo "Generating coverage report..."
    make coverage || echo "Coverage generation failed"
fi
EOF

    # Code formatter
    cat > scripts/dev/format.sh << 'EOF'
#!/bin/bash
# Format all code in the project

cd "$(dirname "$0")/../.."

echo "🎨 Formatting XP2GDL90 Code"
echo "============================"

if command -v pre-commit &> /dev/null; then
    pre-commit run --all-files
else
    echo "Pre-commit not found, running clang-format directly..."
    find src tests -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs clang-format -i
fi

echo "✅ Code formatting complete"
EOF

    # Clean script
    cat > scripts/dev/clean.sh << 'EOF'
#!/bin/bash
# Clean all build artifacts

cd "$(dirname "$0")/../.."

echo "🧹 Cleaning XP2GDL90 Build Artifacts"
echo "===================================="

rm -rf build build-debug build-release
rm -rf docs/generated
find . -name "*.o" -delete
find . -name "*.so" -delete
find . -name "*.dylib" -delete
find . -name "*.xpl" -delete

echo "✅ Clean complete"
EOF

    # Release builder
    cat > scripts/dev/release.sh << 'EOF'
#!/bin/bash
# Build release versions for all platforms

cd "$(dirname "$0")/../.."

echo "🚀 Building XP2GDL90 Release"
echo "============================="

# Clean previous builds
./scripts/dev/clean.sh

# Build all platforms
echo "Building cross-platform release..."
python build_all.py

# Verify builds
echo ""
echo "Build verification:"
echo "==================="

for platform in win mac lin; do
    if [ -f "build/${platform}.xpl" ]; then
        size=$(du -h "build/${platform}.xpl" | cut -f1)
        echo "✅ ${platform}.xpl (${size})"
    else
        echo "❌ ${platform}.xpl - BUILD FAILED"
    fi
done

echo ""
echo "Release build complete!"
EOF

    # Make scripts executable
    chmod +x scripts/dev/*.sh
    
    echo -e "${GREEN}✅ Development scripts created${NC}"
}

# Verify installation
verify_installation() {
    echo -e "${BLUE}🔍 Verifying installation...${NC}"
    
    # Check required tools
    local tools=(
        "cmake:CMake"
        "clang-format:Clang Format"
        "pre-commit:Pre-commit"
        "python3:Python 3"
    )
    
    local all_good=true
    
    for tool_info in "${tools[@]}"; do
        tool=$(echo "$tool_info" | cut -d: -f1)
        name=$(echo "$tool_info" | cut -d: -f2)
        
        if command -v "$tool" &> /dev/null; then
            version=$($tool --version 2>/dev/null | head -n1 || echo "unknown version")
            echo -e "✅ $name: ${GREEN}$version${NC}"
        else
            echo -e "❌ $name: ${RED}Not found${NC}"
            all_good=false
        fi
    done
    
    # Check project files
    local files=(
        ".pre-commit-config.yaml:Pre-commit configuration"
        ".clang-format:Clang-format configuration"
        "CMakeLists.txt:CMake configuration"
        "tests/CMakeLists.txt:Test configuration"
    )
    
    for file_info in "${files[@]}"; do
        file=$(echo "$file_info" | cut -d: -f1)
        name=$(echo "$file_info" | cut -d: -f2)
        
        if [ -f "$file" ]; then
            echo -e "✅ $name: ${GREEN}Present${NC}"
        else
            echo -e "❌ $name: ${RED}Missing${NC}"
            all_good=false
        fi
    done
    
    echo ""
    if [ "$all_good" = true ]; then
        echo -e "${GREEN}🎉 Development environment setup complete!${NC}"
        return 0
    else
        echo -e "${RED}⚠️  Some components are missing. Please check the errors above.${NC}"
        return 1
    fi
}

# Show usage information
show_usage_info() {
    echo -e "${BLUE}📚 Development Environment Usage${NC}"
    echo "================================="
    echo ""
    echo "Quick Start Commands:"
    echo "  ./scripts/dev/test.sh     - Run tests"
    echo "  ./scripts/dev/format.sh   - Format code"
    echo "  ./scripts/dev/clean.sh    - Clean builds"
    echo "  ./scripts/dev/release.sh  - Build release"
    echo ""
    echo "Build Commands:"
    echo "  ./build.sh                - Debug build (current platform)"
    echo "  python build_all.py       - Release build (all platforms)"
    echo ""
    echo "Development Workflow:"
    echo "  1. Make changes to source code"
    echo "  2. Run ./scripts/dev/format.sh to format"
    echo "  3. Run ./scripts/dev/test.sh to test"
    echo "  4. Commit (pre-commit hooks will run automatically)"
    echo ""
    echo "IDE Setup:"
    echo "  - VS Code: Open project folder (configuration included)"
    echo "  - CLion: Open CMakeLists.txt as project"
    echo ""
    echo "Python Environment:"
    echo "  source .venv/bin/activate  - Activate Python virtual environment"
    echo ""
}

# Main execution
main() {
    check_permissions
    
    echo -e "${BLUE}Starting setup process...${NC}"
    echo ""
    
    install_system_dependencies
    echo ""
    
    setup_python_environment
    echo ""
    
    setup_precommit_hooks
    echo ""
    
    create_dev_directories
    echo ""
    
    setup_ide_configuration
    echo ""
    
    create_dev_scripts
    echo ""
    
    build_initial_version
    echo ""
    
    if verify_installation; then
        echo ""
        show_usage_info
        echo ""
        echo -e "${GREEN}🚀 Ready to develop XP2GDL90!${NC}"
        exit 0
    else
        echo ""
        echo -e "${RED}❌ Setup incomplete. Please resolve the issues above.${NC}"
        exit 1
    fi
}

# Run main function
main "$@"
