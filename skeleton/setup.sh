#!/bin/bash

if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Sistem macOS detectat. Folosesc Homebrew..."
    if ! command -v brew &> /dev/null; then
        echo "Eroare: Homebrew nu este instalat. Instalați-l de pe brew.sh"
        exit 1
    fi
    brew install libconfig jpeg-turbo libpng optipng


elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Sistem Linux/WSL detectat. Folosesc APT..."
    sudo apt update
    sudo apt install -y libconfig-dev libjpeg-turbo8-dev libpng-dev optipng

else
    echo "Sistem de operare necunoscut sau nesuportat direct de acest script."
    echo "Dacă folosești Windows, te rugăm să instalezi WSL (Ubuntu) și să rulezi scriptul din nou."
fi
#ruleaza in terminal: chmod +x setup.sh 
#apoi: ./setup.sh