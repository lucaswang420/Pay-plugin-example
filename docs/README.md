# Pay Plugin Documentation

This directory contains all documentation for the Pay Plugin project.

## 📂 Directory Structure

```
docs/
├── architecture/           # Architecture design documents
│   ├── overview.md         # Overall architecture overview
│   ├── service-layer.md    # Service layer design details
│   └── data-flow.md        # Data flow and call chains
│
├── development/            # Development-related documents
│   ├── setup.md            # Development environment setup
│   ├── coding-standards.md # Coding standards and conventions
│   └── debugging.md        # Debugging guide
│
├── api/                    # API documentation
│   ├── reference.md        # API interface reference
│   └── examples.md         # API usage examples
│
├── testing/                # Testing documentation
│   ├── strategy.md         # Testing strategy
│   ├── unit-test.md        # Unit testing guide
│   └── integration-test.md # Integration testing guide
│
├── deployment/             # Deployment documentation
│   ├── docker.md           # Docker deployment
│   ├── production.md       # Production environment setup
│   └── monitoring.md       # Monitoring configuration
│
├── operations/             # Operations documentation
│   ├── troubleshooting.md  # Troubleshooting guide
│   ├── maintenance.md      # Maintenance procedures
│   └── backup.md           # Backup strategies
│
└── history/                # Historical documents
    ├── refactoring/        # Refactoring project documents
    │   ├── 2026-04-09-design.md         # Refactoring design document
    │   └── 2026-04-09-implementation.md # Refactoring implementation plan
    └── decisions/          # Architecture Decision Records (ADRs)
```

## 📖 Quick Links

### For New Developers
1. Start with [architecture/overview.md](architecture/overview.md)
2. Set up your environment: [development/setup.md](development/setup.md)
3. Read coding standards: [development/coding-standards.md](development/coding-standards.md)

### For API Users
- API Reference: [api/reference.md](api/reference.md)
- API Examples: [api/examples.md](api/examples.md)

### For Testing
- Testing Strategy: [testing/strategy.md](testing/strategy.md)
- Unit Testing: [testing/unit-test.md](testing/unit-test.md)
- Integration Testing: [testing/integration-test.md](testing/integration-test.md)

### For Deployment
- Docker Deployment: [deployment/docker.md](deployment/docker.md)
- Production Setup: [deployment/production.md](deployment/production.md)
- Monitoring: [deployment/monitoring.md](deployment/monitoring.md)

### For Operations
- Troubleshooting: [operations/troubleshooting.md](operations/troubleshooting.md)
- Maintenance: [operations/maintenance.md](operations/maintenance.md)

## 📝 Document Status

### Current Documents (✅ Available)

#### Architecture
- [architecture_overview.md](architecture/architecture_overview.md) - System architecture design

#### Development
- [environment_setup.md](development/environment_setup.md) - Development environment setup
- [configuration_guide.md](development/configuration_guide.md) - Application configuration guide
- [migration_guide.md](development/migration_guide.md) - API migration guide
- [alipay_sandbox_setup.md](development/alipay_sandbox_setup.md) - Alipay sandbox setup
- [alipay_sandbox_quickstart.md](development/alipay_sandbox_quickstart.md) - Alipay quick start

#### API
- [pay-api-examples.md](api/pay-api-examples.md) - Payment API usage examples
- [api_configuration_guide.md](api/api_configuration_guide.md) - API configuration guide
- [api_key_configuration.md](api/api_key_configuration.md) - API key configuration

#### Testing
- [testing_guide.md](testing/testing_guide.md) - Testing framework usage
- [e2e_testing_guide.md](testing/e2e_testing_guide.md) - End-to-end testing guide

#### Deployment
- [deployment_guide.md](deployment/deployment_guide.md) - Deployment instructions
- [monitoring_setup.md](deployment/monitoring_setup.md) - Monitoring configuration

#### Operations
- [operations_manual.md](operations/operations_manual.md) - Operations manual
- [troubleshooting.md](operations/troubleshooting.md) - Troubleshooting guide
- [security_checklist.md](operations/security_checklist.md) - Security checklist
- [security_audit_report_2026_04_13.md](operations/security_audit_report_2026_04_13.md) - Security audit report
- [health_check_implementation.md](operations/health_check_implementation.md) - Health check implementation

### Historical Documents

#### Refactoring History
- [2026-04-09-design.md](history/refactoring/2026-04-09-design.md) - Refactoring design document
- [2026-04-09-implementation.md](history/refactoring/2026-04-09-implementation.md) - Refactoring implementation plan
- [2026-04-13-option-b-implementation.md](history/refactoring/2026-04-13-option-b-implementation.md) - Option B implementation plan

#### Project Reports
- [project_completion_summary.md](history/reports/project_completion_summary.md) - Project completion summary
- [release_notes_v1.0.md](history/reports/release_notes_v1.0.md) - Release notes v1.0
- [production_readiness_roadmap.md](history/reports/production_readiness_roadmap.md) - Production readiness roadmap
- [performance_test_final_report.md](history/reports/performance_test_final_report.md) - Performance test report
- [performance_optimization_complete.md](history/reports/performance_optimization_complete.md) - Performance optimization report
- [current_status_2026_04_13.md](history/reports/current_status_2026_04_13.md) - Project status (2026-04-13)
- [configuration_status.md](history/reports/configuration_status.md) - Configuration status
- [test_update_progress.md](history/reports/test_update_progress.md) - Test update progress

#### Archive
- [archive/legacy_docs/](history/archive/legacy_docs/) - Legacy documentation
- [archive/*.backup](history/archive/) - Test file backups

## 🤝 Contributing

When adding new documentation:
1. Place it in the appropriate category directory
2. Use clear, descriptive filenames
3. Update this README if adding a new category
4. Follow the existing naming conventions
5. Move outdated documents to `history/reports/` with date prefixes

## 📜 Document Lifecycle

- **Active Documents** - Kept in category directories (architecture/, development/, etc.)
- **Status Reports** - Move to `history/reports/` when superseded
- **Project Records** - Keep in `history/refactoring/` for reference
- **Legacy/Archive** - Move to `history/archive/` for historical context
