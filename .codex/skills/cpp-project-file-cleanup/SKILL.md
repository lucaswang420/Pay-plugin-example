---
name: cpp-project-file-cleanup
description: Standardize and organize C++ project file structure. Use this skill whenever the user mentions cleaning up project files, organizing documentation, archiving old files, removing backups, standardizing file layout, restructuring docs, cleaning up repository, or says things like "my project is messy" or "too many files in my docs folder". This skill handles creating archive directories, moving backup/temporary files to archive, updating .gitignore, creating documentation indexes, and general project file organization for C/C++ projects.
compatibility:
  - Works with any C/C++ project (Drogon, CMake, Makefile, etc.)
  - Requires file system access
---

# C++ Project File Cleanup and Organization

This skill helps you organize and standardize the file structure of C/C++ projects by:

- Creating organized archive directories
- Moving backup and temporary files to archive
- Updating .gitignore to prevent future clutter
- Creating documentation indexes
- Standardizing project structure

## When to Use This Skill

Use this skill when:
- Project has accumulated backup files (*.backup, *.bak, *.old)
- Documentation is disorganized or mixed together
- Test files have accumulated temporary versions
- .gitignore is missing or incomplete
- You want to clean up before a release or handoff
- Project documentation lacks a clear index or navigation

## What This Skill Does

### 1. Analyze Current File Structure

First, examine the project to identify:

```bash
# Find backup and temporary files
find . -name "*.backup" -o -name "*.bak" -o -name "*.tmp" -o -name "*_old*" -o -name "*.before_*"

# Check documentation organization
ls -la docs/ 2>/dev/null || echo "No docs/ directory"

# Check current .gitignore
cat .gitignore 2>/dev/null || echo "No .gitignore"
```

### 2. Create Archive Structure

Create a standardized archive directory structure:

```
docs/
├── archive/
│   ├── tdd_reports/        # Test-driven development reports
│   ├── legacy_docs/        # Old project documentation
│   └── backups/            # Code and test file backups
├── README.md               # Documentation index
├── deployment_guide.md     # Current docs
└── ...
```

**Rules for categorizing files:**
- **tdd_reports/**: Any file starting with `tdd_*.md`, test execution reports, validation plans
- **legacy_docs/**: Old phase reports, validation reports, summaries, configuration docs that are outdated
- **backups/**: Source code or test file backups (*.backup, *.backup2, *.before_*, test_*.sh)

### 3. Move Files to Archive

Move backup and temporary files to appropriate archive locations:

```bash
# Create archive directories
mkdir -p docs/archive/{tdd_reports,legacy_docs,backups}

# Move backup files
find . -name "*.backup" -exec mv {} docs/archive/backups/ \;
find . -name "*.backup2" -exec mv {} docs/archive/backups/ \;
find . -name "*.before_*" -exec mv {} docs/archive/backups/ \;

# Move TDD reports
find docs/ -name "tdd_*.md" -exec mv {} docs/archive/tdd_reports/ \;

# Move legacy docs (phase reports, validation reports, etc.)
find docs/ -name "phase*_*.md" -exec mv {} docs/archive/legacy_docs/ \;
find docs/ -name "*_report.md" -exec mv {} docs/archive/legacy_docs/ \;
```

### 4. Update .gitignore

Create or update `.gitignore` with comprehensive ignore patterns:

```gitignore
# Build directories
build/
cmake-build-*/
out/

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# Backup and temporary files
*.backup
*.backup2
*.bak
*.tmp
*.old
*.before_*
*.after_*

# Test artifacts
test/*.backup
test/*.backup2
test/*.before_*
test/test_*.sh

# Python
__pycache__/
*.py[cod]
*$py.class

# Logs
*.log
logs/

# OS generated files
.DS_Store
Thumbs.db
desktop.ini

# Environment files
.env
.env.local
.env.*.local

# Coverage reports
coverage/
*.coverage
*.gcov
*.gcda
*.gcno
```

**Key principles:**
- Be comprehensive but not overly aggressive
- Preserve important files (don't ignore everything)
- Comment sections to explain what's ignored and why

### 5. Create Documentation Index

Create a `docs/README.md` that serves as a navigation hub:

```markdown
# Project Documentation Center

## Quick Start

Essential documentation for new users:
- [Main README](../README.md) - Project overview
- [Deployment Guide](deployment_guide.md) - How to deploy
- [Operations Manual](operations_manual.md) - Daily operations

## Documentation Categories

### Deployment & Operations
- deployment_guide.md
- operations_manual.md
- monitoring_setup.md

### Architecture & Design
- architecture_overview.md
- api_reference.md
- migration_guide.md

## Archive

Historical documentation and reports are archived in:
- [archive/tdd_reports/](archive/tdd_reports/) - Test development history
- [archive/legacy_docs/](archive/legacy_docs/) - Old project reports
```

**Organization principles:**
- Categorize docs by purpose (deployment, operations, architecture)
- Provide quick links to most important docs
- Include archive section with descriptions
- Add recommended reading paths for different user types

### 6. Update Main README

Ensure the main `README.md` reflects current project status:
- Update version number and status
- Add badge for test coverage (if known)
- Update features list
- Ensure all section links work
- Add quick links to key documentation

## Best Practices

### DO:
- ✅ Archive rather than delete - preserve project history
- ✅ Organize by category (TDD, legacy, backups)
- ✅ Create clear documentation indexes
- ✅ Update .gitignore to prevent future clutter
- ✅ Communicate what you're doing and why
- ✅ Test that the project still builds after cleanup

### DON'T:
- ❌ Delete files that might be needed - archive them instead
- ❌ Ignore everything in .gitignore - be selective
- ❌ Move files without understanding their purpose
- ❌ Create too many subdirectories - keep structure simple
- ❌ Archive currently active documentation

## Verification

After cleanup, verify:

1. **Project still builds:**
   ```bash
   # Test compilation
   cd PayBackend
   scripts/build.bat  # or your build command
   ```

2. **Tests still run:**
   ```bash
   # Run tests
   ./build/Release/test_payplugin.exe
   ```

3. **No important files lost:**
   - Check that essential source files are present
   - Verify config files (config.json, CMakeLists.txt) remain
   - Ensure current documentation is accessible

4. **Git status is clean:**
   ```bash
   git status
   # Should show organized changes, not random clutter
   ```

## Example Output

After cleanup, the project structure should look like:

```
project/
├── docs/
│   ├── README.md (index)
│   ├── deployment_guide.md
│   ├── operations_manual.md
│   ├── monitoring_setup.md
│   └── archive/
│       ├── tdd_reports/ (9 files)
│       ├── legacy_docs/ (8 files)
│       └── backups/ (4 files)
├── .gitignore (comprehensive)
├── README.md (updated)
└── src/ (clean and organized)
```

## Common Issues

**Issue:** Archive directory becomes too cluttered
**Solution:** Create subdirectories by year or phase if needed

**Issue:** Can't find files after moving
**Solution:** The documentation index (docs/README.md) helps locate archived content

**Issue:** .gitignore is too aggressive
**Solution:** Test with `git check-ignore` before committing

**Issue:** Project doesn't build after moving files
**Solution:** Ensure you only moved backup files, not source files needed for compilation

## Tips for Success

1. **Start with analysis** - Don't move files until you understand what they are
2. **Communicate clearly** - Tell the user what you're doing and why
3. **Create backups** - Use git to ensure you can revert if needed
4. **Test thoroughly** - Build and test after cleanup
5. **Document changes** - Update README.md with new structure
6. **Be conservative** - When in doubt, archive rather than delete

## Related Skills

- **cpp-release-build-setup** - Set up correct C++ build configuration
- **production-readiness-docs** - Create comprehensive documentation package
- **repository-cleanup** - General repository cleanup for any project
