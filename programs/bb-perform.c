/* TODO:
     saved-edit-buffer
       - write_config()
       - call bb_xml_iterate("BB/Saved")
       - handle__saved_scripts_window()
       - handle__save_script()
 */
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <gsk/gskutils.h>
#include <gsk/gsklistmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "mix.h"
#include "bb-sample-widget.h"
#include "bb-perform-interpreter.h"
#include "../core/bb-xml.h"
#include <gtk/gtk.h>

GtkWidget *script_window, *seq_window, *scrolled_window, *sequence_vbox,
          *vbox, *label_hbox, *label_label, *label_entry,
          *text_view, *go_button, *interp_combo_box,
          *hbox, *buf_scrolled, *buf_sel_box;
GtkWidget *time_seq_vbox, *time_scrollbar;
GtkAdjustment *time_adjustment;
GtkActionGroup *action_group;
GtkUIManager *manager;


static BbPerformInterpreter *default_interpreter;

static gdouble rate;

char *dir;
char *status_fname;

static gdouble beat_period = 60.0 / 90.0;


static const char *tmp_dir = ".";

typedef struct _Seq Seq;
struct _Seq
{
  int pid;              /* nonzero only during construction */
  guint sequence_index; /* mix's notion of index */
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *box;
  GtkAdjustment *volume_adj;
  GtkWidget *volume;
  GtkWidget *sample_widget;

  GtkWidget *offset_entry;              /* gtkentry */
  GtkWidget *offset_units;              /* a text combobox */
  char *fname;

  guint offset;         /* in samples */

  /* set when the offset_entry is being changed
     due to the units changing */
  guint units_changing : 1;

  guint n_samples;
  gdouble *samples;

  Seq *prev, *next;
};
static Seq *first_seq, *last_seq;
#define GET_SEQ_LIST()    Seq *, first_seq, last_seq, prev, next


typedef struct _Script Script;
struct _Script
{
  GtkTextBuffer *text_buffer;
  GtkWidget *button;
  GtkWidget *label;
  BbPerformInterpreter *interpreter;
};
Script *selected_script;
GPtrArray *scripts;


static guint write_status_timer_id = 0;

static void write_config (void);

static gboolean
handle_write_status_timer_expired (gpointer data)
{
  write_status_timer_id = 0;
  write_config ();
  return FALSE;
}

static void
notify_something_changed (void)
{
  g_timeout_add (100, handle_write_status_timer_expired, NULL);
}

static char *
gen_base (void)
{
  static char *base = NULL;
  static guint seq_no = 0;
  if (base == NULL)
    base = g_strdup_printf ("tmp%06u%10u", getpid (), (guint)time(NULL));
  return g_strdup_printf ("%s.%08u", base, seq_no++);
}

/* --- Saved Scripts Window --- */
static GArray *saved_scripts;
static GtkListStore *list_store;
static GtkWidget *saved_scripts_tree_view;
static GtkWidget *saved_scripts_window;
static gint savedpopup_index;

typedef struct _BbPerformSavedScript BbPerformSavedScript;
struct _BbPerformSavedScript
{
  char *name;
  char *description;
  char *text;
  BbPerformInterpreter *interpreter;
};

static inline void
ensure_saved_scripts (void)
{
  if (saved_scripts == NULL)
    {
      saved_scripts = g_array_new (FALSE, FALSE, sizeof (BbPerformSavedScript));
      list_store = gtk_list_store_new (3,
                                       G_TYPE_STRING,
                                       G_TYPE_STRING,
                                       G_TYPE_STRING);
    }
}

/* Add a saved script to the saved_scripts array
   and the list_store. */
static void
save_script (const char           *name,
             const char           *description,
             const char           *text,
             BbPerformInterpreter *interpreter)
{
  BbPerformSavedScript ss;
  GtkTreeIter iter;
  ensure_saved_scripts ();
  ss.name = g_strdup (name);
  ss.description = g_strdup (description);
  ss.text = g_strdup (text);
  ss.interpreter = interpreter;
  g_array_append_val (saved_scripts, ss);
  gtk_list_store_append (list_store, &iter);
  g_message ("gtk_list_store_append");
  gtk_list_store_set (list_store, &iter,
                      0, name,
                      1, description,
                      2, interpreter->name,
                      -1);
}

/* Parse a <Saved> node from the config xml */
static void
saved_scripts_load_xml   (BbXml *saved_xml)
{
  BbXml *name_node, *desc_node, *text_node, *interp_node;
  char *name, *desc, *text, *interp;
  BbPerformInterpreter *interpreter;
  name_node = bb_xml_find_child (saved_xml, "name");
  desc_node = bb_xml_find_child (saved_xml, "description");
  text_node = bb_xml_find_child (saved_xml, "text");
  interp_node = bb_xml_find_child (saved_xml, "interpreter");
  if (name_node == NULL)
    g_error ("missing <name> under <Saved>");
  if (desc_node == NULL)
    g_error ("missing <description> under <Saved>");
  if (text_node == NULL)
    g_error ("missing <text> under <Saved>");
  if (interp_node == NULL)
    g_error ("missing <interpreter> under <Saved>");
  name = bb_xml_get_all_text (name_node);
  desc = bb_xml_get_all_text (desc_node);
  text = bb_xml_get_all_text (text_node);
  interp = bb_xml_get_all_text (interp_node);
  interpreter = bb_perform_interpreter_find_by_name (interp);
  if (interpreter == NULL)
    g_error ("unknown interpreter %s (processing <Saved>)", interp);
  save_script (name, desc, text, interpreter);
  g_free (name);
  g_free (desc);
  g_free (text);
  g_free (interp);
}

/* Create <Saved> nodes. */
static void
saved_scripts_append_xml (BbXml *parent)
{
  guint i;
  if (saved_scripts == NULL)
    return;
  for (i = 0; i < saved_scripts->len; i++)
    {
      BbPerformSavedScript ss = g_array_index (saved_scripts, BbPerformSavedScript, i);
      BbXml *saved = bb_xml_new_node ("Saved", NULL);
      bb_xml_add_text_child (saved, "name", ss.name);
      bb_xml_add_text_child (saved, "description", ss.description);
      bb_xml_add_text_child (saved, "text", ss.text);
      bb_xml_add_text_child (saved, "interpreter", ss.interpreter->name);
      bb_xml_add_child (parent, saved);
    }
}

/* Saved-Node GUI */
static void
saved_scripts_window_destroyed (void)
{
  saved_scripts_tree_view = NULL;
  saved_scripts_window = NULL;
}

static gboolean
handle_button_press_event (GtkTreeView *view,
                           GdkEventButton *event)
{
  GtkWidget *menu = gtk_ui_manager_get_widget (manager, "/savedpopup");
  GtkTreePath *path;
  const gint *indices;
  if (!gtk_tree_view_get_path_at_pos (view, event->x, event->y, &path, NULL, NULL, NULL))
    {
      g_warning ("could not get path from pos in tree-view");
      return FALSE;
    }
  indices = gtk_tree_path_get_indices (path);
  savedpopup_index = indices[0];
  gtk_tree_path_free (path);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
  return TRUE;
}

static void
save_script_window_raise_or_create (void)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  if (saved_scripts_window != NULL)
    {
      gdk_window_raise (saved_scripts_window->window);
      return;
    }
  ensure_saved_scripts ();
  saved_scripts_tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (saved_scripts_tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (saved_scripts_tree_view), column);
  column = gtk_tree_view_column_new_with_attributes ("Description", renderer, "text", 1, NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (saved_scripts_tree_view), column);
  column = gtk_tree_view_column_new_with_attributes ("Interpreter", renderer, "text", 2, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (saved_scripts_tree_view), column);
  saved_scripts_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (saved_scripts_window, "destroy", G_CALLBACK (saved_scripts_window_destroyed), NULL);
  gtk_container_add (GTK_CONTAINER (saved_scripts_window), saved_scripts_tree_view);
  g_signal_connect (saved_scripts_tree_view, "button-press-event",
                    G_CALLBACK (handle_button_press_event), NULL);
  gtk_widget_show_all (saved_scripts_window);
}

/* --- Sequence Handling --- */
static void
seq_volume_value_changed (GtkAdjustment *adj,
                          Seq           *seq)
{
  mix_set_volume (seq->sequence_index, adj->value);
}

static void
handle_child_done (GPid     pid,
                   int      status,
                   gpointer data)
{
  Seq *seq = data;
  int fd;
  gssize r;
  guint got;
  guint n_slab = mix_get_n_samples ();
  gdouble *slab = g_new (gdouble, n_slab);
  double max_abs, factor;
  guint i;
  guint cur_time;
  g_message ("handle_child_done: status=%u",status);
  if (status != 0)
    {
      g_warning ("failed to make audio: status %u", status);
      gtk_widget_destroy (seq->box);
      GSK_LIST_REMOVE (GET_SEQ_LIST (), seq);
      g_free (seq);
      return;
    }
  fd = open (seq->fname, O_RDONLY);
  if (fd < 0)
    g_error ("error reading output file %s: %s",
             seq->fname, g_strerror (errno));
  r = gsk_readn (fd, slab, n_slab * sizeof (double));
  if (r < 0)
    g_error ("error reading from %s: %s", seq->fname, g_strerror (errno));
  if (r % sizeof (gdouble) != 0)
    g_error ("output file's size was not a multiple of sizeof(double)==%u",
             (guint) sizeof(gdouble));
  got = r / sizeof (gdouble);
  g_message ("got %u of %u samples", got,n_slab);
  max_abs = 0.00001;
  for (i = 0; i < got; i++)
    max_abs = MAX (max_abs, ABS (slab[i]));
  factor = 1.0 / max_abs;
  for (i = 0; i < got; i++)
    slab[i] *= factor;


  while (got < n_slab)
    slab[got++] = 0;

  seq->pid = 0;
  seq->sequence_index = mix_add_sequence (n_slab, slab);

  seq->sample_widget = bb_sample_widget_new (n_slab, slab, rate);
  gtk_widget_set_size_request (seq->sample_widget, -1, 64);
  gtk_box_pack_start_defaults (GTK_BOX (seq->vbox), seq->sample_widget);
  gtk_widget_show (seq->sample_widget);
  seq->samples = slab;
  seq->n_samples = n_slab;
  mix_sequence_get_time (seq->sequence_index, &cur_time, NULL, NULL);
  g_object_set (G_OBJECT (seq->sample_widget), "time", cur_time, "has-time", TRUE, NULL);

  g_signal_connect (seq->volume_adj, "value-changed", G_CALLBACK (seq_volume_value_changed), seq);
  gtk_widget_set_sensitive (seq->box, TRUE);
}

static void
kill_seq (GtkButton *button,
          Seq       *seq)
{
  GSK_LIST_REMOVE (GET_SEQ_LIST (), seq);
  mix_remove_sequence (seq->sequence_index);
  unlink (seq->fname);
  g_free (seq->fname);
  gtk_widget_destroy (seq->frame);
  g_free (seq->samples);
  g_free (seq);
}

static void
handle_offset_entry_changed (GtkEntry *entry,
                             Seq      *seq)
{
  const char *txt;
  double val;
  double samples;
  if (seq->units_changing)
    return;
  val = strtod (gtk_entry_get_text (entry), NULL);
  txt = gtk_combo_box_get_active_text (GTK_COMBO_BOX (seq->offset_units));

  if (strcmp (txt, "beats") == 0)
    samples = val * rate * beat_period;
  else if (strcmp (txt, "seconds") == 0)
    samples = val * rate;
  else if (strcmp (txt, "samples") == 0)
    samples = val;
  else
    g_return_if_reached ();

  samples = fmod (samples, mix_get_n_samples ());
  if (samples < 0.0)
    samples += mix_get_n_samples ();
  mix_sequence_set_offset (seq->sequence_index,  (guint) samples);
  g_message ("mix_sequence_set_offset: %f",samples);
}

static void
handle_offset_units_changed (GtkComboBox *box,
                             Seq         *seq)
{
  gdouble val;
  char buf[128];
  const char *txt = gtk_combo_box_get_active_text (box);
  if (strcmp (txt, "beats") == 0)
    val = (double)seq->offset / rate / beat_period;
  else if (strcmp (txt, "seconds") == 0)
    val = (double)seq->offset / rate;
  else if (strcmp (txt, "samples") == 0)
    val = (double)seq->offset;
  else
    g_return_if_reached ();

  seq->units_changing = 1;
  g_snprintf (buf, sizeof (buf), "%g", val);
  gtk_entry_set_text (GTK_ENTRY (seq->offset_entry), buf);
  seq->units_changing = 0;
}

/* --- Edit Buffer Window --- */
static char *
edit_buffer_get_text (Script *scr)
{
  GtkTextIter start_iter, end_iter;
  gtk_text_buffer_get_start_iter (scr->text_buffer, &start_iter);
  gtk_text_buffer_get_end_iter (scr->text_buffer, &end_iter);
  return gtk_text_buffer_get_text (scr->text_buffer, &start_iter, &end_iter, FALSE);
}

static void
handle_go (void)
{
  char *text;
  char *base = gen_base ();
  char *ifilename = g_strdup_printf ("%s.in", base);
  char *ofilename = g_strdup_printf ("%s.out", base);
  FILE *fp = fopen (ifilename, "w");
  int rv;
  Seq *seq;
  GtkWidget *label, *kill_button;
  if (selected_script == NULL)
    {
      g_warning ("go clicked but no buffer selected");
      return;
    }
  text = edit_buffer_get_text (selected_script);
  fprintf (fp, "%s\n", text);
  fclose (fp);

  rv = bb_perform_interpreter_launch (selected_script->interpreter,
                                      ifilename, ofilename,
                                      rate);

  g_free (ifilename);
  g_free (base);
  g_free (text);

  seq = g_new (Seq, 1);
  seq->pid = rv;
  seq->fname = ofilename;
  seq->sequence_index = 0;
  seq->frame = gtk_frame_new (gtk_entry_get_text (GTK_ENTRY (label_entry)));
  seq->offset = 0;
  seq->units_changing = 0;
  seq->vbox = gtk_vbox_new (FALSE, 0);
   seq->box = gtk_hbox_new (FALSE, 0);
    label = gtk_label_new ("Volume: ");
    gtk_box_pack_start (GTK_BOX (seq->box), label, FALSE, FALSE, 0);
    seq->volume_adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 2.0, 0.2, 0.1, 0.1));
    seq->volume = gtk_hscrollbar_new (seq->volume_adj);
    gtk_box_pack_start_defaults (GTK_BOX (seq->box), seq->volume);
    seq->offset_entry = gtk_entry_new ();
    gtk_entry_set_width_chars (GTK_ENTRY (seq->offset_entry), 8);
    gtk_entry_set_alignment (GTK_ENTRY (seq->offset_entry), 1.0);
    gtk_entry_set_text (GTK_ENTRY (seq->offset_entry), "0.0");
    seq->offset_units = gtk_combo_box_new_text ();
    gtk_combo_box_append_text (GTK_COMBO_BOX (seq->offset_units), "beats");
    gtk_combo_box_append_text (GTK_COMBO_BOX (seq->offset_units), "seconds");
    gtk_combo_box_append_text (GTK_COMBO_BOX (seq->offset_units), "samples");
    gtk_combo_box_set_active (GTK_COMBO_BOX (seq->offset_units), 0);
    g_signal_connect (seq->offset_units, "changed", G_CALLBACK (handle_offset_units_changed), seq);
    g_signal_connect (seq->offset_entry, "changed", G_CALLBACK (handle_offset_entry_changed), seq);
    gtk_box_pack_start (GTK_BOX (seq->box), seq->offset_entry, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (seq->box), seq->offset_units, FALSE, FALSE, 0);
    kill_button = gtk_button_new_with_label ("X");
    g_signal_connect (kill_button, "clicked", G_CALLBACK (kill_seq), seq);
    gtk_box_pack_start (GTK_BOX (seq->box), kill_button, FALSE,FALSE, 0);
    gtk_widget_set_sensitive (seq->box, FALSE);
    gtk_box_pack_start_defaults (GTK_BOX (seq->vbox), seq->box);
  gtk_container_add (GTK_CONTAINER (seq->frame), seq->vbox);
  gtk_widget_show_all (seq->frame);
  gtk_box_pack_start (GTK_BOX (sequence_vbox), seq->frame, FALSE, FALSE, 0);
  GSK_LIST_APPEND (GET_SEQ_LIST (), seq);

  g_child_watch_add (rv, handle_child_done, seq);
}

static void
handle_interpreter_changed (GtkComboBox *combo_box)
{
  default_interpreter = bb_perform_interpreters
                      + gtk_combo_box_get_active (combo_box);
  if (selected_script != NULL)
    {
      selected_script->interpreter = default_interpreter;
      notify_something_changed ();
    }
}

static void
handle_label_entry_changed (GtkEntry *entry)
{
  const char *text = gtk_entry_get_text (entry);
  if (selected_script == NULL)
    g_warning ("no selected edit-buffer");
  else if (strcmp (gtk_label_get_text (GTK_LABEL (selected_script->label)), text) != 0)
    {
      notify_something_changed ();
      gtk_label_set_text (GTK_LABEL (selected_script->label), text);
    }
}

static void
handle_edit_button_clicked (GtkButton *button,
                            Script *scr)
{
  char *dup;
  if (selected_script == scr)
    return;
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (text_view), scr->text_buffer);
  selected_script = scr;
  g_assert (GTK_IS_LABEL (scr->label));
  dup = g_strdup (gtk_label_get_text (GTK_LABEL (scr->label)));
  gtk_entry_set_text (GTK_ENTRY (label_entry), dup);
  g_free (dup);

  gtk_combo_box_set_active (GTK_COMBO_BOX (interp_combo_box),
                            scr->interpreter - bb_perform_interpreters);

  notify_something_changed ();
}

static void
create_edit_buffer (const char *label,
                    const char *text,
                    const char *interp,
                    gboolean    selected)
{
  Script *scr = g_new (Script, 1);
  GSList *group;
  GList *list;
  group = scripts->len ? gtk_radio_button_get_group (GTK_RADIO_BUTTON (((Script*)scripts->pdata[0])->button)) : NULL;
  scr->button = gtk_radio_button_new_with_label (group, label);
  list = gtk_container_get_children (GTK_CONTAINER (scr->button));
  g_assert (g_list_length (list) == 1);
  scr->label = list->data;
  scr->text_buffer = gtk_text_buffer_new (NULL);
  scr->interpreter = bb_perform_interpreter_find_by_name (interp);
  if (scr->interpreter == NULL)
    g_error ("no interpreter %s found", interp);
  g_signal_connect (scr->text_buffer, "changed", G_CALLBACK (notify_something_changed), NULL);
  gtk_text_buffer_set_text (scr->text_buffer, text, strlen (text));

  gtk_widget_show_all (scr->button);
  gtk_box_pack_start (GTK_BOX (buf_sel_box), scr->button, FALSE, TRUE, 4);
  gtk_widget_queue_resize (buf_scrolled);
  g_signal_connect (scr->button, "clicked", G_CALLBACK (handle_edit_button_clicked), scr);
  g_ptr_array_add (scripts, scr);

  /* select this buffer */
  if (selected)
    gtk_button_clicked (GTK_BUTTON (scr->button));
}

static gboolean
update_timer (gpointer data)
{
  Seq *seq;
  guint cur, total;
  mix_get_time (&cur, &total);
  gtk_adjustment_set_value (time_adjustment, (gdouble)cur/rate);

  for (seq = first_seq; seq; seq = seq->next)
    if (seq->pid == 0)
      {
        mix_sequence_get_time (seq->sequence_index, &cur, NULL, NULL);
        g_object_set (G_OBJECT (seq->sample_widget), "time", cur, NULL);
      }

  return TRUE;
}

static void
handle__new_script (GtkAction *action)
{
  create_edit_buffer ("unnamed", "", default_interpreter->name, TRUE);
}

static void
handle__copy_script (GtkAction *action)
{
  if (selected_script == NULL)
    g_warning ("no selected script to copy");
  else
    {
      char *text = edit_buffer_get_text (selected_script);
      char *title = g_strdup_printf ("%s (copy)", gtk_label_get_text (GTK_LABEL (selected_script->label)));
      create_edit_buffer (title, text, selected_script->interpreter->name, TRUE);
      g_free (text);
      g_free (title);
    }
}

static void
handle_import_script_file_chooser_response (GtkFileChooserDialog *dialog,
                                            gint                  response)
{
  if (response == GTK_RESPONSE_ACCEPT)
    {
      char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      if (filename != NULL)
        {
          const char *basename = strrchr (filename, '/');
          GError *error = NULL;
          char *contents;
          if (basename == NULL)
            basename = filename;
          if (!g_file_get_contents (filename, &contents, NULL, &error))
            {
              g_warning ("error loading %s: %s", filename, error->message);
              g_clear_error (&error);
            }
          else
            {
              create_edit_buffer (basename, contents,
                                  default_interpreter->name, TRUE);
            }
        }
    }
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
handle__import_script (GtkAction *action)
{
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new ("Import Script",
                                        GTK_WINDOW (script_window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                        NULL);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (handle_import_script_file_chooser_response),
                    NULL);
  gtk_widget_show (dialog);
}

static void
handle_export_script_file_chooser_response (GtkFileChooserDialog *dialog,
                                            gint                  response)
{
  if (response == GTK_RESPONSE_ACCEPT
   && selected_script != NULL)
    {
      char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      if (filename != NULL)
        {
          char *text = edit_buffer_get_text (selected_script);
          FILE *fp = fopen (filename, "wb");
          if (fp == NULL)
            {
              g_warning ("error creating %s: %s", filename, g_strerror (errno));
              return;
            }
          if (fwrite (text, strlen (text), 1, fp) != 1)
            {
              g_warning ("error writing %s", filename);
              fclose (fp);
              unlink (filename);
              g_free (text);
              return;
            }
          g_free (text);
          fclose (fp);
        }
    }
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
handle__export_script (GtkAction *action)
{
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new ("Export Script",
                                        GTK_WINDOW (script_window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                        NULL);
#if GTK_CHECK_VERSION(2,8,0)
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
#endif
  g_signal_connect (dialog, "response",
                    G_CALLBACK (handle_export_script_file_chooser_response),
                    NULL);
  gtk_widget_show (dialog);
}

static void
handle__delete_script (GtkAction *action)
{
  /* find index of script */
  guint i;
  Script *scr;
  if (selected_script == NULL)
    {
      g_warning ("no script is selected: cannot delete");
      return;
    }
  for (i = 0; i < scripts->len; i++)
    if (scripts->pdata[i] == selected_script)
      break;
  if (i == scripts->len)
    {
      g_warning ("uh, selected buffer not found");
      return;
    }
  g_ptr_array_remove_index (scripts, i);
  scr = selected_script;

  if (scripts->len > 0)
    {
      guint new_i = (i < scripts->len) ? i : (i - 1);
      Script *sel_eb = scripts->pdata[new_i];
      gtk_button_clicked (GTK_BUTTON (sel_eb->button));
    }
  else
    {
      gtk_text_buffer_set_text (scr->text_buffer, "", 0);
    }

  g_object_unref (scr->text_buffer);
  gtk_widget_destroy (scr->button);
  g_free (scr);
}

static void
handle__save_script (GtkAction *action)
{
  char *text;
  if (selected_script == NULL)
    {
      g_warning ("no edit-buffer selected");
      return;
    }
  text = edit_buffer_get_text (selected_script);
  save_script (gtk_label_get_text (GTK_LABEL (selected_script->label)),
               "",              /* TODO? */
               text,
               selected_script->interpreter);
  g_free (text);
  notify_something_changed ();
}

/* window menu */
static void
handle__sequences_window (GtkAction *action)
{
  gdk_window_raise (seq_window->window);
}
static void
handle__scripts_window (GtkAction *action)
{
  gdk_window_raise (script_window->window);
}
static void
handle__saved_scripts_window (GtkAction *action)
{
  save_script_window_raise_or_create ();
}

static void
handle__saved_delete (GtkAction *action)
{
  BbPerformSavedScript ss;
  GtkTreePath *path;
  GtkTreeIter iter;
  g_return_if_fail ((guint) savedpopup_index < saved_scripts->len);
  ss = g_array_index (saved_scripts, BbPerformSavedScript, savedpopup_index);
  g_free (ss.name);
  g_free (ss.description);
  g_free (ss.text);
  g_array_remove_index (saved_scripts, savedpopup_index);
  path = gtk_tree_path_new_from_indices (savedpopup_index, -1);
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store), &iter, path))
    {
      g_warning ("gtk_tree_model_get_iter failed?");
      return;
    }
  gtk_tree_path_free (path);
  gtk_list_store_remove (list_store, &iter);
  notify_something_changed ();
}

static void
handle__saved_use (GtkAction *action)
{
  BbPerformSavedScript ss;
  g_return_if_fail ((guint) savedpopup_index < saved_scripts->len);
  ss = g_array_index (saved_scripts, BbPerformSavedScript, savedpopup_index);
  create_edit_buffer (ss.name, ss.text, ss.interpreter->name, TRUE);
}

static void
handle__saved_export (GtkAction *action)
{
  g_warning ("unimplemented");
}

static void
handle__saved_edit (GtkAction *action)
{
  g_warning ("unimplemented");
}

static GtkActionEntry action_entries [] =
{
  {
    "FileMenu",
    NULL,
    "File",
    NULL, /* shortcut key */
    "File Menu",
    G_CALLBACK (NULL)
  },

  {
    "NewScript",
    NULL,
    "New Script",
    NULL, /* shortcut key */
    "create a new empty script",
    G_CALLBACK (handle__new_script)
  },

  {
    "CopyScript",
    NULL,
    "Copy Script",
    NULL, /* shortcut key */
    "copy existing script to new script",
    G_CALLBACK (handle__copy_script)
  },

  {
    "ImportScript",
    NULL,
    "Import Script",
    NULL, /* shortcut key */
    "import buffer from file",
    G_CALLBACK (handle__import_script)
  },

  {
    "ExportScript",
    NULL,
    "Export Script",
    NULL, /* shortcut key */
    "export current buffer to file",
    G_CALLBACK (handle__export_script)
  },

  {
    "DeleteScript",
    NULL,
    "Delete Script",
    NULL, /* shortcut key */
    "delete current buffer",
    G_CALLBACK (handle__delete_script)
  },

  {
    "SaveScript",
    NULL,
    "Save Script for Later",
    NULL, /* shortcut key */
    "save script into saved-script table",
    G_CALLBACK (handle__save_script)
  },

  {
    "WindowMenu",
    NULL,
    "Window",
    NULL, /* shortcut key */
    "Window Menu",
    G_CALLBACK (NULL)
  },
  {
    "SeqWindow",
    NULL,
    "Sequences",
    NULL, /* shortcut key */
    "window of all the sequences",
    G_CALLBACK (handle__sequences_window)
  },
  {
    "ScriptWindow",
    NULL,
    "Scripts",
    NULL, /* shortcut key */
    "window of all the scripts",
    G_CALLBACK (handle__scripts_window)
  },
  {
    "SavedScriptWindow",
    NULL,
    "Saved Scripts",
    NULL, /* shortcut key */
    "window of the scripts saved 'for later'",
    G_CALLBACK (handle__saved_scripts_window)
  },

  /* Saved Script Popup */
  {
    "SavedDelete",
    NULL,
    "Delete",
    NULL, /* shortcut key */
    "delete the saved script",
    G_CALLBACK (handle__saved_delete)
  },
  {
    "SavedUse",
    NULL,
    "Use",
    NULL, /* shortcut key */
    "use the saved-script in the main script-editor",
    G_CALLBACK (handle__saved_use)
  },
  {
    "SavedExport",
    NULL,
    "Export",
    NULL, /* shortcut key */
    "export the saved-script to a file",
    G_CALLBACK (handle__saved_export)
  },
  {
    "SavedEdit",
    NULL,
    "Edit",
    NULL, /* shortcut key */
    "edit the saved script's info",
    G_CALLBACK (handle__saved_edit)
  },
};


static const char *menu_hier =
"<ui>"
  "<menubar name=\"main\">"
    "<menu action=\"FileMenu\">"
      "<menuitem action=\"NewScript\" />"
      "<menuitem action=\"CopyScript\" />"
      "<menuitem action=\"ImportScript\" />"
      "<menuitem action=\"ExportScript\" />"
      "<menuitem action=\"DeleteScript\" />"
      "<menuitem action=\"SaveScript\" />"
    "</menu>"
    "<menu action=\"WindowMenu\">"
      "<menuitem action=\"SeqWindow\" />"
      "<menuitem action=\"ScriptWindow\" />"
      "<menuitem action=\"SavedScriptWindow\" />"
    "</menu>"
  "</menubar>"
  "<popup name=\"savedpopup\">"
    "<menuitem action=\"SavedDelete\" />"
    "<menuitem action=\"SavedUse\" />"
    "<menuitem action=\"SavedExport\" />"
    "<menuitem action=\"SavedEdit\" />"
  "</popup>"
"</ui>"
;

static void
write_config (void)
{
  guint i;
  BbXml *xml = bb_xml_new_node ("BB", NULL);
  GError *error = NULL;
  for (i = 0; i < scripts->len; i++)
    {
      Script *scr = scripts->pdata[i];
      BbXml *buf_node = bb_xml_new_node ("Buffer", NULL);
      const char *label = gtk_label_get_text (GTK_LABEL (scr->label));
      char *contents;
      contents = edit_buffer_get_text (scr);
      bb_xml_add_text_child (buf_node, "label", label);
      bb_xml_add_text_child (buf_node, "contents", contents);
      bb_xml_add_text_child (buf_node, "interpreter", scr->interpreter->name);
      if (scr == selected_script)
        bb_xml_add_child (buf_node, bb_xml_new_node ("selected", NULL));
      g_free (contents);
      bb_xml_add_child (xml, buf_node);
    }
  saved_scripts_append_xml (xml);
  if (!bb_xml_save (xml, status_fname, &error))
    g_warning ("Error saving status file: %s", error->message);
  bb_xml_free (xml);
}

static void
handle_Buffer (BbXml *xml,
               gpointer data)
{
  /* should have "label" and "contents" tags */
  BbXml *label_node, *contents_node, *interpreter_node, *selected_node;
  char *label, *contents, *interp;
  label_node = bb_xml_find_child (xml, "label");
  contents_node = bb_xml_find_child (xml, "contents");
  selected_node = bb_xml_find_child (xml, "selected");
  interpreter_node = bb_xml_find_child (xml, "interpreter");
  if (label_node == NULL)
    g_error ("missing <label> under <Buffer>");
  if (contents_node == NULL)
    g_error ("missing <contents> under <Buffer>");
  if (interpreter_node == NULL)
    interp = g_strdup ("BB");
  else
    interp = bb_xml_get_all_text (interpreter_node);

  label = bb_xml_get_all_text (label_node);
  contents = bb_xml_get_all_text (contents_node);
  create_edit_buffer (label, contents, interp, selected_node != NULL);
  g_free (label);
  g_free (contents);
}

static void
handle_Saved (BbXml *xml,
              gpointer data)
{
  saved_scripts_load_xml (xml);
}

int main(int argc, char **argv)
{
  guint i;
  guint total;
  GtkWidget *menubar;
  gdouble bpm = 90;
  gdouble n_beats = 16;
  GError *error = NULL;
  g_thread_init (NULL);
  gtk_init (&argc, &argv);
  scripts = g_ptr_array_new ();

  default_interpreter = bb_perform_interpreter_find_by_name ("BB");
  g_assert (default_interpreter);

  for (i = 1; i < (guint) argc; i++)
    {
      if (g_str_has_prefix (argv[i], "--tmp-dir="))
        tmp_dir = strchr (argv[i], '=') + 1;
      else if (g_str_has_prefix (argv[i], "--dir="))
        dir = strchr (argv[i], '=') + 1;
    }
  if (dir == NULL)
    g_error ("missing --dir=DIR parameter");

  beat_period = 60.0 / bpm;
  mix_init (&argc, (const char ***) &argv, n_beats * beat_period);
  rate = mix_get_sampling_rate ();

  manager = gtk_ui_manager_new ();
  seq_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  script_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  action_group = gtk_action_group_new ("BbPerform");
  gtk_action_group_add_actions (action_group,
                                action_entries, G_N_ELEMENTS (action_entries),
                                script_window);
  gtk_ui_manager_insert_action_group (manager, action_group, 0);
  if (gtk_ui_manager_add_ui_from_string (manager, menu_hier, -1, &error)==0)
    g_error ("error parsing menu-hierarchy: %s", error->message);


    time_seq_vbox = gtk_vbox_new (FALSE, 1);
    mix_get_time (NULL, &total);
    time_adjustment = GTK_ADJUSTMENT (gtk_adjustment_new(0.0,0.0,(gdouble)total/rate,0.01,0.01,0.01));
    time_scrollbar = gtk_hscale_new (time_adjustment);
    gtk_box_pack_start (GTK_BOX (time_seq_vbox), time_scrollbar, FALSE, TRUE, 0);
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start_defaults (GTK_BOX (time_seq_vbox), scrolled_window);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    sequence_vbox = gtk_vbox_new (TRUE, 1);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), sequence_vbox);
    gtk_container_add (GTK_CONTAINER (seq_window), time_seq_vbox);

    vbox = gtk_vbox_new (FALSE, 1);
     menubar = gtk_ui_manager_get_widget (manager, "/main");
     gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
     hbox = gtk_hbox_new (FALSE, 1);
     buf_scrolled = gtk_scrolled_window_new (NULL, NULL);
     gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (buf_scrolled),
                                     GTK_POLICY_ALWAYS, GTK_POLICY_NEVER);
      buf_sel_box = gtk_hbox_new (FALSE, 0);
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (buf_scrolled), buf_sel_box);
     gtk_box_pack_start_defaults (GTK_BOX (hbox), buf_scrolled);
     gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 1);

     label_hbox = gtk_hbox_new (FALSE, 1);
      label_label = gtk_label_new ("Label: ");
      label_entry = gtk_entry_new ();
      g_signal_connect (label_entry, "changed", G_CALLBACK (handle_label_entry_changed), NULL);
      gtk_box_pack_start (GTK_BOX (label_hbox), label_label, FALSE, FALSE, 0);
      gtk_box_pack_start_defaults (GTK_BOX (label_hbox), label_entry);
      interp_combo_box = gtk_combo_box_new_text ();
      g_signal_connect (interp_combo_box, "changed",
                        G_CALLBACK (handle_interpreter_changed), NULL);

      for (i = 0; i < bb_perform_interpreters_count; i++)
        gtk_combo_box_append_text (GTK_COMBO_BOX (interp_combo_box),
                                   bb_perform_interpreters[i].name);
      gtk_box_pack_start (GTK_BOX (label_hbox), interp_combo_box, FALSE, FALSE, 0);
      go_button = gtk_button_new_with_label ("Go!");
      g_signal_connect (go_button, "clicked", G_CALLBACK (handle_go), NULL);
      gtk_box_pack_start (GTK_BOX (label_hbox), go_button, FALSE, FALSE, 0);
     gtk_box_pack_start (GTK_BOX (vbox), label_hbox, FALSE, TRUE, 0);

     text_view = gtk_text_view_new ();
     gtk_box_pack_start_defaults (GTK_BOX (vbox), text_view);
    gtk_container_add (GTK_CONTAINER (script_window), vbox);

 gtk_widget_set_size_request (script_window, 400,500);
 gtk_widget_set_size_request (seq_window, 400,700);
 gtk_widget_show_all (script_window);
 gtk_widget_show_all (seq_window);

 status_fname = g_strdup_printf ("%s/status.xml", dir);
 if (g_file_test (dir, G_FILE_TEST_IS_DIR))
   {
     BbXml *status = bb_xml_parse_file (status_fname, &error);
     if (status == NULL)
       g_error ("error parsing status from %s: %s", status_fname, error->message);

     bb_xml_iterate (status, "BB/Buffer", handle_Buffer, NULL);
     bb_xml_iterate (status, "BB/Saved", handle_Saved, NULL);

     bb_xml_free (status);
   }
 else
   {
     BbXml *empty_status = bb_xml_new_node ("BB", NULL);
     if (mkdir (dir, 0755) < 0)
       {
         g_error ("error creating %s: %s", dir, g_strerror (errno));
       }
     if (!bb_xml_save (empty_status, status_fname, &error))
       g_error ("error writing empty status file %s: %s", status_fname, error->message);
     bb_xml_free (empty_status);
   }

 g_timeout_add (50, update_timer, NULL);

 gtk_main ();
 return 0;
}
