#!/bin/bash

echo "╔═══════════════════════════════════════════════════╗"
echo "║  Script de Test - Scanner de Bulletins           ║"
echo "╚═══════════════════════════════════════════════════╝"
echo ""

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Fonction de vérification
check_file() {
    if [ -f "$1" ]; then
        echo -e "${GREEN}✅ $1 existe${NC}"
        return 0
    else
        echo -e "${RED}❌ $1 manquant${NC}"
        return 1
    fi
}

# 1. Vérification des fichiers sources
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "1️⃣  Vérification des fichiers sources"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

FILES_OK=true

check_file "include/bulletin.h" || FILES_OK=false
check_file "include/database.h" || FILES_OK=false
check_file "include/ocr_utils.h" || FILES_OK=false
check_file "src/bulletin_utils.c" || FILES_OK=false
check_file "src/database.c" || FILES_OK=false
check_file "src/ocr_utils.c" || FILES_OK=false
check_file "src/main.c" || FILES_OK=false
check_file "Makefile" || FILES_OK=false

if [ "$FILES_OK" = false ]; then
    echo -e "\n${RED}⚠️  Certains fichiers manquent !${NC}"
    exit 1
fi

echo ""

# 2. Vérification des dépendances
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "2️⃣  Vérification des dépendances"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

DEPS_OK=true

if command -v tesseract &> /dev/null; then
    echo -e "${GREEN}✅ Tesseract installé$(tesseract --version | head -1)${NC}"
else
    echo -e "${RED}❌ Tesseract manquant${NC}"
    echo -e "${YELLOW}   Installation: sudo apt install tesseract-ocr tesseract-ocr-fra${NC}"
    DEPS_OK=false
fi

if command -v pdftoppm &> /dev/null; then
    echo -e "${GREEN}✅ pdftoppm installé (poppler-utils)${NC}"
else
    echo -e "${RED}❌ pdftoppm manquant${NC}"
    echo -e "${YELLOW}   Installation: sudo apt install poppler-utils${NC}"
    DEPS_OK=false
fi

if pkg-config --exists sqlite3; then
    echo -e "${GREEN}✅ SQLite3 installé${NC}"
else
    echo -e "${RED}❌ SQLite3 manquant${NC}"
    echo -e "${YELLOW}   Installation: sudo apt install libsqlite3-dev${NC}"
    DEPS_OK=false
fi

if pkg-config --exists lept; then
    echo -e "${GREEN}✅ Leptonica installé${NC}"
else
    echo -e "${RED}❌ Leptonica manquant${NC}"
    echo -e "${YELLOW}   Installation: sudo apt install libleptonica-dev${NC}"
    DEPS_OK=false
fi

if [ "$DEPS_OK" = false ]; then
    echo -e "\n${RED}⚠️  Installez les dépendances manquantes avant de continuer${NC}"
    exit 1
fi

echo ""

# 3. Création des dossiers nécessaires
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "3️⃣  Création des dossiers"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

mkdir -p build data images
echo -e "${GREEN}✅ Dossiers créés/vérifiés${NC}"
echo ""

# 4. Compilation
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "4️⃣  Compilation"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo "🧹 Nettoyage..."
make clean

echo ""
echo "🔨 Compilation en cours..."
if make; then
    echo -e "\n${GREEN}✅ Compilation réussie !${NC}"
else
    echo -e "\n${RED}❌ Erreur de compilation${NC}"
    echo -e "${YELLOW}💡 Vérifiez les messages d'erreur ci-dessus${NC}"
    exit 1
fi

echo ""

# 5. Vérification de l'exécutable
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "5️⃣  Vérification de l'exécutable"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ -f "bulletin_scanner" ]; then
    SIZE=$(ls -lh bulletin_scanner | awk '{print $5}')
    echo -e "${GREEN}✅ Exécutable créé (taille: $SIZE)${NC}"
    echo -e "${GREEN}✅ Prêt à l'utilisation !${NC}"
else
    echo -e "${RED}❌ Exécutable non créé${NC}"
    exit 1
fi

echo ""

# 6. Test rapide
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "6️⃣  Résumé"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}✅ Tous les tests sont passés !${NC}"
echo ""
echo "📝 Pour utiliser le scanner :"
echo "   ./bulletin_scanner"
echo ""
echo "📁 Formats supportés :"
echo "   • PDF (nécessite poppler-utils)"
echo "   • PNG, JPG, TIFF, BMP"
echo ""
echo "💡 Conseil : Placez vos bulletins dans le dossier 'images/'"
echo ""
echo "╔═══════════════════════════════════════════════════╗"
echo "║  Compilation terminée avec succès ! 🎉            ║"
echo "╚═══════════════════════════════════════════════════╝"