#!/usr/bin/env python3
"""
XP2GDL90 Version Bump Script

Automatically manages semantic versioning for the XP2GDL90 plugin.
Supports major, minor, patch, and pre-release version bumps.

Usage:
    python scripts/bump_version.py --type patch
    python scripts/bump_version.py --type minor --dry-run
    python scripts/bump_version.py --type major --commit
"""

import argparse
import re
import sys
import subprocess
import os
from pathlib import Path
from typing import Tuple, Optional, List
from datetime import datetime
import json


class SemanticVersion:
    """Represents a semantic version with major.minor.patch-prerelease+build format"""
    
    def __init__(self, version_string: str):
        self.parse(version_string)
    
    def parse(self, version_string: str):
        """Parse a semantic version string"""
        # Remove 'v' prefix if present
        version_string = version_string.lstrip('v')
        
        # Regex for semantic versioning
        pattern = r'^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9\-\.]+))?(?:\+([a-zA-Z0-9\-\.]+))?$'
        match = re.match(pattern, version_string)
        
        if not match:
            raise ValueError(f"Invalid semantic version: {version_string}")
        
        self.major = int(match.group(1))
        self.minor = int(match.group(2))
        self.patch = int(match.group(3))
        self.prerelease = match.group(4) or None
        self.build = match.group(5) or None
    
    def bump(self, bump_type: str, prerelease_type: Optional[str] = None) -> 'SemanticVersion':
        """Create a new version with the specified bump type"""
        if bump_type == 'major':
            return SemanticVersion(f"{self.major + 1}.0.0")
        elif bump_type == 'minor':
            return SemanticVersion(f"{self.major}.{self.minor + 1}.0")
        elif bump_type == 'patch':
            return SemanticVersion(f"{self.major}.{self.minor}.{self.patch + 1}")
        elif bump_type == 'prerelease':
            if self.prerelease:
                # Increment existing prerelease
                if prerelease_type and not self.prerelease.startswith(prerelease_type):
                    # Change prerelease type
                    return SemanticVersion(f"{self.major}.{self.minor}.{self.patch}-{prerelease_type}.1")
                else:
                    # Increment existing prerelease number
                    parts = self.prerelease.split('.')
                    if len(parts) >= 2 and parts[-1].isdigit():
                        parts[-1] = str(int(parts[-1]) + 1)
                        prerelease = '.'.join(parts)
                    else:
                        prerelease = f"{self.prerelease}.1"
                    return SemanticVersion(f"{self.major}.{self.minor}.{self.patch}-{prerelease}")
            else:
                # Create new prerelease
                prerelease_type = prerelease_type or 'alpha'
                return SemanticVersion(f"{self.major}.{self.minor}.{self.patch}-{prerelease_type}.1")
        else:
            raise ValueError(f"Invalid bump type: {bump_type}")
    
    def __str__(self) -> str:
        """Return the full version string"""
        version = f"{self.major}.{self.minor}.{self.patch}"
        if self.prerelease:
            version += f"-{self.prerelease}"
        if self.build:
            version += f"+{self.build}"
        return version
    
    def __repr__(self) -> str:
        return f"SemanticVersion('{str(self)}')"


class VersionBumper:
    """Manages version bumping across all project files"""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.files_to_update = [
            {
                'path': 'VERSION',
                'pattern': r'.*',
                'replacement': lambda v: str(v)
            },
            {
                'path': 'README.md',
                'pattern': r'!\[Version\]\(https://img\.shields\.io/badge/version-([^-]+)-brightgreen\)',
                'replacement': lambda v: f'![Version](https://img.shields.io/badge/version-{v}-brightgreen)'
            },
            {
                'path': 'CHANGELOG.md',
                'pattern': r'^## \[Unreleased\]',
                'replacement': lambda v: f'## [Unreleased]\n\n## [{v}] - {datetime.now().strftime("%Y-%m-%d")}'
            },
            {
                'path': 'CMakeLists.txt',
                'pattern': r'project\(xp2gdl90\s+VERSION\s+([^\s\)]+)',
                'replacement': lambda v: f'project(xp2gdl90 VERSION {v}'
            }
        ]
    
    def get_current_version(self) -> SemanticVersion:
        """Get the current version from VERSION file"""
        version_file = self.project_root / 'VERSION'
        
        if not version_file.exists():
            raise FileNotFoundError(f"VERSION file not found at {version_file}")
        
        with open(version_file, 'r') as f:
            version_string = f.read().strip()
        
        return SemanticVersion(version_string)
    
    def update_version_in_files(self, new_version: SemanticVersion, dry_run: bool = False) -> List[str]:
        """Update version in all project files"""
        updated_files = []
        
        for file_info in self.files_to_update:
            file_path = self.project_root / file_info['path']
            
            if not file_path.exists():
                print(f"Warning: {file_path} not found, skipping...")
                continue
            
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            pattern = file_info['pattern']
            replacement = file_info['replacement'](new_version)
            
            if file_info['path'] == 'VERSION':
                # Special handling for VERSION file - replace entire content
                new_content = str(new_version)
            elif file_info['path'] == 'CHANGELOG.md':
                # Special handling for CHANGELOG - add new section
                if re.search(pattern, content, re.MULTILINE):
                    new_content = re.sub(pattern, replacement, content, count=1, flags=re.MULTILINE)
                else:
                    # Add unreleased section if not present
                    lines = content.split('\n')
                    # Find the first ## [version] line and insert before it
                    for i, line in enumerate(lines):
                        if re.match(r'^## \[[^\]]+\]', line):
                            lines.insert(i, f'## [Unreleased]\n')
                            lines.insert(i + 1, f'## [{new_version}] - {datetime.now().strftime("%Y-%m-%d")}')
                            break
                    new_content = '\n'.join(lines)
            else:
                # Regular regex replacement
                new_content = re.sub(pattern, replacement, content)
            
            if content != new_content:
                if not dry_run:
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                updated_files.append(str(file_path))
                print(f"Updated: {file_path}")
            else:
                print(f"No changes needed: {file_path}")
        
        return updated_files
    
    def validate_git_repo(self) -> bool:
        """Check if we're in a git repository"""
        try:
            subprocess.run(['git', 'rev-parse', '--git-dir'], 
                         check=True, capture_output=True, cwd=self.project_root)
            return True
        except (subprocess.CalledProcessError, FileNotFoundError):
            return False
    
    def check_git_status(self) -> Tuple[bool, List[str]]:
        """Check if there are uncommitted changes"""
        if not self.validate_git_repo():
            return True, []  # No git repo, assume clean
        
        try:
            result = subprocess.run(['git', 'status', '--porcelain'], 
                                  check=True, capture_output=True, text=True, 
                                  cwd=self.project_root)
            
            if result.stdout.strip():
                uncommitted = result.stdout.strip().split('\n')
                return False, uncommitted
            return True, []
        except subprocess.CalledProcessError:
            return False, ['Git status check failed']
    
    def create_git_commit(self, new_version: SemanticVersion, updated_files: List[str]) -> bool:
        """Create a git commit for the version bump"""
        if not self.validate_git_repo():
            print("Not a git repository, skipping commit")
            return False
        
        try:
            # Add updated files
            subprocess.run(['git', 'add'] + updated_files, 
                         check=True, cwd=self.project_root)
            
            # Create commit
            commit_message = f"chore: bump version to {new_version}\n\nAutomated version bump"
            subprocess.run(['git', 'commit', '-m', commit_message], 
                         check=True, cwd=self.project_root)
            
            print(f"Created git commit for version {new_version}")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Failed to create git commit: {e}")
            return False
    
    def create_git_tag(self, new_version: SemanticVersion, push: bool = False) -> bool:
        """Create a git tag for the new version"""
        if not self.validate_git_repo():
            return False
        
        tag_name = f"v{new_version}"
        
        try:
            # Create annotated tag
            tag_message = f"Release {new_version}\n\nAutomated release tag"
            subprocess.run(['git', 'tag', '-a', tag_name, '-m', tag_message], 
                         check=True, cwd=self.project_root)
            
            print(f"Created git tag: {tag_name}")
            
            if push:
                # Push tag to origin
                subprocess.run(['git', 'push', 'origin', tag_name], 
                             check=True, cwd=self.project_root)
                print(f"Pushed tag to origin: {tag_name}")
            
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Failed to create git tag: {e}")
            return False
    
    def generate_changelog_entry(self, old_version: SemanticVersion, new_version: SemanticVersion) -> str:
        """Generate changelog entry based on git commits"""
        if not self.validate_git_repo():
            return f"## [{new_version}] - {datetime.now().strftime('%Y-%m-%d')}\n\n- Version bump from {old_version}"
        
        try:
            # Get commits since last version
            result = subprocess.run([
                'git', 'log', f'v{old_version}..HEAD', '--pretty=format:- %s', '--no-merges'
            ], check=True, capture_output=True, text=True, cwd=self.project_root)
            
            commits = result.stdout.strip()
            
            if not commits:
                commits = "- No significant changes"
            
            return f"## [{new_version}] - {datetime.now().strftime('%Y-%m-%d')}\n\n{commits}"
            
        except subprocess.CalledProcessError:
            return f"## [{new_version}] - {datetime.now().strftime('%Y-%m-%d')}\n\n- Version bump from {old_version}"


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Bump version for XP2GDL90 plugin',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    python scripts/bump_version.py --type patch
    python scripts/bump_version.py --type minor --dry-run
    python scripts/bump_version.py --type major --commit --tag
    python scripts/bump_version.py --type prerelease --prerelease-type beta
        '''
    )
    
    parser.add_argument('--type', 
                       choices=['major', 'minor', 'patch', 'prerelease'],
                       required=True,
                       help='Type of version bump')
    
    parser.add_argument('--prerelease-type',
                       choices=['alpha', 'beta', 'rc'],
                       default='alpha',
                       help='Type of prerelease (when using --type prerelease)')
    
    parser.add_argument('--dry-run',
                       action='store_true',
                       help='Show what would be changed without making changes')
    
    parser.add_argument('--commit',
                       action='store_true', 
                       help='Create git commit for version bump')
    
    parser.add_argument('--tag',
                       action='store_true',
                       help='Create git tag for version bump')
    
    parser.add_argument('--push',
                       action='store_true',
                       help='Push tag to origin (requires --tag)')
    
    parser.add_argument('--force',
                       action='store_true',
                       help='Force version bump even with uncommitted changes')
    
    args = parser.parse_args()
    
    # Determine project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    print(f"XP2GDL90 Version Bumper")
    print(f"======================")
    print(f"Project root: {project_root}")
    print()
    
    bumper = VersionBumper(project_root)
    
    try:
        # Get current version
        current_version = bumper.get_current_version()
        print(f"Current version: {current_version}")
        
        # Calculate new version
        if args.type == 'prerelease':
            new_version = current_version.bump(args.type, args.prerelease_type)
        else:
            new_version = current_version.bump(args.type)
        
        print(f"New version: {new_version}")
        print()
        
        # Check git status if committing
        if args.commit and not args.force:
            is_clean, uncommitted = bumper.check_git_status()
            if not is_clean:
                print("Error: Repository has uncommitted changes:")
                for change in uncommitted:
                    print(f"  {change}")
                print("\nUse --force to proceed anyway or commit your changes first.")
                sys.exit(1)
        
        # Perform dry run or actual update
        if args.dry_run:
            print("DRY RUN - No changes will be made")
            print("Would update the following files:")
        else:
            print("Updating version in files:")
        
        updated_files = bumper.update_version_in_files(new_version, args.dry_run)
        
        if args.dry_run:
            print(f"\nDry run complete. Would update {len(updated_files)} files.")
            sys.exit(0)
        
        print(f"\nVersion bump complete: {current_version} → {new_version}")
        
        # Create git commit if requested
        if args.commit:
            if bumper.create_git_commit(new_version, updated_files):
                print("Git commit created successfully")
            else:
                print("Warning: Failed to create git commit")
        
        # Create git tag if requested
        if args.tag:
            if bumper.create_git_tag(new_version, args.push):
                print("Git tag created successfully")
            else:
                print("Warning: Failed to create git tag")
        
        print()
        print("Next steps:")
        if not args.commit:
            print("1. Review the changes")
            print("2. Commit the changes: git add . && git commit -m 'chore: bump version'")
        if not args.tag:
            print(f"3. Create a release tag: git tag -a v{new_version} -m 'Release {new_version}'")
        print("4. Push changes: git push && git push --tags")
        print("5. Create GitHub release from the tag")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
