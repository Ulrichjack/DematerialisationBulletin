#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include "ocr_utils.h"
#include "database.h"

// ========== EXTRACTION OCR (INCHANGÉ) ==========

char* extraire_texte_image(const char* chemin_image) {
    TessBaseAPI *api = TessBaseAPICreate();
    
    if (TessBaseAPIInit3(api, NULL, "fra") != 0) {
        fprintf(stderr, "❌ Erreur d'initialisation Tesseract\n");
        TessBaseAPIDelete(api);
        return NULL;
    }
    
    TessBaseAPISetPageSegMode(api, PSM_AUTO);
    
    printf("📂 Lecture du fichier: %s\n", chemin_image);
    
    PIX *image = NULL;
    char temp_path[256] = "/tmp/bulletin_temp.png";
    int is_pdf = 0;
    
    if (strcasestr(chemin_image, ".pdf")) {
        is_pdf = 1;
        printf("📄 Conversion PDF...\n");
        
        char cmd[512];
        snprintf(cmd, sizeof(cmd), 
                 "pdftoppm -png -singlefile -r 300 '%s' /tmp/bulletin_temp 2>/dev/null", 
                 chemin_image);
        
        system(cmd);
        image = pixRead(temp_path);
    } else {
        image = pixRead(chemin_image);
    }
    
    if (!image) {
        fprintf(stderr, "❌ Impossible de lire l'image\n");
        TessBaseAPIEnd(api);
        TessBaseAPIDelete(api);
        return NULL;
    }
    
    printf("✅ Image chargée (%d x %d pixels)\n", pixGetWidth(image), pixGetHeight(image));
    
    PIX *gray = pixConvertTo8(image, 0);
    pixDestroy(&image);
    
    printf("🔄 Extraction OCR...\n");
    
    TessBaseAPISetImage2(api, gray);
    char *texte = TessBaseAPIGetUTF8Text(api);
    
    if (texte) {
        printf("✅ Texte extrait (%zu caractères)\n", strlen(texte));
    }
    
    pixDestroy(&gray);
    TessBaseAPIEnd(api);
    TessBaseAPIDelete(api);
    
    if (is_pdf) unlink(temp_path);
    
    return texte;
}

// ========== UTILITAIRES ==========

void extraire_info_apres_label(const char* texte, const char* label, char* destination, int max_len) {
    if (!texte || !label || !destination) return;
    
    const char* pos = strcasestr(texte, label);
    if (!pos) return;

    pos += strlen(label);
    while (*pos && (*pos == ' ' || *pos == ':')) pos++;
    
    int i = 0;
    while (*pos && *pos != '\n' && i < max_len - 1) {
        destination[i++] = *pos++;
    }
    destination[i] = '\0';
    nettoyer_chaine(destination);
}

// ========== DÉTECTION AMÉLIORÉE DES CODES ==========

int est_code_matiere(const char* str) {
    if (!str || strlen(str) < 4) return 0;
    
    // Format: MCP + chiffres (ex: MCP31L1101)
    if (strncmp(str, "MCP", 3) == 0 && strlen(str) >= 8) {
        int nb_chiffres = 0;
        for (int i = 3; i < strlen(str) && i < 15; i++) {
            if (isdigit(str[i])) nb_chiffres++;
        }
        return nb_chiffres >= 4;
    }
    
    // Format: CP + chiffres (ex: CP31L1101 ou CP3IL1104)
    if (strncmp(str, "CP", 2) == 0 && strlen(str) >= 7) {
        int nb_chiffres = 0;
        for (int i = 2; i < strlen(str) && i < 15; i++) {
            if (isdigit(str[i])) nb_chiffres++;
        }
        return nb_chiffres >= 4;
    }
    
    return 0;
}

// ========== DÉTECTION MULTI-CRITÈRES DU SEMESTRE 2 ==========

int est_debut_semestre2(const char* ligne, int* mcp_vus, int nb_mcp_vus) {
    // Critère 1 : Mots-clés explicites
    if (strcasestr(ligne, "Semestre 2") || strcasestr(ligne, "Semester 2") ||
        strcasestr(ligne, "Résultats Semestre 2") || strcasestr(ligne, "Semester 2 Results") ||
        strcasestr(ligne, "SEMESTRE 2") || strcasestr(ligne, "S2") ||
        strcasestr(ligne, "2nd Semester") || strcasestr(ligne, "Second Semester")) {
        return 1;
    }
    
    // Critère 2 : Détection de MCP31L105+ (après avoir vu MCP31L1101-104)
    // Si on a déjà vu au moins 4 MCP du S1, et qu'on voit MCP31L105 ou supérieur
    if (nb_mcp_vus >= 4) {
        char code[50];
        char *token = strdup(ligne);
        char *tok = strtok(token, " \t|");
        
        while (tok) {
            if (strncmp(tok, "MCP31L1", 7) == 0 && strlen(tok) >= 10) {
                strncpy(code, tok, sizeof(code) - 1);
                code[sizeof(code) - 1] = '\0';
                
                // Extraire le numéro (ex: MCP31L1105 -> 105)
                if (strlen(code) >= 10 && isdigit(code[7]) && isdigit(code[8]) && isdigit(code[9])) {
                    int num = atoi(&code[7]);
                    
                    // Si >= 105, c'est probablement S2
                    if (num >= 105 && num <= 110) {
                        free(token);
                        return 1;
                    }
                }
            }
            tok = strtok(NULL, " \t|");
        }
        free(token);
    }
    
    return 0;
}

// ========== STRUCTURE MATIÈRE ==========

typedef struct {
    char code[50];
    char nom[MAX_MATIERE];
    float note;
    float coef;
    int semestre;
    char type[10];
    int est_valide;
    char module_parent[50];
} InfoMatiere;

// ========== EXTRACTION MATIÈRE AMÉLIORÉE ==========

int extraire_matiere_ligne(const char* ligne, InfoMatiere* info, int semestre_actuel, const char* module_parent) {
    if (!ligne || !info || strlen(ligne) < 10) return 0;
    
    memset(info, 0, sizeof(InfoMatiere));
    info->note = -1;
    info->coef = 1.0;
    info->semestre = semestre_actuel;
    info->est_valide = 0;
    
    if (module_parent) {
        strncpy(info->module_parent, module_parent, sizeof(info->module_parent) - 1);
    }
    
    // Détecter validation
    if (strcasestr(ligne, "Validé") || strcasestr(ligne, "Valide") ||
        strcasestr(ligne, "Assez Bien") || strcasestr(ligne, "Bien") || 
        strcasestr(ligne, "Très Bien") || strcasestr(ligne, "Tres Bien") ||
        strcasestr(ligne, "Passable")) {
        info->est_valide = 1;
    }
    
    // Chercher code matière
    char copie[1024];
    strncpy(copie, ligne, sizeof(copie) - 1);
    copie[sizeof(copie) - 1] = '\0';
    
    char *token = strtok(copie, " \t|");
    while (token) {
        if (est_code_matiere(token)) {
            strncpy(info->code, token, sizeof(info->code) - 1);
            strcpy(info->type, strncmp(token, "MCP", 3) == 0 ? "MCP" : "CP");
            break;
        }
        token = strtok(NULL, " \t|");
    }
    
    if (strlen(info->code) == 0) return 0;
    
    // Extraire nom et nombres
    const char *apres_code = strstr(ligne, info->code);
    if (!apres_code) return 0;
    
    apres_code += strlen(info->code);
    while (*apres_code == ' ' || *apres_code == '\t') apres_code++;
    
    char temp_nom[MAX_MATIERE] = "";
    float nombres[20];
    int nb_nombres = 0;
    
    char reste[512];
    strncpy(reste, apres_code, sizeof(reste) - 1);
    reste[sizeof(reste) - 1] = '\0';
    
    char *tok = strtok(reste, " \t|");
    int nom_termine = 0;
    
    while (tok && nb_nombres < 20) {
        for (int i = 0; tok[i]; i++) {
            if (tok[i] == ',') tok[i] = '.';
        }
        
        char *endptr;
        float val = strtof(tok, &endptr);
        
        if (*endptr == '\0' && val >= 0.0 && val <= 100.0) {
            nombres[nb_nombres++] = val;
            nom_termine = 1;
        } else if (!nom_termine && strlen(tok) > 1) {
            if (strcasecmp(tok, "Validé") != 0 && strcasecmp(tok, "Valide") != 0 &&
                strcasecmp(tok, "Passable") != 0 && strcasecmp(tok, "Assez") != 0 &&
                strcasecmp(tok, "Bien") != 0 && strcasecmp(tok, "Très") != 0 &&
                strcasecmp(tok, "Tres") != 0) {
                if (strlen(temp_nom) > 0) strcat(temp_nom, " ");
                strncat(temp_nom, tok, sizeof(temp_nom) - strlen(temp_nom) - 1);
            }
        }
        
        tok = strtok(NULL, " \t|");
    }
    
    if (strlen(temp_nom) > 0) {
        strncpy(info->nom, temp_nom, sizeof(info->nom) - 1);
    } else {
        strcpy(info->nom, "Sans nom");
    }
    
    // Analyser nombres : COEF NOTE (généralement)
    if (nb_nombres >= 2) {
        // Heuristique : coef entre 0.5-15, note entre 0-20
        if (nombres[0] >= 0.5 && nombres[0] <= 15.0 && nombres[1] >= 0.0 && nombres[1] <= 20.0) {
            info->coef = nombres[0];
            info->note = nombres[1];
        } else if (nombres[1] >= 0.5 && nombres[1] <= 15.0 && nombres[0] >= 0.0 && nombres[0] <= 20.0) {
            info->note = nombres[0];
            info->coef = nombres[1];
        } else {
            info->coef = nombres[0];
            info->note = nombres[1];
        }
    } else if (nb_nombres == 1) {
        info->note = nombres[0];
        info->coef = 1.0;
    }
    
    if (info->note < 0.0 || info->note > 20.0) info->note = -1;
    if (info->coef < 0.5 || info->coef > 15.0) info->coef = 1.0;
    
    return (strlen(info->code) > 0 && info->note >= 0.0);
}

// ========== ANALYSE PRINCIPALE ULTRA-ROBUSTE ==========

int analyser_bulletin_texte(const char* texte_ocr, Eleve* eleve) {
    if (!texte_ocr || !eleve) return 0;
    
    printf("\n╔════════════════════════════════════╗\n");
    printf("║   ANALYSE DU BULLETIN EN COURS    ║\n");
    printf("╚════════════════════════════════════╝\n\n");
    
    char *texte_copie = strdup(texte_ocr);
    if (!texte_copie) return 0;
    
    memset(eleve, 0, sizeof(Eleve));
    strcpy(eleve->date_bulletin, "2025-01-08");
    strcpy(eleve->matricule, "N/A");
    strcpy(eleve->niveau, "N/A");
    strcpy(eleve->annee_academique, "N/A");
    strcpy(eleve->lieu_naissance, "N/A");
    strcpy(eleve->date_naissance, "N/A");
    
    // ========== EXTRACTION INFORMATIONS ==========
    
    char temp[512];
    
    // Nom
    extraire_info_apres_label(texte_copie, "Last & First Name", temp, sizeof(temp));
    if (strlen(temp) == 0) extraire_info_apres_label(texte_copie, "Noms et prénoms", temp, sizeof(temp));
    
    if (strlen(temp) > 0) {
        char *p = strcasestr(temp, "Student");
        if (p) *p = '\0';
        p = strcasestr(temp, "Matricule");
        if (p) *p = '\0';
        nettoyer_chaine(temp);
        
        char *espace = strchr(temp, ' ');
        if (espace) {
            *espace = '\0';
            strncpy(eleve->nom, temp, MAX_NOM - 1);
            strncpy(eleve->prenom, espace + 1, MAX_PRENOM - 1);
        } else {
            strncpy(eleve->nom, temp, MAX_NOM - 1);
        }
    }
    
    // Matricule
    extraire_info_apres_label(texte_copie, "Student ID", eleve->matricule, MAX_MATRICULE);
    if (strlen(eleve->matricule) == 0 || strcmp(eleve->matricule, "N/A") == 0) {
        extraire_info_apres_label(texte_copie, "Matricule", eleve->matricule, MAX_MATRICULE);
    }
    char *p = strchr(eleve->matricule, ' ');
    if (p) *p = '\0';
    nettoyer_chaine(eleve->matricule);
    
    // Date naissance
    extraire_info_apres_label(texte_copie, "Date of birth", eleve->date_naissance, 20);
    if (strlen(eleve->date_naissance) == 0) {
        extraire_info_apres_label(texte_copie, "Date de naissance", eleve->date_naissance, 20);
    }
    p = strcasestr(eleve->date_naissance, "Place");
    if (p) *p = '\0';
    p = strcasestr(eleve->date_naissance, "Lieu");
    if (p) *p = '\0';
    nettoyer_chaine(eleve->date_naissance);
    
    // Lieu naissance
    extraire_info_apres_label(texte_copie, "Place of birth", eleve->lieu_naissance, MAX_LIEU);
    if (strlen(eleve->lieu_naissance) == 0) {
        extraire_info_apres_label(texte_copie, "Lieu de naissance", eleve->lieu_naissance, MAX_LIEU);
    }
    p = strcasestr(eleve->lieu_naissance, "Cycle");
    if (p) *p = '\0';
    p = strcasestr(eleve->lieu_naissance, "Level");
    if (p) *p = '\0';
    nettoyer_chaine(eleve->lieu_naissance);
    
    // Classe/Spécialité
    extraire_info_apres_label(texte_copie, "Specialty", eleve->classe, MAX_CLASSE);
    if (strlen(eleve->classe) == 0) {
        extraire_info_apres_label(texte_copie, "Spécialité", eleve->classe, MAX_CLASSE);
    }
    p = strcasestr(eleve->classe, "Academic");
    if (p) *p = '\0';
    p = strcasestr(eleve->classe, "Année");
    if (p) *p = '\0';
    nettoyer_chaine(eleve->classe);
    
    // Année académique
    extraire_info_apres_label(texte_copie, "Academic Year", eleve->annee_academique, MAX_ANNEE);
    if (strlen(eleve->annee_academique) == 0) {
        extraire_info_apres_label(texte_copie, "Année Académique", eleve->annee_academique, MAX_ANNEE);
    }
    nettoyer_chaine(eleve->annee_academique);
    
    printf("═══════════════════════════════════════════════\n");
    printf("  INFORMATIONS EXTRAITES\n");
    printf("═══════════════════════════════════════════════\n");
    printf("👤 Nom complet       : %s %s\n", eleve->nom, eleve->prenom);
    printf("🎓 Matricule         : %s\n", eleve->matricule);
    printf("🎂 Date naissance    : %s\n", eleve->date_naissance);
    printf("🌍 Lieu naissance    : %s\n", eleve->lieu_naissance);
    printf("🏫 Classe/Spécialité : %s\n", eleve->classe);
    printf("📅 Année académique  : %s\n", eleve->annee_academique);
    printf("═══════════════════════════════════════════════\n\n");
    
    // ========== EXTRACTION MATIÈRES AVEC DÉTECTION ROBUSTE S1/S2 ==========
    
    printf("═══════════════════════════════════════════════\n");
    printf("  EXTRACTION DES MATIÈRES PAR SEMESTRE\n");
    printf("═══════════════════════════════════════════════\n\n");
    
    char *ptr = texte_copie;
    int semestre_actuel = 1;
    char module_courant[50] = "";
    
    // Tracking des MCP vus pour détecter S2
    int mcp_codes_s1[20];
    int nb_mcp_s1 = 0;
    
    int nb_modules_s1 = 0, nb_modules_s2 = 0;
    int nb_matieres_s1 = 0, nb_matieres_s2 = 0;
    
    // Calcul position dans le texte
    int position_actuelle = 0;
    int longueur_totale = strlen(texte_copie);
    
    while (*ptr && eleve->nombre_matieres < MAX_MATIERES) {
        char *fin_ligne = strchr(ptr, '\n');
        if (!fin_ligne) fin_ligne = ptr + strlen(ptr);
        
        int len = fin_ligne - ptr;
        position_actuelle += len + 1;
        
        if (len > 0 && len < 1000) {
            char ligne[1024];
            strncpy(ligne, ptr, len);
            ligne[len] = '\0';
            nettoyer_chaine(ligne);
            
            // DÉTECTION SEMESTRE 2 (multi-critères)
            if (semestre_actuel == 1 && est_debut_semestre2(ligne, mcp_codes_s1, nb_mcp_s1)) {
                semestre_actuel = 2;
                module_courant[0] = '\0';
                printf("\n🔵 ════════════════════════════════════\n");
                printf("   PASSAGE AU SEMESTRE 2 DÉTECTÉ !\n");
                printf("   ════════════════════════════════════\n\n");
            }
            
            // Heuristique supplémentaire : si > 60% du texte et on voit encore des MCP
            if (semestre_actuel == 1 && nb_mcp_s1 >= 4 && 
                position_actuelle > (longueur_totale * 60 / 100)) {
                if (strstr(ligne, "MCP31L1") != NULL) {
                    semestre_actuel = 2;
                    module_courant[0] = '\0';
                    printf("\n🟡 ════════════════════════════════════\n");
                    printf("   S2 DÉTECTÉ (position + MCP trouvé)\n");
                    printf("   ════════════════════════════════════\n\n");
                }
            }
            
            // Extraction matière
            if (strlen(ligne) >= 10) {
                InfoMatiere info;
                if (extraire_matiere_ligne(ligne, &info, semestre_actuel, 
                                          strlen(module_courant) > 0 ? module_courant : NULL)) {
                    
                    if (strcmp(info.type, "MCP") == 0) {
                        strcpy(module_courant, info.code);
                        
                        // Enregistrer MCP S1 pour détection S2
                        if (semestre_actuel == 1 && nb_mcp_s1 < 20) {
                            mcp_codes_s1[nb_mcp_s1++] = atoi(&info.code[7]); // Extraire numéro
                        }
                        
                        if (semestre_actuel == 1) nb_modules_s1++;
                        else nb_modules_s2++;
                        
                        printf("📦 MODULE S%d: %s - %s (Coef: %.1f, Moy: %.2f) %s\n", 
                               semestre_actuel, info.code, info.nom, info.coef, info.note,
                               info.est_valide ? "✅" : "❌");
                    } else {
                        Matiere *m = &eleve->matieres[eleve->nombre_matieres];
                        
                        if (strlen(module_courant) > 0) {
                            snprintf(m->nom_matiere, MAX_MATIERE - 1, "[S%d-%s|%s] %s - %s", 
                                    info.semestre, info.type, module_courant,
                                    info.code, info.nom);
                        } else {
                            snprintf(m->nom_matiere, MAX_MATIERE - 1, "[S%d-%s] %s - %s", 
                                    info.semestre, info.type, info.code, info.nom);
                        }
                        
                        m->nom_matiere[MAX_MATIERE - 1] = '\0';
                        m->note = info.note;
                        m->coefficient = info.coef;
                        strcpy(m->appreciation, info.est_valide ? "Validé" : "");
                        
                        if (semestre_actuel == 1) nb_matieres_s1++;
                        else nb_matieres_s2++;
                        
                        printf("  📝 %2d. %-60s | Note: %5.2f | Coef: %.1f %s\n",
                               eleve->nombre_matieres + 1, m->nom_matiere, m->note, m->coefficient,
                               info.est_valide ? "✅" : "");
                        
                        eleve->nombre_matieres++;
                    }
                }
            }
        }
        
        ptr = (*fin_ligne == '\n') ? fin_ligne + 1 : fin_ligne;
    }
    
    free(texte_copie);
    
    printf("\n═══════════════════════════════════════════════\n");
    printf("📊 RÉSULTATS D'EXTRACTION\n");
    printf("═══════════════════════════════════════════════\n");
    printf("📘 Semestre 1: %d cours + %d modules\n", nb_matieres_s1, nb_modules_s1);
    
    if (nb_matieres_s2 > 0 || nb_modules_s2 > 0) {
        printf("📗 Semestre 2: %d cours + %d modules\n", nb_matieres_s2, nb_modules_s2);
    }
    
    printf("📊 Total: %d matière(s)\n", eleve->nombre_matieres);
    printf("═══════════════════════════════════════════════\n\n");
    
    if (eleve->nombre_matieres > 0) {
        calculer_moyenne(eleve);
        printf("🎯 Moyenne générale: %.2f/20\n\n", eleve->moyenne_generale);
        return 1;
    }
    
    printf("❌ Aucune matière détectée\n\n");
    return 0;
}

// ========== SAISIE MANUELLE (INCHANGÉ) ==========

void saisir_bulletin_manuel(sqlite3 *db) {
    Eleve eleve;
    
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║  SAISIE MANUELLE D'UN BULLETIN        ║\n");
    printf("╚═══════════════════════════════════════╝\n\n");
    
    strcpy(eleve.matricule, "N/A");
    strcpy(eleve.niveau, "N/A");
    strcpy(eleve.annee_academique, "N/A");
    strcpy(eleve.lieu_naissance, "N/A");
    strcpy(eleve.date_naissance, "N/A");
    
    printf("👤 Nom: ");
    fgets(eleve.nom, MAX_NOM, stdin);
    nettoyer_chaine(eleve.nom);
    
    printf("👤 Prénom: ");
    fgets(eleve.prenom, MAX_PRENOM, stdin);
    nettoyer_chaine(eleve.prenom);
    
    printf("🎓 Matricule: ");
    fgets(eleve.matricule, MAX_MATRICULE, stdin);
    nettoyer_chaine(eleve.matricule);
    
    printf("🏫 Classe: ");
    fgets(eleve.classe, MAX_CLASSE, stdin);
    nettoyer_chaine(eleve.classe);
    
    printf("📅 Année (2023-2024): ");
    fgets(eleve.annee_academique, MAX_ANNEE, stdin);
    nettoyer_chaine(eleve.annee_academique);
    
    printf("📅 Date (AAAA-MM-JJ): ");
    fgets(eleve.date_bulletin, 20, stdin);
    nettoyer_chaine(eleve.date_bulletin);
    
    printf("📚 Nombre de matières: ");
    scanf("%d", &eleve.nombre_matieres);
    getchar();
    
    if (eleve.nombre_matieres > MAX_MATIERES) {
        eleve.nombre_matieres = MAX_MATIERES;
    }
    
    for (int i = 0; i < eleve.nombre_matieres; i++) {
        printf("\n📖 Matière %d/%d\n", i + 1, eleve.nombre_matieres);
        
        printf("  Code: ");
        char code[50];
        fgets(code, sizeof(code), stdin);
        nettoyer_chaine(code);
        
        printf("  Nom: ");
        char nom[MAX_MATIERE];
        fgets(nom, MAX_MATIERE, stdin);
        nettoyer_chaine(nom);
        
        snprintf(eleve.matieres[i].nom_matiere, MAX_MATIERE - 1, "%s - %s", code, nom);
        
        printf("  Note: ");
        scanf("%f", &eleve.matieres[i].note);
        
        printf("  Coef: ");
        scanf("%f", &eleve.matieres[i].coefficient);
        getchar();
        
        strcpy(eleve.matieres[i].appreciation, "");
    }
    
    calculer_moyenne(&eleve);
    afficher_eleve(&eleve);
    
    printf("\n💾 Enregistrer ? (o/n): ");
    char choix;
    scanf("%c", &choix);
    getchar();
    
    if (choix == 'o' || choix == 'O') {
        if (inserer_eleve(db, &eleve)) {
            printf("✅ Enregistré!\n");
        }
    }
}