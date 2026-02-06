# Velo Quick Start Guide

Get up and running with Velo in 5 minutes!

## 🚀 Installation

### Option 1: From Source (Recommended for Development)

```bash
# Clone the repository
git clone https://github.com/yourusername/velo.git
cd velo

# Build in release mode
cargo build --release

# Install (optional)
sudo cp target/release/velo /usr/local/bin/

# Verify installation
velo --version
```

### Option 2: From Release Binary

```bash
# Download latest release
wget https://github.com/yourusername/velo/releases/latest/download/velo-linux-x86_64.tar.gz

# Extract
tar xzf velo-linux-x86_64.tar.gz

# Install
sudo mv velo /usr/local/bin/
sudo chmod +x /usr/local/bin/velo
```

## ⚡ First Steps

### 1. Initialize Velo

```bash
# Initialize the package database and configuration
sudo velo init

# This creates:
# - /etc/velo/velo.conf (configuration)
# - /var/lib/velo/db/ (package database)
# - /var/cache/velo/ (package cache)
```

### 2. Update Package Database

```bash
# Sync with repositories
sudo velo update
```

### 3. Install Your First Package

```bash
# Search for a package
velo search nginx

# Get package information
velo info nginx

# Install the package
sudo velo install nginx

# Verify installation
velo list | grep nginx
```

## 📚 Basic Commands

### Package Management

```bash
# Install a package
sudo velo install <package>

# Install multiple packages
sudo velo install nginx redis postgresql

# Remove a package
sudo velo remove <package>

# Remove with dependencies
sudo velo remove --recursive <package>

# Upgrade all packages
sudo velo upgrade

# Upgrade specific package
sudo velo upgrade nginx
```

### Information Commands

```bash
# Search for packages
velo search <query>

# Show package details
velo info <package>

# List installed packages
velo list
velo list --explicit  # Only user-installed
velo list --deps      # Only dependencies

# Show system information
velo system
```

### Repository Management

```bash
# List repositories
velo repo list

# Add a repository
sudo velo repo add custom https://repo.example.com

# Remove a repository
sudo velo repo remove custom

# Sync all repositories
sudo velo repo sync
```

### Transaction Management

```bash
# List all transactions
velo transaction list

# Show transaction details
velo transaction show tx-123456

# Rollback a transaction
sudo velo transaction rollback tx-123456
```

## 🎯 Common Use Cases

### Installing Development Tools

```bash
sudo velo install gcc make cmake git
```

### Setting Up a Web Server

```bash
# Install web stack
sudo velo install nginx postgresql redis

# Check what was installed
velo list --explicit
```

### Building from Source

```bash
# Create a PKGBUILD file (see examples/PKGBUILD)
velo build

# Build with optimization
velo build --optimize=native

# Build with multiple jobs
velo build --jobs=8
```

### Using USE Flags

```bash
# Show available USE flags for a package
velo use show nginx

# Enable a flag globally
sudo velo use add ssl

# Enable flag for specific package
sudo velo use set nginx +http2 +ssl

# Disable a flag
sudo velo use set nginx -debug

# Rebuild with new flags
sudo velo install --rebuild nginx
```

### Managing Transactions

```bash
# Every install creates a transaction
sudo velo install package

# View transaction history
velo transaction list

# If something breaks, rollback
sudo velo transaction rollback tx-latest
```

## 🔧 Configuration

### Edit Configuration File

```bash
sudo nano /etc/velo/velo.conf
```

Key settings:

```toml
[general]
db_path = "/var/lib/velo/db/installed.db"
cache_dir = "/var/cache/velo"
log_dir = "/var/log/velo"

[security]
require_signature = true      # Require GPG signatures
signature_level = "Required"  # Signature requirement level

[build]
jobs = "auto"                 # Parallel build jobs
optimize = "default"          # Optimization level
ccache = true                 # Use ccache
lto = true                    # Link-time optimization

[repository]
sync_on_update = true         # Auto-sync repos
parallel_downloads = 4        # Simultaneous downloads
```

## 🐛 Troubleshooting

### Package Installation Failed

```bash
# Check logs
sudo cat /var/log/velo/latest.log

# Try with verbose output
sudo velo install --verbose package

# Check dependencies
velo info package
```

### Database Corruption

```bash
# Backup current database
sudo cp -r /var/lib/velo/db /var/lib/velo/db.backup

# Rebuild database
sudo velo db rebuild
```

### Signature Verification Failed

```bash
# Update GPG keys
sudo velo keys update

# Import specific key
sudo velo keys add keyfile.gpg

# Temporarily disable signature check (not recommended)
sudo velo install --no-signature package
```

### Transaction Stuck

```bash
# Check active transactions
velo transaction list

# Force rollback
sudo velo transaction rollback --force tx-id
```

## 📖 Next Steps

- Read the full [README.md](README.md) for detailed information
- Check [CONTRIBUTING.md](CONTRIBUTING.md) to contribute
- See [GIT_GITHUB_SETUP.md](GIT_GITHUB_SETUP.md) for development setup
- Browse examples in the `examples/` directory
- Join our community (links coming soon)

## 💡 Tips

1. **Always update before installing**: `sudo velo update && sudo velo install package`
2. **Use transactions**: They allow you to rollback if something goes wrong
3. **Check logs**: `/var/log/velo/` contains detailed operation logs
4. **Use USE flags wisely**: Only enable what you need
5. **Keep your system updated**: Run `sudo velo upgrade` regularly

## 🆘 Getting Help

```bash
# Show help for any command
velo help
velo install --help
velo transaction --help

# Check version
velo --version

# Show system information
velo system
```

---

**Welcome to Velo!** 🚀

For more detailed information, check out the full documentation in the repository.
