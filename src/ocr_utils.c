#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>  // Pour strcasestr
#include "ocr_utils.h"

// ========== EXTRACTION DE TEXTE AMÉLIORÉE ==========

char* extraire_texte_image(const char* chemin_image) {
    TessBaseAPI *api = TessBaseAPICreate();
    
    // Initialisation avec le français
    if (TessBaseAPIInit3(api, NULL, "fra") != 0) {
        fprintf(stderr, "❌ Erreur d'initialisation Tesseract\n");
        fprintf(stderr, "💡 Assurez-vous que les données 'fra' sont installées\n");
        TessBaseAPIDelete(api);
        return NULL;
    }
    
    // Configuration optimale pour documents
    TessBaseAPISetPageSegMode(api, PSM_AUTO);
    TessBaseAPISetVariable(api, "tessedit_char_whitelist", 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzàâäéèêëïîôùûüÿçÀÂÄÉÈÊËÏÎÔÙÛÜŸÇ0123456789.,;:/-() ");
    
    printf("📂 Lecture du fichier: %s\n", chemin_image);
    
    PIX *image = NULL;
    char temp_path[256] = "/tmp/bulletin_temp.png";
    int is_pdf = 0;
    
    // Vérifier si c'est un PDF (insensible à la casse)
    if (strcasestr(chemin_image, ".pdf")) {
        is_pdf = 1;
        printf("📄 Détection PDF - Conversion en image...\n");
        
        // Commande pour convertir PDF en PNG (page unique)
        // Nécessite poppler-utils installé (sudo apt install poppler-utils)
        char cmd[512];
        snprintf(cmd, sizeof(cmd), 
                 "pdftoppm -png -singlefile -r 300 '%s' /tmp/bulletin_temp", 
                 chemin_image);
        
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, "❌ Erreur de conversion PDF (pdftoppm requis)\n");
            fprintf(stderr, "💡 Installez poppler-utils: sudo apt install poppler-utils\n");
            TessBaseAPIEnd(api);
            TessBaseAPIDelete(api);
            return NULL;
        }
        
        // Lire l'image convertie
        image = pixRead(temp_path);
        if (!image) {
            fprintf(stderr, "❌ Impossible de lire l'image convertie\n");
            unlink(temp_path);  // Nettoyer
            TessBaseAPIEnd(api);
            TessBaseAPIDelete(api);
            return NULL;
        }
    } else {
        // Lecture directe pour autres formats
        image = pixRead(chemin_image);
        if (!image) {
            fprintf(stderr, "❌ Impossible de lire: %s\n", chemin_image);
            fprintf(stderr, "💡 Formats supportés: JPG, PNG, TIFF, BMP, PDF (avec conversion)\n");
            TessBaseAPIEnd(api);
            TessBaseAPIDelete(api);
            return NULL;
        }
    }
    
    printf("✅ Image chargée (%d x %d pixels)\n", pixGetWidth(image), pixGetHeight(image));
    
    // Prétraitement de l'image pour améliorer l'OCR
    PIX *image_gray = pixConvertTo8(image, 0);
    pixDestroy(&image);
    
    PIX *image_enhanced = pixContrastNorm(NULL, image_gray, 50, 50, 130, 10, 10);
    if (image_enhanced) {
        pixDestroy(&image_gray);
        image_gray = image_enhanced;
    }
    
    // Seuillage adaptatif
    PIX *image_bin = NULL;
    PIX *pixth = NULL;
    if (pixOtsuAdaptiveThreshold(image_gray, 2000, 2000, 0, 0, 0.1, &pixth, &image_bin) == 0) {
        if (image_bin) {
            pixDestroy(&image_gray);
            image_gray = image_bin;
        }
        if (pixth) pixDestroy(&pixth);
    }
    printf("🔄 Extraction du texte en cours...\n");
    
    // Extraction du texte
    TessBaseAPISetImage2(api, image_gray);
    char *texte = TessBaseAPIGetUTF8Text(api);
    
    if (texte) {
        printf("✅ Texte extrait (%zu caractères)\n", strlen(texte));
    } else {
        fprintf(stderr, "❌ Échec de l'extraction\n");
    }
    
    // Nettoyage
    pixDestroy(&image_gray);
    TessBaseAPIEnd(api);
    TessBaseAPIDelete(api);
    
    // Si PDF, nettoyer le fichier temp
    if (is_pdf) {
        unlink(temp_path);
        printf("🧹 Fichier temporaire nettoyé\n");
    }
    
    return texte;
}

// ========== EXTRACTION DE NOMBRES ==========

float extraire_note_depuis_ligne(const char* ligne) {
    if (!ligne) return -1.0;
    
    char temp[500];
    strncpy(temp, ligne, 499);
    temp[499] = '\0';
    
    // Correction des erreurs OCR communes
    for (int i = 0; temp[i]; i++) {
        if (temp[i] == 'O' || temp[i] == 'o') {
            // Vérifier si c'est probablement un 0
            if (i > 0 && (isdigit(temp[i-1]) || temp[i-1] == ' ')) {
                if (i < strlen(temp)-1 && (isdigit(temp[i+1]) || temp[i+1] == '.' || temp[i+1] == ',')) {
                    temp[i] = '0';
                }
            }
        }
        if (temp[i] == 'l' || temp[i] == 'I') {
            if (i > 0 && isdigit(temp[i-1])) {
                temp[i] = '1';
            }
        }
    }
    
    char *p = temp;
    float note_trouvee = -1.0;
    
    while (*p) {
        if (isdigit(*p)) {
            char nombre_str[10];
            int idx = 0;
            
            // Extraire le nombre
            while (*p && (isdigit(*p) || *p == '.' || *p == ',') && idx < 9) {
                if (*p == ',') nombre_str[idx++] = '.';
                else nombre_str[idx++] = *p;
                p++;
            }
            nombre_str[idx] = '\0';
            
            float val = atof(nombre_str);
            
            // Vérifier que c'est bien une note
            if (val >= 0.0 && val <= 20.0) {
                note_trouvee = val;
                return note_trouvee; // Prendre la première note valide
            }
        } else {
            p++;
        }
    }
    
    return note_trouvee;
}

float extraire_coefficient(const char* ligne) {
    if (!ligne) return 1.0;
    
    char temp[500];
    strncpy(temp, ligne, 499);
    temp[499] = '\0';
    
    char *p = temp;
    int compteur = 0;
    float coef = 1.0;
    
    // Chercher le mot "coefficient" ou "coef"
    char *pos_coef = strstr(temp, "coef");
    if (!pos_coef) pos_coef = strstr(temp, "Coef");
    if (!pos_coef) pos_coef = strstr(temp, "COEF");
    
    if (pos_coef) {
        p = pos_coef;
    }
    
    while (*p) {
        if (isdigit(*p)) {
            char nombre_str[10];
            int idx = 0;
            
            while (*p && (isdigit(*p) || *p == '.' || *p == ',') && idx < 9) {
                if (*p == ',') nombre_str[idx++] = '.';
                else nombre_str[idx++] = *p;
                p++;
            }
            nombre_str[idx] = '\0';
            
            float val = atof(nombre_str);
            
            // Un coefficient est généralement entre 0.5 et 10
            if (val >= 0.5 && val <= 10.0) {
                if (pos_coef) {
                    return val; // Si on a trouvé "coef", c'est le bon
                }
                coef = val;
                break;
            }
        } else {
            p++;
        }
    }
    
    return coef;
}

// ========== DÉTECTION DE LIGNES DE MATIÈRES ==========

int est_ligne_matiere(const char* ligne) {
    if (!ligne || strlen(ligne) < 5) return 0;
    
    // Mots-clés spécifiques à ton bulletin
    const char* prefixes[] = {
        "CP3IL", "MCP3IL", "CPAIL", "MCPAIL", "CP3", "MCP3", NULL
    };
    
    for (int i = 0; prefixes[i] != NULL; i++) {
        if (strstr(ligne, prefixes[i])) {
            return 1;
        }
    }
    
    // Mots-clés généraux de matières
    const char* matieres[] = {
        "Mathématique", "Mathématiques", "Maths", "Français", "Anglais", 
        "Physique", "Chimie", "SVT", "Histoire", "Géographie",
        "Informatique", "Sciences", "Algèbre", "Analyse", "Géométrie",
        "Mécanique", "Électronique", "Énergétique", "Thermodynamique",
        "Expression", "Méthodologie", "Programmation", "EXCEL", "Python",
        "Base de données", "Réseau", "Système", NULL
    };
    
    for (int i = 0; matieres[i] != NULL; i++) {
        if (strstr(ligne, matieres[i])) {
            // Vérifier qu'il y a bien une note dans la ligne
            float note = extraire_note_depuis_ligne(ligne);
            if (note >= 0.0 && note <= 20.0) {
                return 1;
            }
        }
    }
    
    return 0;
}

void extraire_nom_matiere(const char* ligne, char* nom_matiere) {
    if (!ligne || !nom_matiere) return;
    
    char temp[500];
    strncpy(temp, ligne, 499);
    temp[499] = '\0';
    
    char *debut = temp;
    
    // Passer les espaces initiaux
    while (*debut && (*debut == ' ' || *debut == '\t')) debut++;
    
    // Si la ligne commence par un préfixe type CP3IL, sauter jusqu'au vrai nom
    if (strstr(debut, "CP3IL") || strstr(debut, "MCP3IL") || 
        strstr(debut, "CPAIL") || strstr(debut, "MCPAIL")) {
        debut = strstr(debut, " ");
        if (debut) {
            debut++;
            while (*debut == ' ') debut++;
        }
    }
    
    if (!debut || !*debut) {
        strcpy(nom_matiere, "Matière inconnue");
        return;
    }
    
    // Extraire le nom jusqu'au premier chiffre (qui est probablement la note)
    int i = 0;
    int trouve_lettre = 0;
    
    while (*debut && i < MAX_MATIERE - 1) {
        if (isalpha(*debut)) {
            trouve_lettre = 1;
        }
        
        // Arrêter si on trouve un chiffre après avoir trouvé des lettres
        if (trouve_lettre && isdigit(*debut) && i > 3) {
            break;
        }
        
        nom_matiere[i++] = *debut;
        debut++;
    }
    nom_matiere[i] = '\0';
    
    // Nettoyer les espaces en fin
    nettoyer_chaine(nom_matiere);
}

// ========== ANALYSE COMPLÈTE DU BULLETIN ==========

int analyser_bulletin_texte(const char* texte_ocr, Eleve* eleve) {
    if (!texte_ocr || !eleve) return 0;
    
    printf("\n╔════════════════════════════════════╗\n");
    printf("║   ANALYSE DU BULLETIN EN COURS    ║\n");
    printf("╚════════════════════════════════════╝\n\n");
    
    // Initialisation
    eleve->nombre_matieres = 0;
    eleve->id = 0;
    strcpy(eleve->date_bulletin, "2025-01-08");
    
    // Extraction du nom (à améliorer selon le format)
    strcpy(eleve->nom, "NOM_A_EXTRAIRE");
    strcpy(eleve->prenom, "PRENOM_A_EXTRAIRE");
    strcpy(eleve->classe, "CLASSE_A_EXTRAIRE");
    
    // Recherche intelligente du nom
    char *copie_nom = strdup(texte_ocr);
    char *ligne_nom = strtok(copie_nom, "\n");
    int ligne_num = 0;
    
    while (ligne_nom && ligne_num < 20) { // Chercher dans les 20 premières lignes
        nettoyer_chaine(ligne_nom);
        
        // Chercher "Nom:" ou "Élève:" ou "Étudiant:"
        if (strstr(ligne_nom, "Nom:") || strstr(ligne_nom, "lève:") || strstr(ligne_nom, "tudiant:")) {
            char *debut_nom = strchr(ligne_nom, ':');
            if (debut_nom) {
                debut_nom++;
                while (*debut_nom == ' ') debut_nom++;
                strncpy(eleve->nom, debut_nom, MAX_NOM - 1);
                nettoyer_chaine(eleve->nom);
            }
        }
        
        // Chercher "Prénom:"
        if (strstr(ligne_nom, "nom:")) {
            char *debut_prenom = strchr(ligne_nom, ':');
            if (debut_prenom) {
                debut_prenom++;
                while (*debut_prenom == ' ') debut_prenom++;
                strncpy(eleve->prenom, debut_prenom, MAX_PRENOM - 1);
                nettoyer_chaine(eleve->prenom);
            }
        }
        
        // Chercher "Classe:"
        if (strstr(ligne_nom, "Classe:")) {
            char *debut_classe = strchr(ligne_nom, ':');
            if (debut_classe) {
                debut_classe++;
                while (*debut_classe == ' ') debut_classe++;
                strncpy(eleve->classe, debut_classe, MAX_CLASSE - 1);
                nettoyer_chaine(eleve->classe);
            }
        }
        
        ligne_nom = strtok(NULL, "\n");
        ligne_num++;
    }
    free(copie_nom);
    
    printf("📋 Élève: %s %s\n", eleve->nom, eleve->prenom);
    printf("🏫 Classe: %s\n\n", eleve->classe);
    
    // Analyse des matières
    char *copie = strdup(texte_ocr);
    if (!copie) return 0;
    
    char *ligne = strtok(copie, "\n");
    int lignes_traitees = 0;
    
    printf("═══════════════════════════════════════════════\n");
    printf("  MATIÈRES DÉTECTÉES\n");
    printf("═══════════════════════════════════════════════\n\n");
    
    while (ligne != NULL && eleve->nombre_matieres < MAX_MATIERES) {
        nettoyer_chaine(ligne);
        
        if (strlen(ligne) < 5) {
            ligne = strtok(NULL, "\n");
            continue;
        }
        
        if (est_ligne_matiere(ligne)) {
            float note = extraire_note_depuis_ligne(ligne);
            
            if (note >= 0.0 && note <= 20.0) {
                char nom_temp[MAX_MATIERE];
                extraire_nom_matiere(ligne, nom_temp);
                
                // Filtrer les faux positifs
                if (strlen(nom_temp) > 3 && 
                    !strstr(nom_temp, "Crédits") && 
                    !strstr(nom_temp, "Total") &&
                    !strstr(nom_temp, "Moyenne") &&
                    !strstr(nom_temp, "Points")) {
                    
                    // Copier le nom de la matière
                    strncpy(eleve->matieres[eleve->nombre_matieres].nom_matiere, 
                           nom_temp, MAX_MATIERE - 1);
                    eleve->matieres[eleve->nombre_matieres].nom_matiere[MAX_MATIERE - 1] = '\0';
                    
                    // Assigner la note
                    eleve->matieres[eleve->nombre_matieres].note = note;
                    
                    // Extraire le coefficient
                    float coef = extraire_coefficient(ligne);
                    eleve->matieres[eleve->nombre_matieres].coefficient = coef;
                    
                    // Appréciation vide par défaut
                    strcpy(eleve->matieres[eleve->nombre_matieres].appreciation, "");
                    
                    printf("✅ %2d. %-40s | Note: %5.2f | Coef: %.1f\n",
                           eleve->nombre_matieres + 1,
                           eleve->matieres[eleve->nombre_matieres].nom_matiere,
                           note, coef);
                    
                    eleve->nombre_matieres++;
                    lignes_traitees++;
                }
            }
        }
        
        ligne = strtok(NULL, "\n");
    }
    
    free(copie);
    
    printf("\n═══════════════════════════════════════════════\n");
    printf("📊 Total: %d matière(s) extraite(s)\n", eleve->nombre_matieres);
    
    if (eleve->nombre_matieres > 0) {
        calculer_moyenne(eleve);
        printf("🎯 Moyenne générale: %.2f/20\n", eleve->moyenne_generale);
        printf("═══════════════════════════════════════════════\n\n");
        return 1;
    }
    
    printf("❌ Aucune matière détectée\n");
    printf("═══════════════════════════════════════════════\n\n");
    return 0;
}

// ========== SAISIE MANUELLE ==========

void saisir_bulletin_manuel(sqlite3 *db) {
    Eleve eleve;
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  SAISIE MANUELLE D'UN BULLETIN        ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    printf("👤 Nom de l'élève: ");
    fgets(eleve.nom, MAX_NOM, stdin);
    nettoyer_chaine(eleve.nom);
    
    printf("👤 Prénom: ");
    fgets(eleve.prenom, MAX_PRENOM, stdin);
    nettoyer_chaine(eleve.prenom);
    
    printf("🏫 Classe: ");
    fgets(eleve.classe, MAX_CLASSE, stdin);
    nettoyer_chaine(eleve.classe);
    
    printf("📅 Date (AAAA-MM-JJ): ");
    fgets(eleve.date_bulletin, 20, stdin);
    nettoyer_chaine(eleve.date_bulletin);
    
    printf("📚 Nombre de matières: ");
    scanf("%d", &eleve.nombre_matieres);
    getchar();
    
    if (eleve.nombre_matieres > MAX_MATIERES) {
        printf("⚠️  Limité à %d matières\n", MAX_MATIERES);
        eleve.nombre_matieres = MAX_MATIERES;
    }
    
    printf("\n");
    
    for (int i = 0; i < eleve.nombre_matieres; i++) {
        printf("─────────────────────────────────────────\n");
        printf("📖 Matière %d/%d\n", i + 1, eleve.nombre_matieres);
        printf("─────────────────────────────────────────\n");
        
        printf("  Nom: ");
        fgets(eleve.matieres[i].nom_matiere, MAX_MATIERE, stdin);
        nettoyer_chaine(eleve.matieres[i].nom_matiere);
        
        printf("  Note (/20): ");
        scanf("%f", &eleve.matieres[i].note);
        
        printf("  Coefficient: ");
        scanf("%f", &eleve.matieres[i].coefficient);
        getchar();
        
        printf("  Appréciation (optionnel): ");
        fgets(eleve.matieres[i].appreciation, 200, stdin);
        nettoyer_chaine(eleve.matieres[i].appreciation);
        
        printf("\n");
    }
    
    calculer_moyenne(&eleve);
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║         RÉCAPITULATIF                  ║\n");
    printf("╚════════════════════════════════════════╝\n");
    afficher_eleve(&eleve);
    
    printf("\n💾 Enregistrer dans la base ? (o/n): ");
    char choix;
    scanf("%c", &choix);
    getchar();
    
    if (choix == 'o' || choix == 'O') {
        if (inserer_eleve(db, &eleve)) {
            printf("✅ Bulletin enregistré avec succès!\n");
        } else {
            printf("❌ Erreur lors de l'enregistrement\n");
        }
    } else {
        printf("❌ Bulletin non enregistré\n");
    }
}