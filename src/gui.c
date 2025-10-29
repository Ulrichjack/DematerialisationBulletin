// src/gui.c
#include "gui.h"
#include "ocr_utils.h"
#include "bulletin_utils.h"
#include <string.h>

// Structure pour passer les données aux callbacks d'édition de cellule
typedef struct {
    AppWidgets *app;
    int column_id;
} CellEditData;


// ========== CALLBACKS MENU ==========

void on_menu_scanner_activate(GtkMenuItem *item, AppWidgets *app) {
    clear_scan_form(app);
    app->is_new_bulletin = FALSE;
    app->is_editing_existing = FALSE;
    gtk_window_set_title(GTK_WINDOW(app->dialog_scan), "Scanner et Éditer un Bulletin");
    
    // Montrer les éléments de scan
    gtk_widget_set_visible(app->filechooser_scan, TRUE);
    gtk_widget_set_visible(app->btn_lancer_scan, TRUE);
    gtk_widget_set_visible(app->expander_ocr, TRUE);
    
    gtk_widget_show_all(GTK_WIDGET(app->dialog_scan));
    gtk_dialog_run(GTK_DIALOG(app->dialog_scan));
    gtk_widget_hide(GTK_WIDGET(app->dialog_scan));
}

void on_menu_nouveau_activate(GtkMenuItem *item, AppWidgets *app) {
    clear_scan_form(app);
    app->is_new_bulletin = TRUE;
    app->is_editing_existing = FALSE;
    gtk_window_set_title(GTK_WINDOW(app->dialog_scan), "Nouveau Bulletin Manuel");
    
    // Pour un nouveau bulletin, on n'a pas besoin de la partie scan
    gtk_widget_set_visible(app->filechooser_scan, FALSE);
    gtk_widget_set_visible(app->btn_lancer_scan, FALSE);
    gtk_widget_set_visible(app->expander_ocr, FALSE);
    
    gtk_widget_set_sensitive(app->btn_scan_save, TRUE);
    gtk_widget_show_all(GTK_WIDGET(app->dialog_scan));
    update_statusbar(app, "Prêt pour la saisie manuelle d'un nouveau bulletin.");
    
    gtk_dialog_run(GTK_DIALOG(app->dialog_scan));
    gtk_widget_hide(GTK_WIDGET(app->dialog_scan));
}

void on_menu_supprimer_activate(GtkMenuItem *item, AppWidgets *app) {
    if (app->current_eleve_id <= 0) {
        update_statusbar(app, "Veuillez d'abord sélectionner un élève à supprimer.");
        return;
    }

    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(app->main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Êtes-vous sûr de vouloir supprimer cet élève ?\nToutes ses données seront perdues.");
    
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (result == GTK_RESPONSE_YES) {
        if (supprimer_eleve(app->db, app->current_eleve_id)) {
            update_statusbar(app, "Élève supprimé avec succès.");
            app->current_eleve_id = -1;
            clear_details_panel(app);
            load_eleves_from_db(app, NULL);
        } else {
            update_statusbar(app, "Erreur lors de la suppression de l'élève.");
        }
    }
}


void on_menu_quit_activate(GtkMenuItem *item, AppWidgets *app) {
    g_print("👋 Fermeture de l'application...\n");
    gtk_main_quit();
}

void on_menu_about_activate(GtkMenuItem *item, AppWidgets *app) {
    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "Bulletin Scanner");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.2");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
        "Application de dématérialisation des bulletins scolaires\n"
        "Scan OCR avec Tesseract et stockage SQLite\n\n"
        "Fonctionnalités :\n"
        "• Scanner des bulletins PDF/Image\n"
        "• Saisie manuelle de bulletins\n"
        "• Modification des bulletins existants\n"
        "• Gestion complète de la base de données");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "© 2025");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_GPL_3_0);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// ========== CALLBACKS FENÊTRE PRINCIPALE ==========

void on_search_changed(GtkSearchEntry *entry, AppWidgets *app) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
    load_eleves_from_db(app, text);
}

void on_btn_refresh_clicked(GtkButton *btn, AppWidgets *app) {
    g_print("🔄 Actualisation de la liste...\n");
    gtk_entry_set_text(GTK_ENTRY(app->search_entry), "");
    load_eleves_from_db(app, NULL);
    update_statusbar(app, "Liste actualisée");
}

void on_treeview_selection_changed(GtkTreeView *treeview, AppWidgets *app) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint id;
        gtk_tree_model_get(model, &iter, 0, &id, -1);
        if (id != app->current_eleve_id) {
            app->current_eleve_id = id;
            display_eleve_details(app, id);
        }
    }
}

// NOUVEAU : Callback pour le bouton Modifier
void on_btn_modifier_clicked(GtkButton *btn, AppWidgets *app) {
    if (app->current_eleve_id <= 0) {
        update_statusbar(app, "Veuillez d'abord sélectionner un élève à modifier.");
        return;
    }

    // Récupérer les données de l'élève
    Eleve *eleve = recuperer_eleve(app->db, app->current_eleve_id);
    if (!eleve) {
        update_statusbar(app, "Erreur lors du chargement des données de l'élève.");
        return;
    }

    // Préparer le dialogue pour modification
    clear_scan_form(app);
    app->is_new_bulletin = FALSE;
    app->is_editing_existing = TRUE;
    app->current_eleve = *eleve;
    
    gtk_window_set_title(GTK_WINDOW(app->dialog_scan), "Modifier le Bulletin");
    
    // Cacher les éléments de scan
    gtk_widget_set_visible(app->filechooser_scan, FALSE);
    gtk_widget_set_visible(app->btn_lancer_scan, FALSE);
    gtk_widget_set_visible(app->expander_ocr, FALSE);
    
    // Remplir le formulaire avec les données existantes
    populate_scan_form(app, eleve);
    gtk_widget_set_sensitive(app->btn_scan_save, TRUE);
    
    free(eleve);
    
    gtk_widget_show_all(GTK_WIDGET(app->dialog_scan));
    update_statusbar(app, "Mode édition - Modifiez les données et enregistrez.");
    
    gtk_dialog_run(GTK_DIALOG(app->dialog_scan));
    gtk_widget_hide(GTK_WIDGET(app->dialog_scan));
}

// ========== CALLBACKS DIALOGUE SCAN & ÉDITION ==========

void on_filechooser_file_set(GtkFileChooser *chooser, AppWidgets *app) {
    char *filename = gtk_file_chooser_get_filename(chooser);
    if (filename) {
        if (app->selected_file) g_free(app->selected_file);
        app->selected_file = g_strdup(filename);
        g_free(filename);
        gtk_widget_set_sensitive(app->btn_lancer_scan, TRUE);
        update_statusbar(app, "Fichier sélectionné. Prêt à lancer le scan.");
    }
}

void on_btn_lancer_scan_clicked(GtkButton *btn, AppWidgets *app) {
    if (!app->selected_file) return;

    update_statusbar(app, "🔄 Extraction OCR en cours...");
    gtk_widget_set_sensitive(GTK_WIDGET(btn), FALSE);
    
    // Forcer le rafraîchissement de l'interface
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
    
    char *texte = extraire_texte_image(app->selected_file);
    if (!texte) {
        update_statusbar(app, "❌ Erreur lors de l'extraction OCR");
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->dialog_scan), 
            GTK_DIALOG_MODAL, 
            GTK_MESSAGE_ERROR, 
            GTK_BUTTONS_OK, 
            "Impossible d'extraire le texte. Vérifiez Tesseract et le fichier.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        gtk_widget_set_sensitive(GTK_WIDGET(btn), TRUE);
        return;
    }

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->textview_ocr));
    gtk_text_buffer_set_text(buffer, texte, -1);
    gtk_expander_set_expanded(GTK_EXPANDER(app->expander_ocr), TRUE);

    Eleve eleve;
    if (analyser_bulletin_texte(texte, &eleve)) {
        app->current_eleve = eleve;
        populate_scan_form(app, &eleve);
        gtk_widget_set_sensitive(app->btn_scan_save, TRUE);
        update_statusbar(app, "✅ Bulletin analysé. Vérifiez et modifiez les données si besoin.");
    } else {
        update_statusbar(app, "⚠️ Analyse automatique échouée. Veuillez remplir les champs manuellement.");
        gtk_widget_set_sensitive(app->btn_scan_save, TRUE); // Permettre la saisie manuelle
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->dialog_scan), 
            GTK_DIALOG_MODAL, 
            GTK_MESSAGE_WARNING, 
            GTK_BUTTONS_OK, 
            "L'analyse automatique a échoué.\nVous pouvez remplir les champs manuellement.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    TessDeleteText(texte);
    gtk_widget_set_sensitive(GTK_WIDGET(btn), TRUE);
}

void on_btn_scan_save_clicked(GtkButton *btn, AppWidgets *app) {
    // 1. Récupérer infos élève depuis le formulaire
    strncpy(app->current_eleve.nom, gtk_entry_get_text(GTK_ENTRY(app->entry_nom)), MAX_NOM - 1);
    strncpy(app->current_eleve.prenom, gtk_entry_get_text(GTK_ENTRY(app->entry_prenom)), MAX_PRENOM - 1);
    strncpy(app->current_eleve.matricule, gtk_entry_get_text(GTK_ENTRY(app->entry_matricule)), MAX_MATRICULE - 1);
    strncpy(app->current_eleve.classe, gtk_entry_get_text(GTK_ENTRY(app->entry_classe)), MAX_CLASSE - 1);
    strncpy(app->current_eleve.annee_academique, gtk_entry_get_text(GTK_ENTRY(app->entry_annee)), MAX_ANNEE - 1);
    
    if (strlen(app->current_eleve.nom) == 0 || strlen(app->current_eleve.prenom) == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->dialog_scan), 
            GTK_DIALOG_MODAL, 
            GTK_MESSAGE_ERROR, 
            GTK_BUTTONS_OK, 
            "Le nom et le prénom sont obligatoires !");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    // 2. Récupérer les matières depuis le GtkListStore
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app->liststore_edit_matieres), &iter);
    int i = 0;
    while (valid && i < MAX_MATIERES) {
        gchar *nom_matiere;
        gdouble note, coef;
        
        gtk_tree_model_get(GTK_TREE_MODEL(app->liststore_edit_matieres), &iter, 
                          0, &nom_matiere, 
                          1, &note, 
                          2, &coef, 
                          -1);
        
        strncpy(app->current_eleve.matieres[i].nom_matiere, nom_matiere, MAX_MATIERE - 1);
        app->current_eleve.matieres[i].note = note;
        app->current_eleve.matieres[i].coefficient = coef;
        strcpy(app->current_eleve.matieres[i].appreciation, note >= 10 ? "Validé" : "");

        g_free(nom_matiere);
        i++;
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(app->liststore_edit_matieres), &iter);
    }
    app->current_eleve.nombre_matieres = i;

    // 3. Conserver l'ID si on modifie un bulletin existant
    if (app->is_editing_existing) {
        app->current_eleve.id = app->current_eleve_id;
    }

    // 4. Recalculer la moyenne et sauvegarder
    calculer_moyenne(&app->current_eleve);

    if (inserer_eleve(app->db, &app->current_eleve)) {
        update_statusbar(app, "✅ Bulletin enregistré avec succès !");
        load_eleves_from_db(app, NULL);
        
        // Recharger les détails si on modifiait un bulletin existant
        if (app->is_editing_existing) {
            display_eleve_details(app, app->current_eleve_id);
        }
        
        gtk_widget_hide(GTK_WIDGET(app->dialog_scan));
    } else {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->dialog_scan), 
            GTK_DIALOG_MODAL, 
            GTK_MESSAGE_ERROR, 
            GTK_BUTTONS_OK, 
            "Erreur lors de l'enregistrement.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

void on_btn_add_matiere_clicked(GtkButton *btn, AppWidgets *app) {
    GtkTreeIter iter;
    gtk_list_store_append(app->liststore_edit_matieres, &iter);
    gtk_list_store_set(app->liststore_edit_matieres, &iter,
                       0, "[S1-CP] NOUVELLE_MATIERE - Nom",
                       1, 10.0,
                       2, 1.0,
                       -1);
}

void on_btn_del_matiere_clicked(GtkButton *btn, AppWidgets *app) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->treeview_edit_matieres));
    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    } else {
        update_statusbar(app, "Veuillez sélectionner une matière à supprimer.");
    }
}

void on_cell_edited(GtkCellRendererText *cell, const gchar *path_string, const gchar *new_text, gpointer data) {
    CellEditData *edit_data = (CellEditData *)data;
    AppWidgets *app = edit_data->app;
    int column = edit_data->column_id;

    GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(GTK_TREE_MODEL(app->liststore_edit_matieres), &iter, path);

    switch (column) {
        case 0: // Nom matière
            gtk_list_store_set(app->liststore_edit_matieres, &iter, column, new_text, -1);
            break;
        case 1: // Note
        case 2: // Coef
            {
                gdouble value = g_strtod(new_text, NULL);
                gtk_list_store_set(app->liststore_edit_matieres, &iter, column, value, -1);
            }
            break;
    }
    gtk_tree_path_free(path);
}


// ========== UTILITAIRES ==========

void update_statusbar(AppWidgets *app, const char *msg) {
    static guint context_id = 0;
    if (!context_id) context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(app->statusbar), "main");
    gtk_statusbar_pop(GTK_STATUSBAR(app->statusbar), context_id);
    gtk_statusbar_push(GTK_STATUSBAR(app->statusbar), context_id, msg);
    while (gtk_events_pending()) gtk_main_iteration();
}

void clear_scan_form(AppWidgets *app) {
    gtk_entry_set_text(GTK_ENTRY(app->entry_nom), "");
    gtk_entry_set_text(GTK_ENTRY(app->entry_prenom), "");
    gtk_entry_set_text(GTK_ENTRY(app->entry_matricule), "");
    gtk_entry_set_text(GTK_ENTRY(app->entry_classe), "");
    gtk_entry_set_text(GTK_ENTRY(app->entry_annee), "");
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->textview_ocr));
    gtk_text_buffer_set_text(buffer, "", -1);
    gtk_list_store_clear(app->liststore_edit_matieres);
    
    gtk_widget_set_sensitive(app->btn_scan_save, FALSE);
    gtk_widget_set_sensitive(app->btn_lancer_scan, FALSE);

    if (app->selected_file) {
        g_free(app->selected_file);
        app->selected_file = NULL;
    }
    
    // Réinitialiser le file chooser
    gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->filechooser_scan));
}

void populate_scan_form(AppWidgets *app, Eleve *eleve) {
    gtk_entry_set_text(GTK_ENTRY(app->entry_nom), eleve->nom);
    gtk_entry_set_text(GTK_ENTRY(app->entry_prenom), eleve->prenom);
    gtk_entry_set_text(GTK_ENTRY(app->entry_matricule), eleve->matricule);
    gtk_entry_set_text(GTK_ENTRY(app->entry_classe), eleve->classe);
    gtk_entry_set_text(GTK_ENTRY(app->entry_annee), eleve->annee_academique);

    gtk_list_store_clear(app->liststore_edit_matieres);
    for (int i = 0; i < eleve->nombre_matieres; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(app->liststore_edit_matieres, &iter);
        gtk_list_store_set(app->liststore_edit_matieres, &iter,
                           0, eleve->matieres[i].nom_matiere,
                           1, eleve->matieres[i].note,
                           2, eleve->matieres[i].coefficient,
                           -1);
    }
}

void load_eleves_from_db(AppWidgets *app, const char* filter) {
    gtk_list_store_clear(app->liststore_eleves);

    char sql[512];
    if (filter && strlen(filter) > 0) {
        snprintf(sql, sizeof(sql), 
                "SELECT id, nom, prenom, matricule, classe, moyenne_generale, annee_academique "
                "FROM eleves WHERE nom LIKE '%%%s%%' OR prenom LIKE '%%%s%%' OR matricule LIKE '%%%s%%' "
                "ORDER BY nom, prenom;", 
                filter, filter, filter);
    } else {
        strcpy(sql, "SELECT id, nom, prenom, matricule, classe, moyenne_generale, annee_academique "
                   "FROM eleves ORDER BY id DESC;");
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, 0) != SQLITE_OK) return;

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GtkTreeIter iter;
        gtk_list_store_append(app->liststore_eleves, &iter);
        gtk_list_store_set(app->liststore_eleves, &iter,
                           0, sqlite3_column_int(stmt, 0),
                           1, sqlite3_column_text(stmt, 1),
                           2, sqlite3_column_text(stmt, 2),
                           3, sqlite3_column_text(stmt, 3),
                           4, sqlite3_column_text(stmt, 4),
                           5, sqlite3_column_double(stmt, 5),
                           6, sqlite3_column_text(stmt, 6),
                           -1);
        count++;
    }
    sqlite3_finalize(stmt);

    char msg[100];
    snprintf(msg, sizeof(msg), "%d élève(s) trouvé(s).", count);
    update_statusbar(app, msg);
}


void display_eleve_details(AppWidgets *app, int eleve_id) {
    Eleve *e = recuperer_eleve(app->db, eleve_id);
    if (!e) {
        clear_details_panel(app);
        return;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s %s", e->prenom, e->nom);
    gtk_label_set_text(GTK_LABEL(app->lbl_details_title), buffer);

    gtk_label_set_text(GTK_LABEL(app->lbl_detail_matricule), e->matricule);
    gtk_label_set_text(GTK_LABEL(app->lbl_detail_classe), e->classe);
    gtk_label_set_text(GTK_LABEL(app->lbl_detail_annee), e->annee_academique);
    snprintf(buffer, sizeof(buffer), "<b>%.2f / 20</b>", e->moyenne_generale);
    gtk_label_set_markup(GTK_LABEL(app->lbl_detail_moyenne), buffer);

    gtk_list_store_clear(app->liststore_details_matieres);
    int credits_s1 = 0, credits_s2 = 0;
    int valides_s1 = 0, valides_s2 = 0;
    int total_s1 = 0, total_s2 = 0;

    for (int i = 0; i < e->nombre_matieres; i++) {
        MetadataMatiere meta;
        extraire_metadata_matiere(e->matieres[i].nom_matiere, &meta);

        if (meta.est_module) continue;

        GtkTreeIter iter;
        gtk_list_store_append(app->liststore_details_matieres, &iter);
        gtk_list_store_set(app->liststore_details_matieres, &iter,
                           0, e->matieres[i].nom_matiere,
                           1, e->matieres[i].note,
                           2, e->matieres[i].coefficient,
                           3, e->matieres[i].appreciation,
                           4, meta.semestre,
                           -1);
        
        int est_valide = (e->matieres[i].note >= 10.0);
        if (meta.semestre == 1) {
            total_s1++;
            if(est_valide) { valides_s1++; credits_s1 += e->matieres[i].coefficient; }
        } else {
            total_s2++;
            if(est_valide) { valides_s2++; credits_s2 += e->matieres[i].coefficient; }
        }
    }
    
    snprintf(buffer, sizeof(buffer), "<b>Bilan Semestre 1 :</b> %d/%d matières validées (%d crédits)", 
             valides_s1, total_s1, credits_s1);
    gtk_label_set_markup(GTK_LABEL(app->lbl_stats_s1), buffer);
    gtk_widget_set_visible(app->lbl_stats_s1, TRUE);

    if (total_s2 > 0) {
        snprintf(buffer, sizeof(buffer), "<b>Bilan Semestre 2 :</b> %d/%d matières validées (%d crédits)", 
                 valides_s2, total_s2, credits_s2);
        gtk_label_set_markup(GTK_LABEL(app->lbl_stats_s2), buffer);
        gtk_widget_set_visible(app->lbl_stats_s2, TRUE);
    } else {
        gtk_widget_set_visible(app->lbl_stats_s2, FALSE);
    }

    gtk_widget_set_visible(app->grid_details_infos, TRUE);
    gtk_widget_set_visible(app->treeview_details_matieres, TRUE);
    gtk_widget_set_visible(app->btn_modifier_details, TRUE); // Afficher le bouton modifier
    
    free(e);
}

void clear_details_panel(AppWidgets *app) {
    gtk_label_set_text(GTK_LABEL(app->lbl_details_title), "Sélectionnez un élève pour voir les détails");
    gtk_widget_set_visible(app->grid_details_infos, FALSE);
    gtk_widget_set_visible(app->treeview_details_matieres, FALSE);
    gtk_widget_set_visible(app->lbl_stats_s1, FALSE);
    gtk_widget_set_visible(app->lbl_stats_s2, FALSE);
    gtk_widget_set_visible(app->btn_modifier_details, FALSE); // Cacher le bouton modifier
    gtk_list_store_clear(app->liststore_details_matieres);
}


// ========== INIT GUI ==========

void init_gui(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkBuilder *builder = gtk_builder_new_from_file("ui/bulletin_app.glade");
    AppWidgets *app = g_new0(AppWidgets, 1);
    app->current_eleve_id = -1;
    app->is_new_bulletin = FALSE;
    app->is_editing_existing = FALSE;

    // Fenêtre principale et détails
    app->main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    app->treeview_eleves = GTK_WIDGET(gtk_builder_get_object(builder, "treeview_eleves"));
    app->liststore_eleves = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_eleves"));
    app->search_entry = GTK_WIDGET(gtk_builder_get_object(builder, "search_entry"));
    app->statusbar = GTK_WIDGET(gtk_builder_get_object(builder, "statusbar"));
    app->details_box = GTK_WIDGET(gtk_builder_get_object(builder, "details_box"));
    app->lbl_details_title = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_details_title"));
    app->grid_details_infos = GTK_WIDGET(gtk_builder_get_object(builder, "grid_details_infos"));
    app->lbl_detail_matricule = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_detail_matricule"));
    app->lbl_detail_classe = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_detail_classe"));
    app->lbl_detail_annee = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_detail_annee"));
    app->lbl_detail_moyenne = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_detail_moyenne"));
    app->treeview_details_matieres = GTK_WIDGET(gtk_builder_get_object(builder, "treeview_details_matieres"));
    app->liststore_details_matieres = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_details_matieres"));
    app->lbl_stats_s1 = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stats_s1"));
    app->lbl_stats_s2 = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stats_s2"));
    app->btn_modifier_details = GTK_WIDGET(gtk_builder_get_object(builder, "btn_modifier_details"));

    // Dialogue scan & édition
    app->dialog_scan = GTK_WIDGET(gtk_builder_get_object(builder, "dialog_scan"));
    app->filechooser_scan = GTK_WIDGET(gtk_builder_get_object(builder, "filechooser_scan"));
    app->btn_lancer_scan = GTK_WIDGET(gtk_builder_get_object(builder, "btn_lancer_scan"));
    app->btn_scan_save = GTK_WIDGET(gtk_builder_get_object(builder, "btn_scan_save"));
    app->textview_ocr = GTK_WIDGET(gtk_builder_get_object(builder, "textview_ocr"));
    app->expander_ocr = GTK_WIDGET(gtk_builder_get_object(builder, "expander_ocr"));
    app->entry_nom = GTK_WIDGET(gtk_builder_get_object(builder, "entry_nom"));
    app->entry_prenom = GTK_WIDGET(gtk_builder_get_object(builder, "entry_prenom"));
    app->entry_matricule = GTK_WIDGET(gtk_builder_get_object(builder, "entry_matricule"));
    app->entry_classe = GTK_WIDGET(gtk_builder_get_object(builder, "entry_classe"));
    app->entry_annee = GTK_WIDGET(gtk_builder_get_object(builder, "entry_annee"));
    app->treeview_edit_matieres = GTK_WIDGET(gtk_builder_get_object(builder, "treeview_edit_matieres"));
    app->liststore_edit_matieres = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore_edit_matieres"));

    // Connexion des signaux
    gtk_builder_connect_signals(builder, app);
    
    // Connexion manuelle du bouton Modifier dans le panneau de détails
    g_signal_connect(app->btn_modifier_details, "clicked", G_CALLBACK(on_btn_modifier_clicked), app);
    
    // Configuration de l'édition des cellules
    static CellEditData edit_data[3];
    const char* cell_ids[] = {"cell_edit_nom", "cell_edit_note", "cell_edit_coef"};
    for (int i=0; i<3; i++) {
        GtkCellRenderer *renderer = GTK_CELL_RENDERER(gtk_builder_get_object(builder, cell_ids[i]));
        edit_data[i] = (CellEditData){app, i};
        g_signal_connect(renderer, "edited", G_CALLBACK(on_cell_edited), &edit_data[i]);
    }

    // DB
    if (!ouvrir_base(&app->db) || !creer_tables(app->db)) {
        g_critical("❌ Erreur base de données");
        return;
    }

    load_eleves_from_db(app, NULL);
    clear_details_panel(app);
    update_statusbar(app, "✅ Prêt");

    gtk_widget_show_all(app->main_window);
    gtk_widget_hide(app->dialog_scan);
    
    gtk_main();

    sqlite3_close(app->db);
    g_object_unref(builder);
    if (app->selected_file) g_free(app->selected_file);
    g_free(app);
}