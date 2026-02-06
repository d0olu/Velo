# Velo - Universal Package Manager

**Velo** is a universal package manager for Linux that combines the best practices from existing solutions into a fast, flexible, and secure tool.

## ✨ Features

- ⚡ **Speed and simplicity** like Pacman
- 🎛️ **Flexibility and USE flags** like Portage  
- 🔒 **Reliable dependency resolution** like APT
- 🧩 **Modularity and rollbacks** like DNF
- ⚛️ **Transaction atomicity** like Nix

### Key Capabilities

- Automatic CPU architecture detection (x86_64, ARM, RISC-V)
- Mandatory GPG signature verification for all packages
- Hybrid approach: binary packages + source building
- Atomic transactions with rollback support
- USE flags for flexible feature configuration
- Slots for multiple version installations
- Parallel download and installation
- Minimal dependencies and fast operation

## 📦 Installation

### From Source

```bash
git clone https://github.com/yourusername/velo.git
cd velo
cargo build --release
sudo cp target/release/velo /usr/local/bin/
```

### Quick Start

```bash
# Initialize system
sudo velo init

# Update package database
sudo velo update

# Install a package
sudo velo install nginx

# Search for packages
velo search firefox

# System information
velo system
```

## 🚀 Basic Commands

### Package Management

```bash
# Install
velo install <package>

# Remove
velo remove <package>

# Upgrade
velo upgrade

# Search
velo search <query>

# Information
velo info <package>

# List installed
velo list
velo list --explicit  # only explicitly installed
velo list --deps      # only dependencies
```

### Building from Source

```bash
# Basic build
velo build

# With optimization
velo build --optimize=aggressive

# For specific CPU
velo build --optimize=native

# Multi-threaded
velo build --jobs=8
```

### USE Flags

```bash
# Show USE flags
velo use list

# Add global flag
velo use add ssl

# Set for package
velo use set nginx +http2
velo use set nginx -debug
```

### Repositories

```bash
# List repositories
velo repo list

# Add repository
velo repo add myrepo https://repo.example.com

# Synchronize
velo repo sync
```

## 📁 Architecture

```
velo/
├── src/
│   ├── main.rs           # Entry point
│   ├── lib.rs            # Main library module
│   ├── architecture.rs   # Architecture detection
│   ├── security.rs       # Security system and GPG
│   ├── package.rs        # Package management
│   ├── repository.rs     # Repository management
│   ├── database.rs       # Package database
│   ├── transaction.rs    # Transactions and rollbacks
│   ├── dependency.rs     # Dependency resolution
│   ├── build.rs          # Build system
│   ├── config.rs         # Configuration
│   ├── cli.rs            # CLI interface
│   ├── useflags.rs       # USE flags system
│   ├── pkgbuild.rs       # PKGBUILD parser
│   └── error.rs          # Error handling
└── Cargo.toml
```

## 🖥️ Architecture Detection

Velo automatically detects optimal architecture for your system:

```bash
$ velo system

Architecture Information:
  Architecture: x86_64
  Microarchitecture: x86_64-v3
  CPU Features: sse2, sse4.2, avx, avx2, fma, bmi2
  Optimal package type: x86_64/optimized
  Fallback: x86_64/generic
```

## 🔐 Security

All packages must be signed with GPG keys. Velo uses a keyring system:

- **Master** - Velo master keys
- **Official** - Official repositories
- **Community** - Community repositories
- **User** - User keys
- **Revoked** - Revoked keys

## ⚛️ Transactions

All operations are performed atomically with rollback capability:

```bash
# List transactions
velo transaction list

# Transaction information
velo transaction show <id>

# Rollback transaction
velo transaction rollback <id>
```

## ⚙️ Configuration

Main configuration file: `/etc/velo/velo.conf`

```toml
[general]
db_path = "/var/lib/velo/db/installed.db"
cache_dir = "/var/cache/velo"
log_dir = "/var/log/velo"

[security]
require_signature = true
signature_level = "Required"

[build]
jobs = "auto"
optimize = "default"
ccache = true
lto = true
```

## 🛠️ Development

### Requirements

- Rust 1.70+
- SQLite 3.35+
- GnuPG (optional)

### Building

```bash
cargo build
cargo test
cargo run -- --help
```

### Project Structure

The project follows Rust best practices:
- Modular architecture
- Error handling via `thiserror` and `anyhow`
- Async via `tokio`
- CLI via `clap`
- Serialization via `serde`

## 📄 License

This project is licensed under either of:

- MIT License ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)
- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)

at your option.

## 👥 Authors

Velo Development Team

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## 📞 Support

- GitHub Issues: https://github.com/yourusername/velo/issues
- Documentation: https://docs.velo.dev (coming soon)
- Discord: https://discord.gg/velo (coming soon)

---

Made with ❤️ for the Linux community
