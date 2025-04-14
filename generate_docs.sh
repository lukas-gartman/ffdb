#!/bin/bash

# Fail on any error
set -e

if ! command -v doxygen &> /dev/null; then
    echo "Doxygen is not installed and is required to generate documentation."
    echo "Install with (Arch): sudo pacman -S doxygen"
    exit 1
fi

if ! command -v dot &> /dev/null; then
    echo "Graphviz (dot) not found. Diagrams will not be generated."
    echo "Install with (Arch): sudo pacman -S graphviz"
fi

if [ ! -d "docs/doxygen-awesome" ]; then
    echo "Downloading doxygen-awesome-css..."
    curl -sSL https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v2.3.4.zip -o temp.zip
    unzip -o temp.zip -d docs
    mv docs/doxygen-awesome-css-2.3.4 docs/doxygen-awesome
    rm temp.zip
fi

echo "Generating documentation..."
doxygen Doxyfile > /dev/null

echo -e "Documentation generated at \033]8;;file://$(pwd)/docs/html/index.html\033\\docs/html/index.html\033]8;;\033\\"
