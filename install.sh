#!/bin/bash
echo "Téléchargement de MINI..."
curl -L -o /data/data/com.termux/files/usr/bin/mini https://raw.githubusercontent.com/HackerCompagnion7/mini/main/mini
chmod 755 /data/data/com.termux/files/usr/bin/mini
echo "MINI installé ! Tape 'mini' pour l'utiliser."
