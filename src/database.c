#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bulletin.h"

int ouvrir_base(sqlite3 **db) {
    int rc = sqlite3_open("data/bulletins.db", db);
    if (rc) {
        fprintf(stderr, "Impossible d'ouvrir la base de données: %s\n", sqlite3_errmsg(*db));
        return 0;
    }
    return 1;
}

int creer_tables(sqlite3 *db) {
    // Table élèves
    const char *req_eleves =
        "CREATE TABLE IF NOT EXISTS eleves ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "nom TEXT, "
        "prenom TEXT, "
        "matricule TEXT UNIQUE, "
        "classe TEXT, "
        "niveau TEXT, "
        "annee_academique TEXT, "
        "lieu_naissance TEXT, "
        "date_naissance TEXT, "
        "date_bulletin TEXT, "
        "moyenne_generale REAL);";

    // Table matières avec semestre
    const char *req_matieres =
        "CREATE TABLE IF NOT EXISTS matieres ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "eleve_id INTEGER, "
        "nom_matiere TEXT, "
        "code_matiere TEXT, "
        "note REAL, "
        "coefficient REAL, "
        "appreciation TEXT, "
        "semestre INTEGER DEFAULT 1, "
        "type_matiere TEXT, "  // 'CP' ou 'MCP'
        "module_parent TEXT, "  // Code du module parent pour les CP
        "est_valide INTEGER DEFAULT 0, "
        "FOREIGN KEY (eleve_id) REFERENCES eleves(id) ON DELETE CASCADE);";
    
    // Table modules (pour les MCP)
    const char *req_modules =
        "CREATE TABLE IF NOT EXISTS modules ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "eleve_id INTEGER, "
        "code_module TEXT, "
        "nom_module TEXT, "
        "semestre INTEGER, "
        "moyenne_module REAL, "
        "coefficient REAL, "
        "est_valide INTEGER DEFAULT 0, "
        "nb_matieres_composantes INTEGER DEFAULT 0, "
        "FOREIGN KEY (eleve_id) REFERENCES eleves(id) ON DELETE CASCADE);";
    
    // Table statistiques par semestre
    const char *req_stats =
        "CREATE TABLE IF NOT EXISTS statistiques_semestre ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "eleve_id INTEGER, "
        "semestre INTEGER, "
        "nb_matieres INTEGER, "
        "nb_modules INTEGER, "
        "nb_valides INTEGER, "
        "moyenne_semestre REAL, "
        "FOREIGN KEY (eleve_id) REFERENCES eleves(id) ON DELETE CASCADE);";

    char *err = NULL;
    int rc1 = sqlite3_exec(db, req_eleves, 0, 0, &err);
    int rc2 = sqlite3_exec(db, req_matieres, 0, 0, &err);
    int rc3 = sqlite3_exec(db, req_modules, 0, 0, &err);
    int rc4 = sqlite3_exec(db, req_stats, 0, 0, &err);

    if (rc1 != SQLITE_OK || rc2 != SQLITE_OK || rc3 != SQLITE_OK || rc4 != SQLITE_OK) {
        fprintf(stderr, "Erreur lors de la création des tables: %s\n", err);
        sqlite3_free(err);
        return 0;
    }
    
    printf("✅ Tables créées/vérifiées avec succès\n");
    return 1;
}

// Extraire le code matière depuis le nom formaté
void extraire_code_matiere(const char* nom_complet, char* code, char* type, int* semestre, char* module_parent) {
    // Format: [S1-CP|MCP31L1101] CP31L1105 - Algèbre 1A
    // ou: [S1-MCP] MCP31L1101 - Sciences humaines 1a
    
    *semestre = 1;
    strcpy(type, "CP");
    code[0] = '\0';
    module_parent[0] = '\0';
    
    if (nom_complet[0] == '[') {
        // Extraire semestre
        if (nom_complet[2] >= '1' && nom_complet[2] <= '9') {
            *semestre = nom_complet[2] - '0';
        }
        
        // Extraire type
        char *type_pos = strchr(nom_complet, '-');
        if (type_pos) {
            type_pos++;
            if (strncmp(type_pos, "MCP", 3) == 0) {
                strcpy(type, "MCP");
            } else if (strncmp(type_pos, "CP", 2) == 0) {
                strcpy(type, "CP");
                
                // Chercher module parent
                char *pipe = strchr(type_pos, '|');
                if (pipe) {
                    pipe++;
                    char *end = strchr(pipe, ']');
                    if (end) {
                        int len = end - pipe;
                        if (len > 0 && len < 50) {
                            strncpy(module_parent, pipe, len);
                            module_parent[len] = '\0';
                        }
                    }
                }
            }
        }
    }
    
    // Extraire le code après le ]
    char *debut_code = strchr(nom_complet, ']');
    if (debut_code) {
        debut_code++;
        while (*debut_code == ' ') debut_code++;
        
        // Le code est avant le ' - '
        char *fin_code = strstr(debut_code, " - ");
        if (fin_code) {
            int len = fin_code - debut_code;
            if (len > 0 && len < 50) {
                strncpy(code, debut_code, len);
                code[len] = '\0';
            }
        }
    }
}

int inserer_eleve(sqlite3 *db, Eleve *eleve) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "INSERT INTO eleves (nom, prenom, matricule, classe, niveau, "
        "annee_academique, lieu_naissance, date_naissance, date_bulletin, moyenne_generale) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Erreur préparation requête: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, eleve->nom, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, eleve->prenom, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, eleve->matricule, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, eleve->classe, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, eleve->niveau, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, eleve->annee_academique, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, eleve->lieu_naissance, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, eleve->date_naissance, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, eleve->date_bulletin, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 10, eleve->moyenne_generale);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Erreur insertion élève: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);

    int eleve_id = (int)sqlite3_last_insert_rowid(db);
    printf("✅ Élève enregistré avec ID: %d\n", eleve_id);

    // Insertion des matières avec analyse
    const char *sql2 = 
        "INSERT INTO matieres (eleve_id, nom_matiere, code_matiere, note, coefficient, "
        "appreciation, semestre, type_matiere, module_parent, est_valide) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    // Statistiques par semestre
    int stats_s1_nb = 0, stats_s1_val = 0, stats_s2_nb = 0, stats_s2_val = 0;
    float stats_s1_moy = 0, stats_s1_coef = 0, stats_s2_moy = 0, stats_s2_coef = 0;
    
    for (int i = 0; i < eleve->nombre_matieres; i++) {
        char code[50], type[10], module_parent[50];
        int semestre;
        
        extraire_code_matiere(eleve->matieres[i].nom_matiere, code, type, &semestre, module_parent);
        
        int est_valide = (strlen(eleve->matieres[i].appreciation) > 0 && 
                         strstr(eleve->matieres[i].appreciation, "Validé") != NULL) ? 1 : 0;
        
        if (sqlite3_prepare_v2(db, sql2, -1, &stmt, 0) != SQLITE_OK) {
            fprintf(stderr, "Erreur préparation matière %d\n", i);
            return 0;
        }
        
        sqlite3_bind_int(stmt, 1, eleve_id);
        sqlite3_bind_text(stmt, 2, eleve->matieres[i].nom_matiere, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, code, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, eleve->matieres[i].note);
        sqlite3_bind_double(stmt, 5, eleve->matieres[i].coefficient);
        sqlite3_bind_text(stmt, 6, eleve->matieres[i].appreciation, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 7, semestre);
        sqlite3_bind_text(stmt, 8, type, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, module_parent, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 10, est_valide);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "Erreur insertion matière %d: %s\n", i, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 0;
        }
        sqlite3_finalize(stmt);
        
        // Mise à jour statistiques
        if (semestre == 1) {
            stats_s1_nb++;
            if (est_valide) stats_s1_val++;
            stats_s1_moy += eleve->matieres[i].note * eleve->matieres[i].coefficient;
            stats_s1_coef += eleve->matieres[i].coefficient;
        } else {
            stats_s2_nb++;
            if (est_valide) stats_s2_val++;
            stats_s2_moy += eleve->matieres[i].note * eleve->matieres[i].coefficient;
            stats_s2_coef += eleve->matieres[i].coefficient;
        }
    }
    
    // Insérer statistiques semestre 1
    if (stats_s1_nb > 0) {
        const char *sql_stats = 
            "INSERT INTO statistiques_semestre (eleve_id, semestre, nb_matieres, nb_valides, moyenne_semestre) "
            "VALUES (?, 1, ?, ?, ?);";
        
        if (sqlite3_prepare_v2(db, sql_stats, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, eleve_id);
            sqlite3_bind_int(stmt, 2, stats_s1_nb);
            sqlite3_bind_int(stmt, 3, stats_s1_val);
            sqlite3_bind_double(stmt, 4, stats_s1_coef > 0 ? stats_s1_moy / stats_s1_coef : 0);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    // Insérer statistiques semestre 2
    if (stats_s2_nb > 0) {
        const char *sql_stats = 
            "INSERT INTO statistiques_semestre (eleve_id, semestre, nb_matieres, nb_valides, moyenne_semestre) "
            "VALUES (?, 2, ?, ?, ?);";
        
        if (sqlite3_prepare_v2(db, sql_stats, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, eleve_id);
            sqlite3_bind_int(stmt, 2, stats_s2_nb);
            sqlite3_bind_int(stmt, 3, stats_s2_val);
            sqlite3_bind_double(stmt, 4, stats_s2_coef > 0 ? stats_s2_moy / stats_s2_coef : 0);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    printf("✅ %d matière(s) enregistrée(s)\n", eleve->nombre_matieres);
    printf("📊 Semestre 1: %d matières (%d validées)\n", stats_s1_nb, stats_s1_val);
    if (stats_s2_nb > 0) {
        printf("📊 Semestre 2: %d matières (%d validées)\n", stats_s2_nb, stats_s2_val);
    }
    
    return 1;
}

Eleve* recuperer_eleve(sqlite3 *db, int id) {
    Eleve *eleve = malloc(sizeof(Eleve));
    if (!eleve) return NULL;
    
    const char *sql = "SELECT * FROM eleves WHERE id = ?;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        free(eleve);
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        eleve->id = sqlite3_column_int(stmt, 0);
        strcpy(eleve->nom, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(eleve->prenom, (const char*)sqlite3_column_text(stmt, 2));
        strcpy(eleve->matricule, (const char*)sqlite3_column_text(stmt, 3));
        strcpy(eleve->classe, (const char*)sqlite3_column_text(stmt, 4));
        strcpy(eleve->niveau, (const char*)sqlite3_column_text(stmt, 5));
        strcpy(eleve->annee_academique, (const char*)sqlite3_column_text(stmt, 6));
        strcpy(eleve->lieu_naissance, (const char*)sqlite3_column_text(stmt, 7));
        strcpy(eleve->date_naissance, (const char*)sqlite3_column_text(stmt, 8));
        strcpy(eleve->date_bulletin, (const char*)sqlite3_column_text(stmt, 9));
        eleve->moyenne_generale = sqlite3_column_double(stmt, 10);
    } else {
        sqlite3_finalize(stmt);
        free(eleve);
        return NULL;
    }
    sqlite3_finalize(stmt);
    
    // Récupération des matières avec tri par semestre
    const char *sql2 = "SELECT * FROM matieres WHERE eleve_id = ? ORDER BY semestre, type_matiere DESC, code_matiere;";
    if (sqlite3_prepare_v2(db, sql2, -1, &stmt, 0) != SQLITE_OK) {
        free(eleve);
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    eleve->nombre_matieres = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW && eleve->nombre_matieres < MAX_MATIERES) {
        strcpy(eleve->matieres[eleve->nombre_matieres].nom_matiere, 
               (const char*)sqlite3_column_text(stmt, 2));
        eleve->matieres[eleve->nombre_matieres].note = sqlite3_column_double(stmt, 4);
        eleve->matieres[eleve->nombre_matieres].coefficient = sqlite3_column_double(stmt, 5);
        strcpy(eleve->matieres[eleve->nombre_matieres].appreciation, 
               (const char*)sqlite3_column_text(stmt, 6));
        eleve->nombre_matieres++;
    }
    sqlite3_finalize(stmt);
    
    return eleve;
}

void lister_tous_eleves(sqlite3 *db) {
    const char *sql = "SELECT id, nom, prenom, matricule, classe, moyenne_generale FROM eleves ORDER BY moyenne_generale DESC;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Erreur de requête\n");
        return;
    }
    
    printf("\n╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                        LISTE DES ÉLÈVES                                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n\n");
    printf("%-5s %-20s %-15s %-20s %-25s %s\n", 
           "ID", "Nom", "Prénom", "Matricule", "Classe", "Moyenne");
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("%-5d %-20s %-15s %-20s %-25s %.2f\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_text(stmt, 2),
               sqlite3_column_text(stmt, 3),
               sqlite3_column_text(stmt, 4),
               sqlite3_column_double(stmt, 5));
        count++;
    }
    
    sqlite3_finalize(stmt);
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    printf("Total: %d élève(s)\n", count);
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
}

void afficher_statistiques_eleve(sqlite3 *db, int eleve_id) {
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           STATISTIQUES DÉTAILLÉES PAR SEMESTRE               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    const char *sql = 
        "SELECT semestre, nb_matieres, nb_valides, moyenne_semestre "
        "FROM statistiques_semestre WHERE eleve_id = ? ORDER BY semestre;";
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        return;
    }
    
    sqlite3_bind_int(stmt, 1, eleve_id);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int sem = sqlite3_column_int(stmt, 0);
        int nb_mat = sqlite3_column_int(stmt, 1);
        int nb_val = sqlite3_column_int(stmt, 2);
        float moy = sqlite3_column_double(stmt, 3);
        
        printf("📘 Semestre %d\n", sem);
        printf("   • Matières: %d\n", nb_mat);
        printf("   • Validées: %d (%.1f%%)\n", nb_val, nb_mat > 0 ? (nb_val * 100.0 / nb_mat) : 0);
        printf("   • Moyenne: %.2f/20\n", moy);
        printf("\n");
    }
    
    sqlite3_finalize(stmt);
}

Eleve* rechercher_par_matricule(sqlite3 *db, const char* matricule) {
    Eleve *eleve = malloc(sizeof(Eleve));
    if (!eleve) return NULL;
    
    const char *sql = "SELECT id FROM eleves WHERE matricule = ?;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        free(eleve);
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, matricule, -1, SQLITE_STATIC);
    
    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    
    if (id == -1) {
        free(eleve);
        return NULL;
    }
    
    free(eleve);
    return recuperer_eleve(db, id);
}