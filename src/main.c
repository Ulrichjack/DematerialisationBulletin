#include <stdio.h>
#include <string.h>
#include "bulletin.h"
#include "database.h"
#include "ocr_utils.h"

void afficher_menu() {
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   SCANNER DE BULLETINS SCOLAIRES     ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("1. 📸 Scanner les bulletins (AUTO)\n");
    printf("2. 🔍 Scanner un bulletin spécifique\n");
    printf("3. ✍️  Saisir un bulletin manuellement\n");
    printf("4. 🔍 Chercher un élève (par ID)\n");
    printf("5. 📋 Afficher tous les élèves\n");
    printf("6. 📊 Statistiques détaillées d'un élève\n");
    printf("7. 🗑️  Quitter\n");
    printf("\nVotre choix: ");
}

void scanner_bulletin(sqlite3 *db, const char* chemin) {
    printf("\n═══════════════════════════════════════════════\n");
    printf("📄 TRAITEMENT: %s\n", chemin);
    printf("═══════════════════════════════════════════════\n\n");
    
    printf("🔄 Extraction du texte en cours...\n");
    char *texte = extraire_texte_image(chemin);
    
    if (texte) {
        printf("\n--- Texte extrait (100 premiers caractères) ---\n");
        char preview[101];
        strncpy(preview, texte, 100);
        preview[100] = '\0';
        printf("%s...\n", preview);
        
        printf("\n🔄 Analyse du bulletin...\n");
        Eleve eleve_ocr;
        
        if (analyser_bulletin_texte(texte, &eleve_ocr)) {
            printf("\n✅ Bulletin analysé!\n");
            afficher_eleve(&eleve_ocr);
            
            printf("\n💾 Enregistrer dans la base ? (o/n): ");
            char rep;
            scanf("%c", &rep);
            getchar();
            
            if (rep == 'o' || rep == 'O') {
                if (inserer_eleve(db, &eleve_ocr)) {
                    printf("✅ Bulletin enregistré!\n");
                } else {
                    printf("❌ Erreur d'enregistrement\n");
                }
            }
        } else {
            printf("❌ Impossible d'analyser le bulletin\n");
            printf("💡 Conseil: Essayez la saisie manuelle (option 3)\n");
        }
        
        TessDeleteText(texte);
    } else {
        printf("❌ Erreur lors de l'extraction\n");
    }
    
    printf("\n");
}

int main() {
    sqlite3 *db;
    
    if (!ouvrir_base(&db)) {
        fprintf(stderr, "❌ Impossible d'ouvrir la base de données\n");
        return 1;
    }
    
    if (!creer_tables(db)) {
        fprintf(stderr, "❌ Impossible de créer les tables\n");
        sqlite3_close(db);
        return 1;
    }
    
    printf("✅ Base de données prête\n");
    
    int choix;
    char buffer[10];
    
    while (1) {
        afficher_menu();
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }
        
        choix = atoi(buffer);
        
        switch (choix) {
            case 1: {
                printf("\n╔══════════════════════════════════════════════════╗\n");
                printf("║  SCAN AUTOMATIQUE DES BULLETINS                  ║\n");
                printf("╚══════════════════════════════════════════════════╝\n");
                
                // Scanner les fichiers automatiquement
                const char* fichiers[] = {
                    "images/t1.png",
                    "images/test.pdf"
                };
                
                for (int i = 0; i < 2; i++) {
                    scanner_bulletin(db, fichiers[i]);
                }
                
                printf("\n✅ Scan automatique terminé!\n");
                break;
            }
            
            case 2: {
                printf("\n=== SCANNER UN BULLETIN SPÉCIFIQUE ===\n");
                printf("Chemin de l'image (ex: images/bulletin.jpg): ");
                char chemin[MAX_PATH];
                fgets(chemin, MAX_PATH, stdin);
                nettoyer_chaine(chemin);
                
                scanner_bulletin(db, chemin);
                break;
            }
            
            case 3: {
                saisir_bulletin_manuel(db);
                break;
            }
            
            case 4: {
                printf("\n=== CHERCHER UN ÉLÈVE ===\n");
                printf("ID de l'élève: ");
                int id;
                scanf("%d", &id);
                getchar();
                
                Eleve *eleve = recuperer_eleve(db, id);
                if (eleve) {
                    afficher_eleve(eleve);
                    free(eleve);
                } else {
                    printf("❌ Élève non trouvé\n");
                }
                break;
            }
            
            case 5: {
                lister_tous_eleves(db);
                break;
            }
            
            case 6: {
                printf("\n=== STATISTIQUES DÉTAILLÉES ===\n");
                printf("ID de l'élève: ");
                int id;
                scanf("%d", &id);
                getchar();
                
                Eleve *eleve = recuperer_eleve(db, id);
                if (eleve) {
                    afficher_eleve(eleve);
                    afficher_statistiques_eleve(db, id);
                    free(eleve);
                } else {
                    printf("❌ Élève non trouvé\n");
                }
                break;
            }
            
            case 7: {
                printf("\n👋 Au revoir!\n");
                sqlite3_close(db);
                return 0;
            }
            
            default: {
                printf("❌ Choix invalide\n");
                break;
            }
        }
        
        printf("\nAppuyez sur Entrée pour continuer...");
        getchar();
    }
    
    sqlite3_close(db);
    return 0;
}