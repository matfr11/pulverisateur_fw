#!/bin/bash
# Script de build automatisé pour le système pulvérisateur
# Usage: ./build_all.sh [avant|arriere|ecran|all] [clean]

set -e  # Arrêt en cas d'erreur

# Couleurs pour output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Fonction d'affichage
print_header() {
    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}================================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Vérifier que ESP-IDF est sourcé
check_idf() {
    if [ -z "$IDF_PATH" ]; then
        print_error "ESP-IDF n'est pas sourcé !"
        echo "Exécutez d'abord: . \$HOME/esp-idf/export.sh"
        exit 1
    fi
    print_success "ESP-IDF trouvé: $IDF_PATH"
}

# Build carte AVANT
build_avant() {
    print_header "BUILD CARTE AVANT"
    cd carte_relais
    
    if [ "$CLEAN" = "true" ]; then
        print_warning "Nettoyage complet..."
        idf.py fullclean
    fi
    
    echo "Configuration..."
    idf.py -D BOARD_TYPE=AVANT set-target esp32
    
    echo "Compilation..."
    idf.py -D BOARD_TYPE=AVANT build
    
    print_success "Carte AVANT compilée avec succès"
    echo "Binaire: carte_relais/build/pulverisateur_avant.bin"
    cd ..
}

# Build carte ARRIÈRE
build_arriere() {
    print_header "BUILD CARTE ARRIERE"
    cd carte_relais
    
    if [ "$CLEAN" = "true" ]; then
        print_warning "Nettoyage complet..."
        idf.py fullclean
    fi
    
    echo "Configuration..."
    idf.py -D BOARD_TYPE=ARRIERE set-target esp32
    
    echo "Compilation..."
    idf.py -D BOARD_TYPE=ARRIERE build
    
    print_success "Carte ARRIÈRE compilée avec succès"
    echo "Binaire: carte_relais/build/pulverisateur_arriere.bin"
    cd ..
}

# Build carte ÉCRAN
build_ecran() {
    print_header "BUILD CARTE ECRAN"
    
    if [ ! -d "carte_ecran" ]; then
        print_warning "Projet carte_ecran pas encore implémenté"
        return
    fi
    
    cd carte_ecran
    
    if [ "$CLEAN" = "true" ]; then
        print_warning "Nettoyage complet..."
        idf.py fullclean
    fi
    
    echo "Configuration..."
    idf.py set-target esp32p4
    
    echo "Compilation..."
    idf.py build
    
    print_success "Carte ÉCRAN compilée avec succès"
    cd ..
}

# Afficher tailles des binaires
show_sizes() {
    print_header "TAILLES DES BINAIRES"
    
    if [ -f "carte_relais/build/pulverisateur_avant.elf" ]; then
        echo -e "${YELLOW}Carte AVANT:${NC}"
        size carte_relais/build/pulverisateur_avant.elf
    fi
    
    if [ -f "carte_relais/build/pulverisateur_arriere.elf" ]; then
        echo -e "${YELLOW}Carte ARRIÈRE:${NC}"
        size carte_relais/build/pulverisateur_arriere.elf
    fi
}

# Main
main() {
    print_header "SYSTÈME PULVÉRISATEUR - BUILD"
    
    # Vérifier arguments
    TARGET=${1:-all}
    CLEAN=""
    
    if [ "$2" = "clean" ]; then
        CLEAN="true"
        print_warning "Mode nettoyage activé"
    fi
    
    # Vérifier environnement
    check_idf
    
    # Build selon cible
    case $TARGET in
        avant)
            build_avant
            ;;
        arriere)
            build_arriere
            ;;
        ecran)
            build_ecran
            ;;
        all)
            build_avant
            build_arriere
            build_ecran
            ;;
        *)
            print_error "Cible invalide: $TARGET"
            echo "Usage: $0 [avant|arriere|ecran|all] [clean]"
            exit 1
            ;;
    esac
    
    # Afficher résumé
    echo ""
    show_sizes
    
    print_header "BUILD TERMINÉ"
    print_success "Tous les builds ont réussi !"
    
    echo ""
    echo "Prochaines étapes:"
    echo "  1. Flash carte AVANT:   cd carte_relais && idf.py -p /dev/ttyUSB0 flash"
    echo "  2. Flash carte ARRIÈRE: cd carte_relais && idf.py -p /dev/ttyUSB1 flash"
    echo "  3. Moniteur:            idf.py -p /dev/ttyUSB0 monitor"
}

# Exécution
main "$@"
