// include/gui.h
#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "bulletin.h"
#include "database.h"

typedef struct {
    // Fenêtre principale
    GtkWidget *main_window;
    GtkWidget *treeview_eleves;
    GtkListStore *liststore_eleves;
    GtkWidget *search_entry;
    GtkWidget *statusbar;
    
    // Panneau de détails
    GtkWidget *details_box;
    GtkWidget *lbl_details_title;
    GtkWidget *grid_details_infos;
    GtkWidget *lbl_detail_matricule;
    GtkWidget *lbl_detail_classe;
    GtkWidget *lbl_detail_annee;
    GtkWidget *lbl_detail_moyenne;
    GtkWidget *treeview_details_matieres;
    GtkListStore *liststore_details_matieres;
    GtkWidget *lbl_stats_s1;
    GtkWidget *lbl_stats_s2;
    GtkWidget *btn_modifier_details; // NOUVEAU : Bouton pour modifier un bulletin

    // Dialogue Scan & Édition
    GtkWidget *dialog_scan;
    GtkWidget *filechooser_scan;
    GtkWidget *btn_lancer_scan;
    GtkWidget *btn_scan_save;
    GtkWidget *textview_ocr;
    GtkWidget *expander_ocr;

    // Formulaire d'édition
    GtkWidget *entry_nom;
    GtkWidget *entry_prenom;
    GtkWidget *entry_matricule;
    GtkWidget *entry_classe;
    GtkWidget *entry_annee;
    GtkWidget *treeview_edit_matieres;
    GtkListStore *liststore_edit_matieres;

    // Base de données
    sqlite3 *db;

    // État
    Eleve current_eleve;
    char *selected_file;
    int current_eleve_id; // ID de l'élève actuellement sélectionné dans la liste principale
    gboolean is_new_bulletin; // Pour différencier un nouveau bulletin d'un scan
    gboolean is_editing_existing; // NOUVEAU : Pour savoir si on modifie un bulletin existant
} AppWidgets;

// Prototypes - Initialisation
void init_gui(int argc, char *argv[]);

// Prototypes - Callbacks Menu
void on_menu_scanner_activate(GtkMenuItem *item, AppWidgets *app);
void on_menu_nouveau_activate(GtkMenuItem *item, AppWidgets *app);
void on_menu_supprimer_activate(GtkMenuItem *item, AppWidgets *app);
void on_menu_quit_activate(GtkMenuItem *item, AppWidgets *app);
void on_menu_about_activate(GtkMenuItem *item, AppWidgets *app);

// Prototypes - Callbacks Fenêtre principale
void on_search_changed(GtkSearchEntry *entry, AppWidgets *app);
void on_btn_refresh_clicked(GtkButton *button, AppWidgets *app);
void on_treeview_selection_changed(GtkTreeView *treeview, AppWidgets *app);
void on_btn_modifier_clicked(GtkButton *btn, AppWidgets *app); // NOUVEAU

// Prototypes - Callbacks Dialogue Scan & Édition
void on_filechooser_file_set(GtkFileChooser *chooser, AppWidgets *app);
void on_btn_lancer_scan_clicked(GtkButton *btn, AppWidgets *app);
void on_btn_scan_save_clicked(GtkButton *btn, AppWidgets *app);
void on_btn_add_matiere_clicked(GtkButton *btn, AppWidgets *app);
void on_btn_del_matiere_clicked(GtkButton *btn, AppWidgets *app);
void on_cell_edited(GtkCellRendererText *cell, const gchar *path_string, const gchar *new_text, gpointer data);

// Prototypes - Utilitaires
void load_eleves_from_db(AppWidgets *app, const char* filter);
void update_statusbar(AppWidgets *app, const char *msg);
void clear_scan_form(AppWidgets *app);
void populate_scan_form(AppWidgets *app, Eleve *eleve);
void display_eleve_details(AppWidgets *app, int eleve_id);
void clear_details_panel(AppWidgets *app);

#endif