#include "bulletin.h"
#include "bulletin_utils.h"

// ========== EXTRACTION DES MÉTADONNÉES D'UNE MATIÈRE ==========



void extraire_metadata_matiere(const char* nom_complet, MetadataMatiere* meta) {
    memset(meta, 0, sizeof(MetadataMatiere));
    meta->semestre = 1;
    meta->est_module = 0;
    
    // Format: [S1-CP|MCP31L1101] CP31L1105 - Algèbre 1A
    // ou:     [S1-MCP] MCP31L1101 - Sciences humaines
    
    if (nom_complet[0] != '[') {
        // Pas de métadonnées, copier le nom brut
        strncpy(meta->nom, nom_complet, MAX_MATIERE - 1);
        return;
    }
    
    // Extraire semestre
    if (nom_complet[2] >= '1' && nom_complet[2] <= '9') {
        meta->semestre = nom_complet[2] - '0';
    }
    
    // Extraire type et module parent
    char *type_pos = strchr(nom_complet, '-');
    if (type_pos) {
        type_pos++;
        
        if (strncmp(type_pos, "MCP", 3) == 0) {
            strcpy(meta->type, "MCP");
            meta->est_module = 1;
        } else if (strncmp(type_pos, "CP", 2) == 0) {
            strcpy(meta->type, "CP");
            
            // Chercher module parent après |
            char *pipe = strchr(type_pos, '|');
            if (pipe) {
                pipe++;
                char *end = strchr(pipe, ']');
                if (end) {
                    int len = end - pipe;
                    if (len > 0 && len < 50) {
                        strncpy(meta->module_parent, pipe, len);
                        meta->module_parent[len] = '\0';
                    }
                }
            }
        }
    }
    
    // Extraire le code
    char *debut_code = strchr(nom_complet, ']');
    if (debut_code) {
        debut_code++;
        while (*debut_code == ' ') debut_code++;
        
        char *fin_code = strstr(debut_code, " - ");
        if (fin_code) {
            int len = fin_code - debut_code;
            if (len > 0 && len < 50) {
                strncpy(meta->code, debut_code, len);
                meta->code[len] = '\0';
            }
            
            // Extraire le nom
            fin_code += 3; // Sauter " - "
            strncpy(meta->nom, fin_code, MAX_MATIERE - 1);
        }
    }
}

// ========== AFFICHAGE AMÉLIORÉ AVEC STRUCTURE MODULES ==========

void afficher_eleve(const Eleve* eleve) {
    if (!eleve) {
        printf("Élève non trouvé\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                     BULLETIN SCOLAIRE                          ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    // ========== INFORMATIONS PERSONNELLES ==========
    printf("📋 INFORMATIONS PERSONNELLES\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("  ID Système      : %d\n", eleve->id);
    printf("  Nom complet     : %s %s\n", eleve->nom, eleve->prenom);
    
    if (strlen(eleve->matricule) > 0 && strcmp(eleve->matricule, "N/A") != 0) {
        printf("  Matricule       : %s\n", eleve->matricule);
    }
    
    if (strlen(eleve->date_naissance) > 0 && strcmp(eleve->date_naissance, "N/A") != 0) {
        printf("  Date naissance  : %s\n", eleve->date_naissance);
    }
    
    if (strlen(eleve->lieu_naissance) > 0 && strcmp(eleve->lieu_naissance, "N/A") != 0) {
        printf("  Lieu naissance  : %s\n", eleve->lieu_naissance);
    }
    
    printf("\n🏫 INFORMATIONS ACADÉMIQUES\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    
    if (strlen(eleve->classe) > 0) {
        printf("  Classe          : %s\n", eleve->classe);
    }
    
    if (strlen(eleve->niveau) > 0 && strcmp(eleve->niveau, "N/A") != 0) {
        printf("  Niveau          : %s\n", eleve->niveau);
    }
    
    if (strlen(eleve->annee_academique) > 0 && strcmp(eleve->annee_academique, "N/A") != 0) {
        printf("  Année           : %s\n", eleve->annee_academique);
    }
    
    printf("  Date bulletin   : %s\n", eleve->date_bulletin);
    printf("  Nombre matières : %d\n", eleve->nombre_matieres);
    printf("  Moyenne générale: %.2f/20\n", eleve->moyenne_generale);
    
    // ========== AFFICHAGE PAR SEMESTRE ET MODULE ==========
    
    if (eleve->nombre_matieres > 0) {
        printf("\n📚 NOTES PAR SEMESTRE ET MODULE\n");
        printf("═════════════════════════════════════════════════════════════════\n\n");
        
        int semestre_actuel = 0;
        char module_actuel[50] = "";
        int nb_matieres_module = 0;
        float somme_notes_module = 0.0;
        float somme_coefs_module = 0.0;
        
        // Compteurs par semestre
        int nb_valides_s1 = 0, nb_total_s1 = 0;
        int nb_valides_s2 = 0, nb_total_s2 = 0;
        float credits_s1 = 0, credits_s2 = 0;
        
        for (int i = 0; i < eleve->nombre_matieres; i++) {
            MetadataMatiere meta;
            extraire_metadata_matiere(eleve->matieres[i].nom_matiere, &meta);
            
            // Compter crédits validés (on considère validé si note >= 10 ou si appreciation contient "Validé")
            int est_valide = 0;
            if (eleve->matieres[i].note >= 10.0 || 
                (strlen(eleve->matieres[i].appreciation) > 0 && 
                 strstr(eleve->matieres[i].appreciation, "Valid") != NULL)) {
                est_valide = 1;
            }
            
            // Changement de semestre
            if (meta.semestre != semestre_actuel) {
                // Afficher stats du semestre précédent
                if (semestre_actuel == 1) {
                    printf("   ──────────────────────────────────────────────────────\n");
                    printf("   📊 Bilan S1: %d/%d matières validées | %.1f crédits\n\n", 
                           nb_valides_s1, nb_total_s1, credits_s1);
                } else if (semestre_actuel == 2) {
                    printf("   ──────────────────────────────────────────────────────\n");
                    printf("   📊 Bilan S2: %d/%d matières validées | %.1f crédits\n\n", 
                           nb_valides_s2, nb_total_s2, credits_s2);
                }
                
                semestre_actuel = meta.semestre;
                printf("🔵 ══════════════════════════════════════════════════════════\n");
                printf("   SEMESTRE %d\n", semestre_actuel);
                printf("   ══════════════════════════════════════════════════════════\n\n");
                module_actuel[0] = '\0';
                nb_matieres_module = 0;
            }
            
            // Compter pour statistiques
            if (!meta.est_module) {
                if (meta.semestre == 1) {
                    nb_total_s1++;
                    if (est_valide) {
                        nb_valides_s1++;
                        credits_s1 += eleve->matieres[i].coefficient;
                    }
                } else if (meta.semestre == 2) {
                    nb_total_s2++;
                    if (est_valide) {
                        nb_valides_s2++;
                        credits_s2 += eleve->matieres[i].coefficient;
                    }
                }
            }
            
            // Changement de module (ou nouveau module)
            if (strlen(meta.module_parent) > 0 && strcmp(meta.module_parent, module_actuel) != 0) {
                // Afficher moyenne du module précédent
                if (nb_matieres_module > 0 && somme_coefs_module > 0) {
                    float moy_module = somme_notes_module / somme_coefs_module;
                    char *couleur_moy = moy_module >= 10.0 ? "\033[1;32m" : "\033[1;31m";
                    printf("      └─ Moyenne module: %s%.2f/20\033[0m\n\n", couleur_moy, moy_module);
                }
                
                // Nouveau module
                strcpy(module_actuel, meta.module_parent);
                nb_matieres_module = 0;
                somme_notes_module = 0.0;
                somme_coefs_module = 0.0;
                
                // Trouver le nom du module
                char nom_module[MAX_MATIERE] = "Module";
                for (int j = 0; j < eleve->nombre_matieres; j++) {
                    MetadataMatiere meta_mod;
                    extraire_metadata_matiere(eleve->matieres[j].nom_matiere, &meta_mod);
                    if (meta_mod.est_module && strcmp(meta_mod.code, module_actuel) == 0) {
                        strncpy(nom_module, meta_mod.nom, sizeof(nom_module) - 1);
                        break;
                    }
                }
                
                printf("   📦 %s - %s\n", module_actuel, nom_module);
            }
            
            // Afficher la matière
            if (meta.est_module) {
                // C'est un module (MCP) - on ne l'affiche pas dans la liste des matières
                continue;
            } else {
                // C'est un cours (CP)
                char *couleur = "";
                if (eleve->matieres[i].note >= 16.0) couleur = "\033[1;32m"; // Vert
                else if (eleve->matieres[i].note >= 14.0) couleur = "\033[1;36m"; // Cyan
                else if (eleve->matieres[i].note >= 10.0) couleur = "\033[1;33m"; // Jaune
                else couleur = "\033[1;31m"; // Rouge
                
                char validation_mark[5] = "";
                if (est_valide) strcpy(validation_mark, " ✅");
                
                printf("      ├─ %s - %s\n", meta.code, meta.nom);
                printf("      │  Note: %s%.2f/20\033[0m | Coef: %.1f%s", 
                       couleur, eleve->matieres[i].note, eleve->matieres[i].coefficient, validation_mark);
                
                if (strlen(eleve->matieres[i].appreciation) > 0) {
                    printf(" | %s", eleve->matieres[i].appreciation);
                }
                printf("\n");
                
                // Calculer pour moyenne du module
                if (strlen(meta.module_parent) > 0) {
                    nb_matieres_module++;
                    somme_notes_module += eleve->matieres[i].note * eleve->matieres[i].coefficient;
                    somme_coefs_module += eleve->matieres[i].coefficient;
                }
            }
        }
        
        // Afficher moyenne du dernier module
        if (nb_matieres_module > 0 && somme_coefs_module > 0) {
            float moy_module = somme_notes_module / somme_coefs_module;
            char *couleur_moy = moy_module >= 10.0 ? "\033[1;32m" : "\033[1;31m";
            printf("      └─ Moyenne module: %s%.2f/20\033[0m\n\n", couleur_moy, moy_module);
        }
        
        // Afficher stats du dernier semestre
        if (semestre_actuel == 1) {
            printf("   ──────────────────────────────────────────────────────\n");
            printf("   📊 Bilan S1: %d/%d matières validées | %.1f crédits\n\n", 
                   nb_valides_s1, nb_total_s1, credits_s1);
        } else if (semestre_actuel == 2) {
            printf("   ──────────────────────────────────────────────────────\n");
            printf("   📊 Bilan S2: %d/%d matières validées | %.1f crédits\n\n", 
                   nb_valides_s2, nb_total_s2, credits_s2);
        }
        
        printf("═════════════════════════════════════════════════════════════════\n");
        printf("📊 TOTAL ANNUEL: %d/%d matières validées | %.1f crédits\n", 
               nb_valides_s1 + nb_valides_s2, nb_total_s1 + nb_total_s2, credits_s1 + credits_s2);
    }
    
    // ========== RÉSULTAT GLOBAL ==========
    printf("\n🎯 RÉSULTAT GLOBAL\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    printf("  Moyenne générale: ");
    
    if (eleve->moyenne_generale >= 16.0) {
        printf("\033[1;32m%.2f/20 - Très bien ✨\033[0m\n", eleve->moyenne_generale);
    } else if (eleve->moyenne_generale >= 14.0) {
        printf("\033[1;36m%.2f/20 - Bien 👍\033[0m\n", eleve->moyenne_generale);
    } else if (eleve->moyenne_generale >= 12.0) {
        printf("\033[1;33m%.2f/20 - Assez bien 👌\033[0m\n", eleve->moyenne_generale);
    } else if (eleve->moyenne_generale >= 10.0) {
        printf("\033[1;33m%.2f/20 - Passable ⚠️\033[0m\n", eleve->moyenne_generale);
    } else {
        printf("\033[1;31m%.2f/20 - Insuffisant ⚠️\033[0m\n", eleve->moyenne_generale);
    }
    
    printf("═════════════════════════════════════════════════════════════════\n\n");
}

// ========== CALCUL DE LA MOYENNE ==========

void calculer_moyenne(Eleve* eleve) {
    if (!eleve || eleve->nombre_matieres == 0) {
        eleve->moyenne_generale = 0.0;
        return;
    }
    
    float somme_notes = 0.0;
    float somme_coefficients = 0.0;
    
    for (int i = 0; i < eleve->nombre_matieres; i++) {
        // Ne compter que les CP (pas les MCP qui sont des moyennes calculées)
        MetadataMatiere meta;
        extraire_metadata_matiere(eleve->matieres[i].nom_matiere, &meta);
        
        if (!meta.est_module) {
            somme_notes += eleve->matieres[i].note * eleve->matieres[i].coefficient;
            somme_coefficients += eleve->matieres[i].coefficient;
        }
    }
    
    if (somme_coefficients > 0) {
        eleve->moyenne_generale = somme_notes / somme_coefficients;
    } else {
        eleve->moyenne_generale = 0.0;
    }
}

// ========== NETTOYAGE DE CHAÎNE ==========

void nettoyer_chaine(char* str) {
    if (!str) return;
    
    // Supprimer espaces au début
    char* debut = str;
    while (*debut == ' ' || *debut == '\t' || *debut == '\n' || *debut == '\r') {
        debut++;
    }
    
    if (debut != str) {
        memmove(str, debut, strlen(debut) + 1);
    }
    
    // Supprimer espaces à la fin
    int len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || 
                       str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
    }
}