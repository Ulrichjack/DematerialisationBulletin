#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include "ocr_utils.h"
#include "database.h"

// ========== EXTRACTION SIMPLE ET EFFICACE ==========

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

// ========== FONCTIONS UTILITAIRES ==========

void nettoyer_texte_ocr(char *texte) {
    if (!texte) return;
    
    char *p = texte;
    char *dest = texte;
    int espace_precedent = 0;
    
    while (*p) {
        if (isspace(*p)) {
            if (!espace_precedent) {
                *dest++ = ' ';
                espace_precedent = 1;
            }
            p++;
        } else {
            *dest++ = *p++;
            espace_precedent = 0;
        }
    }
    *dest = '\0';
    nettoyer_chaine(texte);
}

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

// ========== DÉTECTION DES CODES - ULTRA SIMPLE ==========

int est_code_matiere(const char* str) {
    if (!str || strlen(str) < 4) return 0;
    
    // MCP suivi de chiffres
    if (strncmp(str, "MCP", 3) == 0) {
        for (int i = 3; i < strlen(str); i++) {
            if (isdigit(str[i])) return 1;
        }
    }
    
    // CP suivi de chiffres
    if (strncmp(str, "CP", 2) == 0 && strlen(str) > 4) {
        for (int i = 2; i < strlen(str); i++) {
            if (isdigit(str[i])) return 1;
        }
    }
    
    return 0;
}

// ========== STRUCTURE POUR MODULE ==========

typedef struct {
    char code_module[50];
    char nom_module[MAX_MATIERE];
    int nb_matieres;
    char codes_cp[20][50];  // Codes des CP dans ce module
    float notes_cp[20];
    float coefs_cp[20];
    float moyenne_module;
    float coef_module;
    int semestre;
    int est_valide;
} Module;

// ========== EXTRACTION DIRECTE - NOUVELLE APPROCHE ==========

typedef struct {
    char code[50];
    char nom[MAX_MATIERE];
    float note;
    float coef;
    int semestre;
    char type[10]; // "MCP" ou "CP"
    int est_valide;
    char module_parent[50]; // Pour les CP, code du MCP parent
} InfoMatiere;

int extraire_matiere_ligne(const char* ligne, InfoMatiere* info, int semestre_actuel, const char* module_parent) {
    if (!ligne || !info) return 0;
    
    memset(info, 0, sizeof(InfoMatiere));
    info->note = -1;
    info->coef = 1.0;
    info->semestre = semestre_actuel;
    info->est_valide = 0;
    
    if (module_parent) {
        strncpy(info->module_parent, module_parent, sizeof(info->module_parent) - 1);
    }
    
    char copie[1024];
    strncpy(copie, ligne, sizeof(copie) - 1);
    copie[sizeof(copie) - 1] = '\0';
    
    // Détecter si validé
    if (strcasestr(ligne, "Validé") || strcasestr(ligne, "Assez Bien") || 
        strcasestr(ligne, "Bien") || strcasestr(ligne, "Très Bien")) {
        info->est_valide = 1;
    }
    
    // Trouver le code de matière
    char *token = strtok(copie, " \t");
    while (token) {
        if (est_code_matiere(token)) {
            strncpy(info->code, token, sizeof(info->code) - 1);
            
            if (strncmp(token, "MCP", 3) == 0) {
                strcpy(info->type, "MCP");
            } else {
                strcpy(info->type, "CP");
            }
            break;
        }
        token = strtok(NULL, " \t");
    }
    
    if (strlen(info->code) == 0) return 0;
    
    // Récupérer le reste
    char reste[512] = "";
    char *p = strstr(ligne, info->code);
    if (p) {
        p += strlen(info->code);
        while (*p == ' ' || *p == '\t') p++;
        strncpy(reste, p, sizeof(reste) - 1);
    }
    
    // Extraire nom et nombres
    char temp_nom[MAX_MATIERE] = "";
    float nombres[20];
    int nb_nombres = 0;
    
    char *tok = strtok(reste, " \t|");
    int nom_fini = 0;
    
    while (tok && nb_nombres < 20) {
        // Remplacer virgule par point
        for (int i = 0; tok[i]; i++) {
            if (tok[i] == ',') tok[i] = '.';
        }
        
        char *endptr;
        float val = strtof(tok, &endptr);
        
        // Si c'est un nombre valide
        if (*endptr == '\0' && (val >= 0.0 && val <= 100.0)) {
            nombres[nb_nombres++] = val;
            nom_fini = 1;
        }
        // Sinon c'est du texte
        else if (!nom_fini && strlen(tok) > 1) {
            // Ignorer certains mots clés
            if (strcasecmp(tok, "Validé") != 0 && 
                strcasecmp(tok, "Passable") != 0 &&
                strcasecmp(tok, "Assez") != 0 &&
                strcasecmp(tok, "Bien") != 0 &&
                strcasecmp(tok, "Très") != 0) {
                if (strlen(temp_nom) > 0) strcat(temp_nom, " ");
                strncat(temp_nom, tok, sizeof(temp_nom) - strlen(temp_nom) - 1);
            }
        }
        
        tok = strtok(NULL, " \t|");
    }
    
    if (strlen(temp_nom) > 0) {
        strncpy(info->nom, temp_nom, sizeof(info->nom) - 1);
    } else {
        strcpy(info->nom, "Matière");
    }
    
    // Analyser les nombres: COEF puis NOTE
    for (int i = 0; i < nb_nombres; i++) {
        if (info->coef == 1.0 && nombres[i] >= 0.5 && nombres[i] <= 15.0) {
            info->coef = nombres[i];
        }
        else if (info->note < 0.0 && nombres[i] >= 0.0 && nombres[i] <= 20.0) {
            info->note = nombres[i];
        }
    }
    
    if (nb_nombres == 1 && info->note < 0.0 && nombres[0] >= 0.0 && nombres[0] <= 20.0) {
        info->note = nombres[0];
        info->coef = 1.0;
    }
    
    return (strlen(info->code) > 0 && info->note >= 0.0);
}

// ========== ANALYSE PRINCIPALE AVEC GESTION MODULES ==========

int analyser_bulletin_texte(const char* texte_ocr, Eleve* eleve) {
    if (!texte_ocr || !eleve) return 0;
    
    printf("\n╔════════════════════════════════════╗\n");
    printf("║   ANALYSE DU BULLETIN EN COURS    ║\n");
    printf("╚════════════════════════════════════╝\n\n");
    
    char *texte_copie = strdup(texte_ocr);
    if (!texte_copie) return 0;
    
    memset(eleve, 0, sizeof(Eleve));
    strcpy(eleve->date_bulletin, "2025-01-08");
    
    // ========== EXTRACTION INFORMATIONS - CORRIGÉE ==========
    
    char temp_nom[MAX_NOM * 2] = "";
    
    // Nom
    extraire_info_apres_label(texte_copie, "Last & First Name", temp_nom, sizeof(temp_nom));
    if (strlen(temp_nom) == 0) {
        extraire_info_apres_label(texte_copie, "Noms et prénoms", temp_nom, sizeof(temp_nom));
    }
    
    if (strlen(temp_nom) > 0) {
        char *p = strcasestr(temp_nom, "Student");
        if (p) *p = '\0';
        p = strcasestr(temp_nom, "Matricule");
        if (p) *p = '\0';
        nettoyer_chaine(temp_nom);
        
        char *espace = strchr(temp_nom, ' ');
        if (espace) {
            *espace = '\0';
            strncpy(eleve->nom, temp_nom, MAX_NOM - 1);
            strncpy(eleve->prenom, espace + 1, MAX_PRENOM - 1);
        } else {
            strncpy(eleve->nom, temp_nom, MAX_NOM - 1);
        }
    }
    
    // Matricule
    extraire_info_apres_label(texte_copie, "Student ID", eleve->matricule, MAX_MATRICULE);
    if (strlen(eleve->matricule) == 0) {
        extraire_info_apres_label(texte_copie, "Matricule", eleve->matricule, MAX_MATRICULE);
    }
    char *p = strchr(eleve->matricule, ' ');
    if (p) *p = '\0';
    nettoyer_chaine(eleve->matricule);
    
    // Date de naissance - CORRIGÉE
    extraire_info_apres_label(texte_copie, "Date of birth", eleve->date_naissance, 20);
    if (strlen(eleve->date_naissance) == 0) {
        extraire_info_apres_label(texte_copie, "Date de naissance", eleve->date_naissance, 20);
    }
    p = strcasestr(eleve->date_naissance, "Place");
    if (p) *p = '\0';
    p = strcasestr(eleve->date_naissance, "Lieu");
    if (p) *p = '\0';
    nettoyer_chaine(eleve->date_naissance);
    
    // Lieu de naissance - CORRIGÉ
    extraire_info_apres_label(texte_copie, "Place of birth", eleve->lieu_naissance, MAX_LIEU);
    if (strlen(eleve->lieu_naissance) == 0) {
        extraire_info_apres_label(texte_copie, "Lieu de naissance", eleve->lieu_naissance, MAX_LIEU);
    }
    p = strcasestr(eleve->lieu_naissance, "Cycle");
    if (p) *p = '\0';
    p = strcasestr(eleve->lieu_naissance, "Level");
    if (p) *p = '\0';
    nettoyer_chaine(eleve->lieu_naissance);
    
    // Spécialité/Classe - CORRECTION AVANCÉE
    // Chercher "Spécialité" ligne complète
    char *ligne_spec = strstr(texte_copie, "Spécialité");
    if (!ligne_spec) ligne_spec = strstr(texte_copie, "Specialty");
    
    if (ligne_spec) {
        char *debut = ligne_spec;
        while (debut > texte_copie && *(debut-1) != '\n') debut--;
        char *fin = strchr(ligne_spec, '\n');
        if (!fin) fin = ligne_spec + strlen(ligne_spec);
        
        int len = fin - debut;
        if (len > 0 && len < 500) {
            char ligne_temp[512];
            strncpy(ligne_temp, debut, len);
            ligne_temp[len] = '\0';
            
            // Extraire entre "Spécialité" et "Année"
            char *apres_label = strstr(ligne_temp, "Spécialité");
            if (!apres_label) apres_label = strstr(ligne_temp, "Specialty");
            
            if (apres_label) {
                apres_label = strchr(apres_label, ' ');
                if (apres_label) {
                    apres_label++;
                    char *fin_classe = strstr(apres_label, "Année");
                    if (!fin_classe) fin_classe = strstr(apres_label, "Academic");
                    
                    if (fin_classe) {
                        int len_classe = fin_classe - apres_label;
                        if (len_classe > 0 && len_classe < MAX_CLASSE) {
                            strncpy(eleve->classe, apres_label, len_classe);
                            eleve->classe[len_classe] = '\0';
                            nettoyer_chaine(eleve->classe);
                        }
                    }
                }
            }
        }
    }
    
    // Année académique - CORRECTION AVANCÉE
    if (ligne_spec) {
        char *annee_pos = strstr(ligne_spec, "Année Académique");
        if (!annee_pos) annee_pos = strstr(ligne_spec, "Academic Year");
        
        if (annee_pos) {
            annee_pos = strchr(annee_pos, ' ');
            if (annee_pos) {
                annee_pos++;
                annee_pos = strchr(annee_pos, ' ');
                if (annee_pos) {
                    annee_pos++;
                    char *fin_annee = strchr(annee_pos, '\n');
                    if (!fin_annee) fin_annee = annee_pos + strlen(annee_pos);
                    
                    int len_annee = fin_annee - annee_pos;
                    if (len_annee > 0 && len_annee < MAX_ANNEE) {
                        strncpy(eleve->annee_academique, annee_pos, len_annee);
                        eleve->annee_academique[len_annee] = '\0';
                        nettoyer_chaine(eleve->annee_academique);
                    }
                }
            }
        }
    }
    
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
    
    // ========== EXTRACTION DES MATIÈRES AVEC MODULES ==========
    
    printf("═══════════════════════════════════════════════\n");
    printf("  EXTRACTION DES MATIÈRES PAR SEMESTRE\n");
    printf("═══════════════════════════════════════════════\n\n");
    
    Module modules[50];
    int nb_modules = 0;
    
    char *ptr2 = texte_copie;
    int semestre_actuel = 1;
    char module_courant[50] = "";
    int idx_module_courant = -1;
    
    while (*ptr2 && eleve->nombre_matieres < MAX_MATIERES) {
        char *fin_ligne = strchr(ptr2, '\n');
        if (!fin_ligne) {
            fin_ligne = ptr2 + strlen(ptr2);
        }
        
        int len = fin_ligne - ptr2;
        if (len > 0 && len < 500) {
            char ligne_temp[512];
            strncpy(ligne_temp, ptr2, len);
            ligne_temp[len] = '\0';
            nettoyer_chaine(ligne_temp);
            
            // Détection changement de semestre
            if (strcasestr(ligne_temp, "Semestre 2") || 
                strcasestr(ligne_temp, "Semester 2") ||
                strcasestr(ligne_temp, "Résultats Semestre 2")) {
                semestre_actuel = 2;
                printf("\n🔄 ═══ PASSAGE AU SEMESTRE 2 ═══\n\n");
                module_courant[0] = '\0';
                idx_module_courant = -1;
            }
            
            if (strlen(ligne_temp) > 10) {
                InfoMatiere info;
                if (extraire_matiere_ligne(ligne_temp, &info, semestre_actuel, 
                                          strlen(module_courant) > 0 ? module_courant : NULL)) {
                    
                    // Si c'est un MODULE (MCP)
                    if (strcmp(info.type, "MCP") == 0) {
                        strcpy(module_courant, info.code);
                        
                        // Créer un nouveau module
                        idx_module_courant = nb_modules;
                        strncpy(modules[nb_modules].code_module, info.code, sizeof(modules[nb_modules].code_module) - 1);
                        strncpy(modules[nb_modules].nom_module, info.nom, sizeof(modules[nb_modules].nom_module) - 1);
                        modules[nb_modules].moyenne_module = info.note;
                        modules[nb_modules].coef_module = info.coef;
                        modules[nb_modules].semestre = semestre_actuel;
                        modules[nb_modules].nb_matieres = 0;
                        modules[nb_modules].est_valide = info.est_valide;
                        
                        printf("📦 MODULE S%d: %s - %s (Coef: %.1f, Moy: %.2f) %s\n", 
                               semestre_actuel, info.code, info.nom, info.coef, info.note,
                               info.est_valide ? "✅" : "❌");
                        
                        nb_modules++;
                    }
                    // Si c'est un COURS (CP)
                    else {
                        Matiere *m = &eleve->matieres[eleve->nombre_matieres];
                        
                        // Format amélioré avec module parent
                        if (idx_module_courant >= 0) {
                            snprintf(m->nom_matiere, MAX_MATIERE - 1, "[S%d-%s|%s] %s - %s", 
                                    info.semestre, info.type, 
                                    modules[idx_module_courant].code_module,
                                    info.code, info.nom);
                            
                            // Ajouter au module
                            int idx = modules[idx_module_courant].nb_matieres;
                            strcpy(modules[idx_module_courant].codes_cp[idx], info.code);
                            modules[idx_module_courant].notes_cp[idx] = info.note;
                            modules[idx_module_courant].coefs_cp[idx] = info.coef;
                            modules[idx_module_courant].nb_matieres++;
                        } else {
                            snprintf(m->nom_matiere, MAX_MATIERE - 1, "[S%d-%s] %s - %s", 
                                    info.semestre, info.type, info.code, info.nom);
                        }
                        
                        m->nom_matiere[MAX_MATIERE - 1] = '\0';
                        m->note = info.note;
                        m->coefficient = info.coef;
                        strcpy(m->appreciation, info.est_valide ? "Validé" : "");
                        
                        printf("  📝 %2d. %-60s | Note: %5.2f | Coef: %.1f %s\n",
                               eleve->nombre_matieres + 1, m->nom_matiere, m->note, m->coefficient,
                               info.est_valide ? "✅" : "");
                        
                        eleve->nombre_matieres++;
                    }
                }
            }
        }
        
        ptr2 = (*fin_ligne == '\n') ? fin_ligne + 1 : fin_ligne;
    }
    
    free(texte_copie);
    
    printf("\n═══════════════════════════════════════════════\n");
    printf("📊 Total: %d matière(s) + %d module(s)\n", eleve->nombre_matieres, nb_modules);
    
    if (eleve->nombre_matieres > 0 || nb_modules > 0) {
        calculer_moyenne(eleve);
        
        // Statistiques détaillées
        int nb_s1 = 0, nb_s2 = 0, nb_mcp = 0, nb_cp = 0;
        int val_s1 = 0, val_s2 = 0;
        float moy_s1 = 0, moy_s2 = 0, moy_mcp = 0, moy_cp = 0;
        float coef_s1 = 0, coef_s2 = 0, coef_mcp = 0, coef_cp = 0;
        
        // Compter modules
        for (int i = 0; i < nb_modules; i++) {
            if (modules[i].semestre == 1) {
                nb_mcp++;
                moy_mcp += modules[i].moyenne_module * modules[i].coef_module;
                coef_mcp += modules[i].coef_module;
                if (modules[i].est_valide) val_s1++;
            } else {
                if (modules[i].est_valide) val_s2++;
            }
        }
        
        // Compter CP
        for (int i = 0; i < eleve->nombre_matieres; i++) {
            if (strstr(eleve->matieres[i].nom_matiere, "[S1-")) {
                nb_s1++;
                moy_s1 += eleve->matieres[i].note * eleve->matieres[i].coefficient;
                coef_s1 += eleve->matieres[i].coefficient;
            } else if (strstr(eleve->matieres[i].nom_matiere, "[S2-")) {
                nb_s2++;
                moy_s2 += eleve->matieres[i].note * eleve->matieres[i].coefficient;
                coef_s2 += eleve->matieres[i].coefficient;
            }
            
            if (strstr(eleve->matieres[i].nom_matiere, "-CP")) {
                nb_cp++;
                moy_cp += eleve->matieres[i].note * eleve->matieres[i].coefficient;
                coef_cp += eleve->matieres[i].coefficient;
            }
        }
        
        printf("\n📈 STATISTIQUES DÉTAILLÉES\n");
        printf("─────────────────────────────────────────────\n");
        
        if (nb_s1 > 0 || nb_modules > 0) {
            printf("📘 Semestre 1: %d cours + %d modules", nb_s1, nb_mcp);
            if (coef_s1 + coef_mcp > 0) {
                printf(" | Moyenne: %.2f/20", (moy_s1 + moy_mcp) / (coef_s1 + coef_mcp));
            }
            printf(" | Validés: %d\n", val_s1);
        }
        
        if (nb_s2 > 0) {
            printf("📗 Semestre 2: %d cours", nb_s2);
            if (coef_s2 > 0) {
                printf(" | Moyenne: %.2f/20", moy_s2 / coef_s2);
            }
            printf(" | Validés: %d\n", val_s2);
        }
        
        printf("\n");
        
        if (nb_modules > 0) {
            printf("📦 Modules (MCP): %d", nb_modules);
            if (coef_mcp > 0) {
                printf(" | Moyenne: %.2f/20", moy_mcp / coef_mcp);
            }
            printf("\n");
        }
        
        if (nb_cp > 0) {
            printf("📝 Cours (CP): %d", nb_cp);
            if (coef_cp > 0) {
                printf(" | Moyenne: %.2f/20", moy_cp / coef_cp);
            }
            printf("\n");
        }
        
        printf("─────────────────────────────────────────────\n");
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
    
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║  SAISIE MANUELLE D'UN BULLETIN        ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");
    
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