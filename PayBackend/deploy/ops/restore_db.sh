#!/bin/bash
# Pay Plugin Database Restore Script
# Version: 1.0.0
# Description: Restore PostgreSQL database from backup

set -e  # Exit on error

# ============================================================================
# Configuration
# ============================================================================
DB_NAME="${DB_NAME:-pay_production}"
DB_USER="${DB_USER:-pay_user}"
DB_ADMIN="${DB_ADMIN:-postgres}"
BACKUP_FILE="${1:-}"

# ============================================================================
# Functions
# ============================================================================

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1"
}

error_exit() {
    log "ERROR: $1"
    exit 1
}

confirm() {
    read -p "$1 (y/N): " -n 1 -r
    echo
    [[ $REPLY =~ ^[Yy]$ ]]
}

# ============================================================================
# Validation
# ============================================================================

if [ -z "$BACKUP_FILE" ]; then
    error_exit "Usage: $0 <backup_file.sql.gz>"
fi

if [ ! -f "$BACKUP_FILE" ]; then
    error_exit "Backup file not found: $BACKUP_FILE"
fi

# Display backup info
FILE_SIZE=$(du -h "$BACKUP_FILE" | cut -f1)
log "Backup file: $BACKUP_FILE ($FILE_SIZE)"

# ============================================================================
# Warning and Confirmation
# ============================================================================

log "WARNING: This will completely replace the existing database '$DB_NAME'"
log "All existing data will be lost!"
confirm "Are you sure you want to continue?" || error_exit "Restore aborted by user"

# ============================================================================
# Stop Application
# ============================================================================

log "Stopping PayPlugin service..."
if systemctl is-active --quiet payplugin; then
    sudo systemctl stop payplugin || error_exit "Failed to stop payplugin service"
    log "PayPlugin service stopped"
else
    log "PayPlugin service is not running"
fi

# ============================================================================
# Restore Database
# ============================================================================

# Temporary file for decompressed backup
TEMP_SQL="/tmp/restore_$(date +%s).sql"

log "Decompressing backup..."
gunzip -c "$BACKUP_FILE" > "$TEMP_SQL" || error_exit "Decompression failed"

log "Dropping existing database..."
psql -U "$DB_ADMIN" -c "DROP DATABASE IF EXISTS $DB_NAME;" || error_exit "Failed to drop database"

log "Creating new database..."
psql -U "$DB_ADMIN" -c "CREATE DATABASE $DB_NAME OWNER $DB_USER;" || error_exit "Failed to create database"

log "Restoring data..."
psql -U "$DB_USER" -d "$DB_NAME" < "$TEMP_SQL" || error_exit "Restore failed"

# Clean up temp file
rm -f "$TEMP_SQL"

# ============================================================================
# Verification
# ============================================================================

log "Verifying restore..."
TABLE_COUNT=$(psql -U "$DB_USER" -d "$DB_NAME" -t -c "SELECT count(*) FROM information_schema.tables WHERE table_schema = 'public';")
log "Tables in database: $TABLE_COUNT"

# ============================================================================
# Start Application
# ============================================================================

log "Starting PayPlugin service..."
sudo systemctl start payplugin || error_exit "Failed to start payplugin service"
log "PayPlugin service started"

# Wait for service to be ready
sleep 5

# Health check
log "Performing health check..."
if curl -f -s http://localhost:5566/health > /dev/null; then
    log "Health check passed"
else
    log "WARNING: Health check failed - service may not be ready yet"
fi

log "Restore completed successfully!"

exit 0
