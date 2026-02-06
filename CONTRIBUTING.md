# Contributing to Velo

Thank you for your interest in contributing to Velo! This document provides guidelines and instructions for contributing.

## 🚀 Getting Started

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/your-username/velo.git
   cd velo
   ```
3. Add the upstream remote:
   ```bash
   git remote add upstream https://github.com/original-owner/velo.git
   ```

## 🏗️ Development Setup

### Prerequisites

- Rust 1.70 or higher
- SQLite 3.35+
- Git
- GnuPG (optional, for signature verification)

### Building

```bash
cargo build
cargo test
```

### Running

```bash
cargo run -- --help
```

## 📝 Code Style

We follow standard Rust formatting guidelines:

```bash
# Format your code
cargo fmt

# Check for common issues
cargo clippy

# Run tests
cargo test
```

## 🔍 Pull Request Process

1. **Create a branch** for your feature or fix:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** following our coding standards

3. **Write tests** for new functionality

4. **Run the test suite**:
   ```bash
   cargo test
   cargo fmt -- --check
   cargo clippy -- -D warnings
   ```

5. **Commit your changes** with clear, descriptive messages:
   ```bash
   git commit -m "Add feature: description of changes"
   ```

6. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

7. **Create a Pull Request** from your fork to our `develop` branch

### PR Guidelines

- Keep PRs focused on a single feature or fix
- Include tests for new functionality
- Update documentation as needed
- Reference any related issues
- Ensure CI passes before requesting review

## 🐛 Bug Reports

When reporting bugs, please include:

- Velo version (`velo --version`)
- Operating system and version
- Steps to reproduce
- Expected behavior
- Actual behavior
- Any error messages or logs

## 💡 Feature Requests

We welcome feature suggestions! Please:

- Check if the feature has already been requested
- Clearly describe the feature and its use case
- Explain how it would benefit users
- Consider implementation complexity

## 📚 Documentation

Documentation improvements are always welcome:

- Fix typos or unclear explanations
- Add examples
- Improve code comments
- Write tutorials or guides

## 🧪 Testing

We maintain high test coverage. When adding new code:

- Write unit tests for individual functions
- Add integration tests for features
- Test edge cases and error conditions
- Ensure tests are deterministic

## 🎯 Areas for Contribution

Looking for ways to contribute? Check out:

- Issues labeled `good-first-issue`
- Issues labeled `help-wanted`
- Documentation improvements
- Performance optimizations
- New feature implementations

## 📞 Communication

- GitHub Issues - Bug reports and feature requests
- GitHub Discussions - General questions and ideas
- Pull Requests - Code contributions

## 📜 License

By contributing, you agree that your contributions will be licensed under the same license as the project (MIT OR Apache-2.0).

## 🙏 Recognition

Contributors will be recognized in:
- The project README
- Release notes
- Our contributors page (coming soon)

Thank you for contributing to Velo! 🚀
