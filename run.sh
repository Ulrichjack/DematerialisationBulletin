#!/bin/bash

# Forcer l'utilisation COMPLÈTE des libs système
export LD_LIBRARY_PATH=/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu
unset SNAP
unset SNAP_CONTEXT
unset SNAP_INSTANCE_NAME

# Créer dossiers si nécessaire
mkdir -p data ui

echo "🚀 Démarrage de l'application..."
exec ./bulletin_scanner "$@"