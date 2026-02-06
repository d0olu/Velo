#!/bin/bash

# Git Initialization Script for Velo
# This script helps you quickly set up Git and push to GitHub

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔══════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   Velo Git Setup Script              ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════╝${NC}"
echo

# Check if we're in the right directory
if [ ! -f "Cargo.toml" ]; then
    echo -e "${YELLOW}⚠ Warning: Cargo.toml not found. Are you in the project directory?${NC}"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check if git is installed
if ! command -v git &> /dev/null; then
    echo -e "${YELLOW}✗ Git is not installed. Please install Git first.${NC}"
    echo "  Ubuntu/Debian: sudo apt install git"
    echo "  Fedora: sudo dnf install git"
    echo "  Arch: sudo pacman -S git"
    exit 1
fi

echo -e "${GREEN}✓ Git is installed${NC}"
echo

# Configure Git user if not already configured
if [ -z "$(git config --global user.name)" ]; then
    echo -e "${BLUE}📝 Git configuration needed${NC}"
    read -p "Enter your name: " git_name
    git config --global user.name "$git_name"
    echo -e "${GREEN}✓ Name configured${NC}"
fi

if [ -z "$(git config --global user.email)" ]; then
    read -p "Enter your email: " git_email
    git config --global user.email "$git_email"
    echo -e "${GREEN}✓ Email configured${NC}"
fi

echo
echo -e "${GREEN}✓ Git user configured:${NC}"
echo "  Name:  $(git config --global user.name)"
echo "  Email: $(git config --global user.email)"
echo

# Initialize git repository if not already initialized
if [ ! -d ".git" ]; then
    echo -e "${BLUE}🔧 Initializing Git repository...${NC}"
    git init
    echo -e "${GREEN}✓ Repository initialized${NC}"
else
    echo -e "${GREEN}✓ Git repository already initialized${NC}"
fi

# Create initial commit if no commits exist
if ! git rev-parse HEAD > /dev/null 2>&1; then
    echo
    echo -e "${BLUE}📦 Creating initial commit...${NC}"
    
    git add .
    git commit -m "Initial commit: Velo package manager

- Universal package manager for Linux
- Atomic transactions with rollback support
- USE flags system for flexible configuration
- Multi-architecture support
- GPG signature verification
- Parallel downloads and installations
- Hybrid binary/source package approach"
    
    echo -e "${GREEN}✓ Initial commit created${NC}"
else
    echo -e "${GREEN}✓ Commits already exist${NC}"
fi

# Rename branch to main if it's master
current_branch=$(git branch --show-current)
if [ "$current_branch" = "master" ]; then
    echo -e "${BLUE}🔄 Renaming branch to 'main'...${NC}"
    git branch -M main
    echo -e "${GREEN}✓ Branch renamed${NC}"
fi

echo
echo -e "${BLUE}🌐 GitHub Setup${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "To push this repository to GitHub:"
echo
echo "1. Create a new repository on GitHub:"
echo "   https://github.com/new"
echo
echo "2. Repository name: velo"
echo "   Description: Universal package manager for Linux"
echo "   Visibility: Public (recommended) or Private"
echo "   DO NOT initialize with README, .gitignore, or license"
echo
echo "3. After creating the repository, run:"
echo

read -p "Enter your GitHub username: " github_user
echo
echo -e "${YELLOW}Copy and run these commands:${NC}"
echo
echo -e "${GREEN}# Add GitHub as remote${NC}"
echo "git remote add origin https://github.com/$github_user/velo.git"
echo
echo -e "${GREEN}# Push to GitHub${NC}"
echo "git push -u origin main"
echo
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo
echo -e "${YELLOW}💡 Tip: For better security, set up SSH keys${NC}"
echo "   See GIT_GITHUB_SETUP.md for detailed instructions"
echo
echo -e "${GREEN}✓ Setup complete!${NC}"
echo

# Offer to add remote if user confirms
read -p "Would you like to add the GitHub remote now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if git remote | grep -q "origin"; then
        echo -e "${YELLOW}⚠ Remote 'origin' already exists${NC}"
        git remote -v
    else
        git remote add origin "https://github.com/$github_user/velo.git"
        echo -e "${GREEN}✓ Remote added${NC}"
        echo
        echo "To push to GitHub, run:"
        echo "  git push -u origin main"
    fi
fi

echo
echo -e "${BLUE}╔══════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   Next Steps                         ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════╝${NC}"
echo
echo "1. Review GIT_GITHUB_SETUP.md for detailed instructions"
echo "2. Push your code: git push -u origin main"
echo "3. Set up branch protection rules on GitHub"
echo "4. Enable GitHub Actions (already configured!)"
echo "5. Create your first release: git tag v0.1.0 && git push --tags"
echo
echo -e "${GREEN}Happy coding! 🚀${NC}"
