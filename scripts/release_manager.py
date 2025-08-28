#!/usr/bin/env python3
"""
XP2GDL90 Advanced Release Manager

Comprehensive release management system with validation, testing,
and automated deployment capabilities.

Features:
- Pre-release validation
- Automated testing
- Changelog generation
- Asset preparation
- Release publishing
- Post-release verification

Usage:
    python scripts/release_manager.py --prepare --version 1.0.4
    python scripts/release_manager.py --validate
    python scripts/release_manager.py --release --tag v1.0.4
"""

import argparse
import json
import subprocess
import sys
import os
import tempfile
import shutil
import zipfile
import tarfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from datetime import datetime
import requests
import hashlib


@dataclass
class ReleaseAsset:
    """Represents a release asset"""
    name: str
    path: Path
    description: str
    platform: str
    size: int
    checksum: str


@dataclass
class ReleaseInfo:
    """Release information"""
    version: str
    tag: str
    title: str
    description: str
    assets: List[ReleaseAsset]
    prerelease: bool
    draft: bool


class ReleaseManager:
    """Advanced release management system"""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.build_dir = project_root / "build"
        self.dist_dir = project_root / "dist"
        self.temp_dir = None
        
        # Release configuration
        self.platforms = ["windows", "mac", "linux"]
        self.asset_extensions = {
            "windows": ".xpl",
            "mac": ".xpl", 
            "linux": ".xpl"
        }
        
    def __enter__(self):
        self.temp_dir = Path(tempfile.mkdtemp())
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.temp_dir and self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)
    
    def validate_environment(self) -> bool:
        """Validate the release environment"""
        print("🔍 Validating release environment...")
        
        checks = [
            self._check_git_repository(),
            self._check_clean_working_directory(),
            self._check_required_files(),
            self._check_version_consistency(),
            self._check_build_tools(),
        ]
        
        if all(checks):
            print("✅ Environment validation passed")
            return True
        else:
            print("❌ Environment validation failed")
            return False
    
    def _check_git_repository(self) -> bool:
        """Check if we're in a git repository"""
        try:
            subprocess.run(['git', 'rev-parse', '--git-dir'], 
                         check=True, capture_output=True, cwd=self.project_root)
            return True
        except (subprocess.CalledProcessError, FileNotFoundError):
            print("  ❌ Not in a git repository")
            return False
    
    def _check_clean_working_directory(self) -> bool:
        """Check if working directory is clean"""
        try:
            result = subprocess.run(['git', 'status', '--porcelain'], 
                                  check=True, capture_output=True, text=True,
                                  cwd=self.project_root)
            if result.stdout.strip():
                print("  ❌ Working directory is not clean")
                print(f"     Uncommitted changes: {result.stdout.strip()}")
                return False
            return True
        except subprocess.CalledProcessError:
            print("  ❌ Cannot check git status")
            return False
    
    def _check_required_files(self) -> bool:
        """Check if required files exist"""
        required_files = [
            "VERSION",
            "README.md", 
            "CHANGELOG.md",
            "CMakeLists.txt",
            "src/xp2gdl90.cpp"
        ]
        
        missing_files = []
        for file_path in required_files:
            if not (self.project_root / file_path).exists():
                missing_files.append(file_path)
        
        if missing_files:
            print(f"  ❌ Missing required files: {', '.join(missing_files)}")
            return False
        return True
    
    def _check_version_consistency(self) -> bool:
        """Check version consistency across files"""
        try:
            version_file = self.project_root / "VERSION"
            with open(version_file) as f:
                version = f.read().strip()
            
            # Check README version badge
            readme_file = self.project_root / "README.md"
            with open(readme_file) as f:
                readme_content = f.read()
                
            if f"version-{version}-" not in readme_content:
                print(f"  ⚠️  Version {version} not found in README.md badge")
                return False
            
            return True
            
        except Exception as e:
            print(f"  ❌ Version consistency check failed: {e}")
            return False
    
    def _check_build_tools(self) -> bool:
        """Check if build tools are available"""
        tools = ['cmake', 'git']
        missing_tools = []
        
        for tool in tools:
            if not shutil.which(tool):
                missing_tools.append(tool)
        
        if missing_tools:
            print(f"  ❌ Missing build tools: {', '.join(missing_tools)}")
            return False
        return True
    
    def run_tests(self) -> bool:
        """Run the test suite"""
        print("🧪 Running test suite...")
        
        try:
            # Build tests
            test_build_dir = self.temp_dir / "test-build"
            test_build_dir.mkdir()
            
            print("  Building tests...")
            subprocess.run([
                'cmake', 
                '-DCMAKE_BUILD_TYPE=Release',
                '-DENABLE_TESTING=ON',
                str(self.project_root)
            ], check=True, cwd=test_build_dir, capture_output=True)
            
            subprocess.run([
                'cmake', '--build', '.', '--config', 'Release'
            ], check=True, cwd=test_build_dir, capture_output=True)
            
            print("  Running tests...")
            result = subprocess.run([
                'ctest', '--output-on-failure', '--timeout', '300'
            ], cwd=test_build_dir, capture_output=True, text=True)
            
            if result.returncode == 0:
                print("  ✅ All tests passed")
                return True
            else:
                print("  ❌ Test failures detected")
                print(f"     Output: {result.stdout}")
                return False
                
        except subprocess.CalledProcessError as e:
            print(f"  ❌ Test execution failed: {e}")
            return False
    
    def build_all_platforms(self) -> bool:
        """Build for all supported platforms"""
        print("🔨 Building for all platforms...")
        
        try:
            # Use the existing build_all.py script
            result = subprocess.run([
                'python', 'build_all.py'
            ], cwd=self.project_root, capture_output=True, text=True)
            
            if result.returncode == 0:
                print("  ✅ All platform builds successful")
                return True
            else:
                print("  ❌ Build failures detected")
                print(f"     Output: {result.stdout}")
                return False
                
        except subprocess.CalledProcessError as e:
            print(f"  ❌ Build failed: {e}")
            return False
    
    def prepare_assets(self, version: str) -> List[ReleaseAsset]:
        """Prepare release assets"""
        print("📦 Preparing release assets...")
        
        assets = []
        self.dist_dir.mkdir(exist_ok=True)
        
        # Individual platform binaries
        for platform in self.platforms:
            binary_name = f"{platform}{self.asset_extensions[platform]}"
            binary_path = self.build_dir / binary_name
            
            if binary_path.exists():
                # Copy to dist directory with versioned name
                versioned_name = f"xp2gdl90-{version}-{platform}{self.asset_extensions[platform]}"
                dist_path = self.dist_dir / versioned_name
                shutil.copy2(binary_path, dist_path)
                
                # Calculate checksum
                checksum = self._calculate_file_hash(dist_path)
                
                asset = ReleaseAsset(
                    name=versioned_name,
                    path=dist_path,
                    description=f"XP2GDL90 plugin for {platform.title()}",
                    platform=platform,
                    size=dist_path.stat().st_size,
                    checksum=checksum
                )
                assets.append(asset)
                print(f"  ✅ Prepared {versioned_name} ({asset.size} bytes)")
            else:
                print(f"  ⚠️  Missing binary for {platform}: {binary_path}")
        
        # Create multi-platform archive
        if len(assets) > 1:
            archive_name = f"xp2gdl90-{version}-all-platforms.zip"
            archive_path = self.dist_dir / archive_name
            
            with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as zf:
                # Create README for archive
                readme_content = self._create_archive_readme(version, assets)
                zf.writestr("README.txt", readme_content)
                
                # Add binaries
                for asset in assets:
                    original_name = f"{asset.platform}{self.asset_extensions[asset.platform]}"
                    zf.write(asset.path, f"xp2gdl90/{original_name}")
            
            # Create asset for archive
            archive_asset = ReleaseAsset(
                name=archive_name,
                path=archive_path,
                description="XP2GDL90 plugin for all platforms",
                platform="all",
                size=archive_path.stat().st_size,
                checksum=self._calculate_file_hash(archive_path)
            )
            assets.append(archive_asset)
            print(f"  ✅ Created multi-platform archive ({archive_asset.size} bytes)")
        
        # Create checksums file
        self._create_checksums_file(assets, version)
        
        return assets
    
    def _create_archive_readme(self, version: str, assets: List[ReleaseAsset]) -> str:
        """Create README content for archive"""
        content = f"""XP2GDL90 Plugin v{version}
========================

X-Plane GDL-90 Broadcasting Plugin

Installation:
1. Choose the correct plugin file for your platform:
"""
        
        for asset in assets:
            if asset.platform != "all":
                platform_name = {
                    "windows": "Windows",
                    "mac": "macOS", 
                    "linux": "Linux"
                }.get(asset.platform, asset.platform.title())
                
                original_name = f"{asset.platform}{self.asset_extensions[asset.platform]}"
                content += f"   - {platform_name}: {original_name}\n"
        
        content += f"""
2. Create directory: X-Plane/Resources/plugins/xp2gdl90/
3. Copy the appropriate .xpl file to that directory
4. Restart X-Plane

Features:
- Direct X-Plane SDK integration
- Real-time GDL-90 data broadcasting
- Support for ownship position and traffic targets
- Cross-platform compatibility
- No X-Plane Data Output configuration required

Documentation: https://github.com/youruser/xp2gdl90

Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} UTC
"""
        return content
    
    def _create_checksums_file(self, assets: List[ReleaseAsset], version: str):
        """Create checksums file for assets"""
        checksums_path = self.dist_dir / f"xp2gdl90-{version}-checksums.txt"
        
        with open(checksums_path, 'w') as f:
            f.write(f"XP2GDL90 v{version} - File Checksums (SHA256)\n")
            f.write("=" * 50 + "\n\n")
            
            for asset in assets:
                f.write(f"{asset.checksum}  {asset.name}\n")
            
            f.write(f"\nGenerated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} UTC\n")
        
        print(f"  ✅ Created checksums file")
    
    def _calculate_file_hash(self, file_path: Path) -> str:
        """Calculate SHA256 hash of a file"""
        sha256_hash = hashlib.sha256()
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha256_hash.update(chunk)
        return sha256_hash.hexdigest()
    
    def generate_changelog(self, version: str, previous_version: Optional[str] = None) -> str:
        """Generate changelog for the release"""
        print("📝 Generating changelog...")
        
        if not previous_version:
            # Try to get previous version from git tags
            try:
                result = subprocess.run([
                    'git', 'describe', '--tags', '--abbrev=0', 'HEAD~1'
                ], check=True, capture_output=True, text=True, cwd=self.project_root)
                previous_version = result.stdout.strip()
            except subprocess.CalledProcessError:
                previous_version = None
        
        changelog_sections = {
            'feat': '🚀 New Features',
            'fix': '🐛 Bug Fixes',
            'docs': '📚 Documentation',
            'style': '🎨 Style Changes',
            'refactor': '♻️ Code Refactoring',
            'perf': '⚡ Performance Improvements',
            'test': '🧪 Tests',
            'chore': '🔧 Maintenance',
            'other': '📋 Other Changes'
        }
        
        changelog = f"# Release v{version}\n\n"
        
        if previous_version:
            try:
                # Get commits since previous version
                result = subprocess.run([
                    'git', 'log', f'{previous_version}..HEAD', 
                    '--pretty=format:%s', '--no-merges'
                ], check=True, capture_output=True, text=True, cwd=self.project_root)
                
                commits = result.stdout.strip().split('\n') if result.stdout.strip() else []
                
                # Categorize commits
                categorized = {section: [] for section in changelog_sections.keys()}
                
                for commit in commits:
                    commit = commit.strip()
                    if not commit:
                        continue
                        
                    # Parse conventional commit format
                    found_category = False
                    for prefix, category in [
                        ('feat:', 'feat'), ('feature:', 'feat'),
                        ('fix:', 'fix'), ('bugfix:', 'fix'),
                        ('docs:', 'docs'), ('doc:', 'docs'),
                        ('style:', 'style'),
                        ('refactor:', 'refactor'),
                        ('perf:', 'perf'), ('performance:', 'perf'),
                        ('test:', 'test'), ('tests:', 'test'),
                        ('chore:', 'chore')
                    ]:
                        if commit.lower().startswith(prefix):
                            categorized[category].append(commit[len(prefix):].strip())
                            found_category = True
                            break
                    
                    if not found_category:
                        categorized['other'].append(commit)
                
                # Generate changelog sections
                for category, title in changelog_sections.items():
                    if categorized[category]:
                        changelog += f"## {title}\n\n"
                        for item in categorized[category]:
                            changelog += f"- {item}\n"
                        changelog += "\n"
                        
            except subprocess.CalledProcessError:
                changelog += "- Version bump and maintenance updates\n\n"
        else:
            changelog += "- Initial release\n\n"
        
        # Add technical details
        changelog += f"""## 📋 Technical Details

**Release Date**: {datetime.now().strftime('%Y-%m-%d')}
**Commit**: {self._get_current_commit()[:8]}
**Platforms**: Windows, macOS, Linux
**X-Plane Compatibility**: 11.55+ and 12.x

"""
        
        return changelog
    
    def _get_current_commit(self) -> str:
        """Get current git commit hash"""
        try:
            result = subprocess.run([
                'git', 'rev-parse', 'HEAD'
            ], check=True, capture_output=True, text=True, cwd=self.project_root)
            return result.stdout.strip()
        except subprocess.CalledProcessError:
            return "unknown"
    
    def create_release_info(self, version: str, assets: List[ReleaseAsset]) -> ReleaseInfo:
        """Create release information"""
        tag = f"v{version}" if not version.startswith('v') else version
        
        # Determine if this is a prerelease
        prerelease = any(keyword in version.lower() 
                        for keyword in ['alpha', 'beta', 'rc', 'dev'])
        
        # Generate changelog
        changelog = self.generate_changelog(version.lstrip('v'))
        
        return ReleaseInfo(
            version=version.lstrip('v'),
            tag=tag,
            title=f"XP2GDL90 Plugin {version}",
            description=changelog,
            assets=assets,
            prerelease=prerelease,
            draft=False
        )
    
    def validate_assets(self, assets: List[ReleaseAsset]) -> bool:
        """Validate release assets"""
        print("🔍 Validating release assets...")
        
        for asset in assets:
            if not asset.path.exists():
                print(f"  ❌ Asset not found: {asset.path}")
                return False
            
            if asset.size == 0:
                print(f"  ❌ Asset is empty: {asset.name}")
                return False
            
            # Verify checksum
            actual_checksum = self._calculate_file_hash(asset.path)
            if actual_checksum != asset.checksum:
                print(f"  ❌ Checksum mismatch for {asset.name}")
                return False
            
            print(f"  ✅ {asset.name} ({asset.size} bytes, {actual_checksum[:8]}...)")
        
        print("✅ Asset validation passed")
        return True
    
    def cleanup(self):
        """Clean up temporary files"""
        if self.build_dir.exists():
            print("🧹 Cleaning up build artifacts...")
            for file in self.build_dir.glob("*.xpl"):
                file.unlink()


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='XP2GDL90 Advanced Release Manager',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    python scripts/release_manager.py --validate
    python scripts/release_manager.py --prepare --version 1.0.4
    python scripts/release_manager.py --release --tag v1.0.4 --publish
        '''
    )
    
    parser.add_argument('--validate', 
                       action='store_true',
                       help='Validate release environment')
    
    parser.add_argument('--prepare',
                       action='store_true', 
                       help='Prepare release assets')
    
    parser.add_argument('--release',
                       action='store_true',
                       help='Create release')
    
    parser.add_argument('--version',
                       help='Release version')
    
    parser.add_argument('--tag',
                       help='Git tag for release')
    
    parser.add_argument('--test',
                       action='store_true',
                       help='Run tests before release')
    
    parser.add_argument('--build',
                       action='store_true',
                       help='Build all platforms')
    
    parser.add_argument('--publish',
                       action='store_true',
                       help='Publish release to GitHub')
    
    parser.add_argument('--dry-run',
                       action='store_true',
                       help='Show what would be done without doing it')
    
    args = parser.parse_args()
    
    # Determine project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    print("🚀 XP2GDL90 Advanced Release Manager")
    print("=" * 40)
    print(f"Project root: {project_root}")
    print(f"Mode: {'DRY RUN' if args.dry_run else 'LIVE'}")
    print()
    
    try:
        with ReleaseManager(project_root) as manager:
            
            # Validate environment
            if args.validate or args.prepare or args.release:
                if not manager.validate_environment():
                    sys.exit(1)
            
            # Run tests
            if args.test:
                if not manager.run_tests():
                    sys.exit(1)
            
            # Build all platforms
            if args.build:
                if not manager.build_all_platforms():
                    sys.exit(1)
            
            # Prepare release
            if args.prepare:
                if not args.version:
                    print("❌ Version required for prepare")
                    sys.exit(1)
                
                if args.dry_run:
                    print("DRY RUN: Would prepare assets for version", args.version)
                else:
                    assets = manager.prepare_assets(args.version)
                    if manager.validate_assets(assets):
                        print("✅ Release assets prepared successfully")
                    else:
                        sys.exit(1)
            
            # Create release
            if args.release:
                version = args.version or (args.tag.lstrip('v') if args.tag else None)
                if not version:
                    print("❌ Version or tag required for release")
                    sys.exit(1)
                
                if args.dry_run:
                    print("DRY RUN: Would create release for version", version)
                else:
                    # Prepare assets if not already done
                    assets = manager.prepare_assets(version)
                    if not manager.validate_assets(assets):
                        sys.exit(1)
                    
                    # Create release info
                    release_info = manager.create_release_info(version, assets)
                    
                    print("🎉 Release preparation complete!")
                    print(f"   Version: {release_info.version}")
                    print(f"   Tag: {release_info.tag}")
                    print(f"   Assets: {len(release_info.assets)}")
                    print(f"   Prerelease: {release_info.prerelease}")
                    
                    if args.publish:
                        print("\n📤 Publishing to GitHub...")
                        print("Note: GitHub publishing requires additional implementation")
                        print("Use the GitHub CLI or web interface to create the release")
    
    except Exception as e:
        print(f"❌ Release process failed: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
