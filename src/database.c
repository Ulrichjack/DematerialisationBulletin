#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "database.h"
#include "bulletin_utils.h" // Pour extraire_metadata_matiere

// Déclaration anticipée pour permettre l'appel dans inserer_eleve
Eleve* rechercher_par_matricule(sqlite3 *db, const char* matricule);
int supprimer_eleve(sqlite3 *db, int eleve_id);


int ouvrir_base(sqlite3 **db) {
    int rc = sqlite3_open("data/bulletins.db", db);
    if (rc) {
        fprintf(stderr, "Impossible d'ouvrir la base de données: %s\n", sqlite3_errmsg(*db));
        return 0;
    }
    // Activer les clés étrangères
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, 0);
    return 1;
}

int creer_tables(sqlite3 *db) {
    const char *req_eleves =
        "CREATE TABLE IF NOT EXISTS eleves ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "nom TEXT, prenom TEXT, matricule TEXT UNIQUE, classe TEXT, "
        "niveau TEXT, annee_academique TEXT, lieu_naissance TEXT, "
        "date_naissance TEXT, date_bulletin TEXT, moyenne_generale REAL);";

    const char *req_matieres =
        "CREATE TABLE IF NOT EXISTS matieres ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, eleve_id INTEGER, nom_matiere TEXT, "
        "code_matiere TEXT, note REAL, coefficient REAL, appreciation TEXT, "
        "semestre INTEGER DEFAULT 1, type_matiere TEXT, module_parent TEXT, "
        "est_valide INTEGER DEFAULT 0, "
        "FOREIGN KEY (eleve_id) REFERENCES eleves(id) ON DELETE CASCADE);";

    char *err = NULL;
    if (sqlite3_exec(db, req_eleves, 0, 0, &err) != SQLITE_OK) {
        fprintf(stderr, "Erreur table eleves: %s\n", err);
        sqlite3_free(err);
        return 0;
    }
    if (sqlite3_exec(db, req_matieres, 0, 0, &err) != SQLITE_OK) {
        fprintf(stderr, "Erreur table matieres: %s\n", err);
        sqlite3_free(err);
        return 0;
    }
    
    printf("✅ Tables créées/vérifiées avec succès\n");
    return 1;
}


// Fonction interne pour mettre à jour un élève existant
int mettre_a_jour_eleve(sqlite3 *db, int eleve_id, Eleve *eleve) {
    sqlite3_stmt *stmt;
    const char *sql_update_eleve = 
        "UPDATE eleves SET nom=?, prenom=?, matricule=?, classe=?, niveau=?, "
        "annee_academique=?, lieu_naissance=?, date_naissance=?, date_bulletin=?, moyenne_generale=? "
        "WHERE id=?;";

    if (sqlite3_prepare_v2(db, sql_update_eleve, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Erreur préparation MAJ élève: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    // Lier les paramètres
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
    sqlite3_bind_int(stmt, 11, eleve_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Erreur exécution MAJ élève: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);

    // Supprimer les anciennes matières pour les remplacer
    const char *sql_delete_matieres = "DELETE FROM matieres WHERE eleve_id = ?;";
    if (sqlite3_prepare_v2(db, sql_delete_matieres, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, eleve_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Insérer les nouvelles matières
    const char *sql_insert_matiere = 
        "INSERT INTO matieres (eleve_id, nom_matiere, code_matiere, note, coefficient, "
        "appreciation, semestre, type_matiere, module_parent, est_valide) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    for (int i = 0; i < eleve->nombre_matieres; i++) {
        MetadataMatiere meta;
        extraire_metadata_matiere(eleve->matieres[i].nom_matiere, &meta);
        
        int est_valide = (eleve->matieres[i].note >= 10.0) ? 1 : 0;
        
        if (sqlite3_prepare_v2(db, sql_insert_matiere, -1, &stmt, 0) != SQLITE_OK) return 0;
        
        sqlite3_bind_int(stmt, 1, eleve_id);
        sqlite3_bind_text(stmt, 2, eleve->matieres[i].nom_matiere, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, meta.code, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, eleve->matieres[i].note);
        sqlite3_bind_double(stmt, 5, eleve->matieres[i].coefficient);
        sqlite3_bind_text(stmt, 6, eleve->matieres[i].appreciation, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 7, meta.semestre);
        sqlite3_bind_text(stmt, 8, meta.type, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, meta.module_parent, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 10, est_valide);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "Erreur insertion matière (MAJ) %d: %s\n", i, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 0;
        }
        sqlite3_finalize(stmt);
    }

    printf("✅ Élève ID %d mis à jour avec succès.\n", eleve_id);
    return 1;
}

int inserer_eleve(sqlite3 *db, Eleve *eleve) {
    // Vérifier si un élève avec ce matricule existe déjà
    Eleve* existant = rechercher_par_matricule(db, eleve->matricule);
    if (existant) {
        int eleve_id = existant->id;
        free(existant);
        // Si oui, on le met à jour
        return mettre_a_jour_eleve(db, eleve_id, eleve);
    }

    // --- Sinon, on procède à l'insertion (code original) ---
    sqlite3_stmt *stmt;
    const char *sql_insert_eleve = 
        "INSERT INTO eleves (nom, prenom, matricule, classe, niveau, "
        "annee_academique, lieu_naissance, date_naissance, date_bulletin, moyenne_generale) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    if (sqlite3_prepare_v2(db, sql_insert_eleve, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Erreur préparation insertion élève: %s\n", sqlite3_errmsg(db));
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
        fprintf(stderr, "Erreur exécution insertion élève: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);

    int eleve_id = (int)sqlite3_last_insert_rowid(db);
    
    // Insertion des matières
    const char *sql_insert_matiere = 
        "INSERT INTO matieres (eleve_id, nom_matiere, code_matiere, note, coefficient, "
        "appreciation, semestre, type_matiere, module_parent, est_valide) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    for (int i = 0; i < eleve->nombre_matieres; i++) {
        MetadataMatiere meta;
        extraire_metadata_matiere(eleve->matieres[i].nom_matiere, &meta);
        int est_valide = (eleve->matieres[i].note >= 10.0) ? 1 : 0;
        
        if (sqlite3_prepare_v2(db, sql_insert_matiere, -1, &stmt, 0) != SQLITE_OK) return 0;
        
        sqlite3_bind_int(stmt, 1, eleve_id);
        sqlite3_bind_text(stmt, 2, eleve->matieres[i].nom_matiere, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, meta.code, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, eleve->matieres[i].note);
        sqlite3_bind_double(stmt, 5, eleve->matieres[i].coefficient);
        sqlite3_bind_text(stmt, 6, eleve->matieres[i].appreciation, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 7, meta.semestre);
        sqlite3_bind_text(stmt, 8, meta.type, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, meta.module_parent, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 10, est_valide);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "Erreur insertion matière %d: %s\n", i, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 0;
        }
        sqlite3_finalize(stmt);
    }
    
    printf("✅ Nouvel élève enregistré avec ID: %d\n", eleve_id);
    return 1;
}

int supprimer_eleve(sqlite3 *db, int eleve_id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM eleves WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Erreur préparation suppression: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_bind_int(stmt, 1, eleve_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Erreur suppression élève: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    printf("🗑️ Élève ID %d supprimé.\n", eleve_id);
    return 1;
}

Eleve* recuperer_eleve(sqlite3 *db, int id) {
    Eleve *eleve = malloc(sizeof(Eleve));
    if (!eleve) return NULL;
    memset(eleve, 0, sizeof(Eleve));

    const char *sql = "SELECT * FROM eleves WHERE id = ?;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        free(eleve);
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        eleve->id = sqlite3_column_int(stmt, 0);
        // ... (copie des champs avec vérification de nullité)
        const char * text;
        text = (const char*)sqlite3_column_text(stmt, 1); if(text) strncpy(eleve->nom, text, MAX_NOM-1);
        text = (const char*)sqlite3_column_text(stmt, 2); if(text) strncpy(eleve->prenom, text, MAX_PRENOM-1);
        text = (const char*)sqlite3_column_text(stmt, 3); if(text) strncpy(eleve->matricule, text, MAX_MATRICULE-1);
        text = (const char*)sqlite3_column_text(stmt, 4); if(text) strncpy(eleve->classe, text, MAX_CLASSE-1);
        text = (const char*)sqlite3_column_text(stmt, 5); if(text) strncpy(eleve->niveau, text, MAX_NIVEAU-1);
        text = (const char*)sqlite3_column_text(stmt, 6); if(text) strncpy(eleve->annee_academique, text, MAX_ANNEE-1);
        text = (const char*)sqlite3_column_text(stmt, 7); if(text) strncpy(eleve->lieu_naissance, text, MAX_LIEU-1);
        text = (const char*)sqlite3_column_text(stmt, 8); if(text) strncpy(eleve->date_naissance, text, 19);
        text = (const char*)sqlite3_column_text(stmt, 9); if(text) strncpy(eleve->date_bulletin, text, 19);
        eleve->moyenne_generale = sqlite3_column_double(stmt, 10);
    } else {
        sqlite3_finalize(stmt);
        free(eleve);
        return NULL;
    }
    sqlite3_finalize(stmt);
    
    const char *sql2 = "SELECT nom_matiere, note, coefficient, appreciation FROM matieres WHERE eleve_id = ? ORDER BY semestre, nom_matiere;";
    if (sqlite3_prepare_v2(db, sql2, -1, &stmt, 0) != SQLITE_OK) {
        free(eleve);
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    eleve->nombre_matieres = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW && eleve->nombre_matieres < MAX_MATIERES) {
        int i = eleve->nombre_matieres;
        const char *text;
        text = (const char*)sqlite3_column_text(stmt, 0); if(text) strncpy(eleve->matieres[i].nom_matiere, text, MAX_MATIERE-1);
        eleve->matieres[i].note = sqlite3_column_double(stmt, 1);
        eleve->matieres[i].coefficient = sqlite3_column_double(stmt, 2);
        text = (const char*)sqlite3_column_text(stmt, 3); if(text) strncpy(eleve->matieres[i].appreciation, text, 199);
        
        eleve->nombre_matieres++;
    }
    sqlite3_finalize(stmt);
    
    return eleve;
}

Eleve* rechercher_par_matricule(sqlite3 *db, const char* matricule) {
    if (!matricule || strlen(matricule) == 0) return NULL;
    
    const char *sql = "SELECT id FROM eleves WHERE matricule = ?;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, matricule, -1, SQLITE_STATIC);
    
    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    
    if (id == -1) {
        return NULL;
    }
    
    return recuperer_eleve(db, id);
}