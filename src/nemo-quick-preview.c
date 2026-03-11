/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-quick-preview.c — Lightweight in-process file viewer
 *
 * Inspired by Double Commander's F3 viewer.  Opens a transient,
 * read-only window showing the selected file's content:
 *
 *   • Plain text / source code  → GtkTextView (monospace, first 2 MB)
 *   • Images (PNG/JPEG/GIF/…)   → GdkPixbuf → GtkImage, scaled to fit
 *   • Audio / Video             → GStreamer playbin (when available)
 *   • Anything else             → hex dump (first 64 KB)
 *
 * Press Escape to dismiss; focus returns to the Nemo window.
 *
 * Copyright (C) 2026 smplOS contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <config.h>
#include "nemo-quick-preview.h"
#include "nemo-paged-viewer.h"
#include "nemo-image-viewer.h"
#include "nemo-preview-utils.h"
#include "nemo-dir-analyzer.h"

#include <glib/gi18n.h>
#include <string.h>

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsink.h>
#endif

/* ------------------------------------------------------------------ */
/* Tunables                                                           */
/* ------------------------------------------------------------------ */

#define DEFAULT_WIDTH      800
#define DEFAULT_HEIGHT     600

/* ------------------------------------------------------------------ */
/* GObject boilerplate                                                */
/* ------------------------------------------------------------------ */

typedef enum {
	PREVIEW_NONE,
	PREVIEW_TEXT,
	PREVIEW_IMAGE,
	PREVIEW_MEDIA,
	PREVIEW_HEX,
	PREVIEW_DIR,
} PreviewMode;

struct _NemoQuickPreview {
	GtkWindow parent_instance;

	/* Current mode */
	PreviewMode  mode;

	/* Layout */
	GtkWidget   *header_bar;
	GtkWidget   *stack;

	/* Paged viewer (text + hex, lazy-loaded) */
	NemoPagedViewer *paged_viewer;

	/* Image page (shared widget) */
	NemoImageViewer *image_viewer;

	/* Media page */
	GtkWidget   *media_box;
#ifdef HAVE_GSTREAMER
	GtkWidget   *video_area;
	GstElement  *pipeline;
	guint        bus_watch_id;
	GtkWidget   *play_btn;
	GtkWidget   *mute_btn;
	GtkWidget   *seek_scale;
	guint        seek_update_id;
	gboolean     seek_lock;
	gboolean     video_muted;

	/* Frame rendering (appsink → cairo) */
	cairo_surface_t *frame_surface;
	GMutex       frame_mutex;
	gint         video_width;
	gint         video_height;
#endif

	/* Directory analyzer (F3 on folders) */
	NemoDirAnalyzer *dir_analyzer;

	/* Parent tracking */
	GtkWindow   *parent_window;

	/* Directory navigation */
	GFile       *current_file;
	GFile       *current_dir;
	GPtrArray   *dir_files;     /* sorted GFile* in parent directory */
	gint         current_index;

	/* Navigation widgets */
	GtkWidget   *prev_button;
	GtkWidget   *next_button;
	GtkWidget   *counter_label;
};

G_DEFINE_TYPE (NemoQuickPreview, nemo_quick_preview, GTK_TYPE_WINDOW)

/* Singleton */
static NemoQuickPreview *_instance = NULL;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void     preview_clear          (NemoQuickPreview *self);
static void     preview_show_paged     (NemoQuickPreview *self, GFile *file, NemoViewerMode mode);
static void     preview_show_image     (NemoQuickPreview *self, GFile *file);

#ifdef HAVE_GSTREAMER
static void     preview_show_media     (NemoQuickPreview *self, GFile *file);
static void     media_stop             (NemoQuickPreview *self);
static gboolean video_area_draw_cb     (GtkWidget *widget, cairo_t *cr, gpointer data);
static GstFlowReturn new_sample_cb     (GstAppSink *sink, gpointer data);
static void     play_pause_clicked_cb  (GtkButton *btn, gpointer data);
static void     mute_clicked_cb        (GtkButton *btn, gpointer data);
static gboolean seek_press_cb          (GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean seek_release_cb        (GtkWidget *widget, GdkEventButton *event, gpointer data);
#endif

static void     navigation_clear       (NemoQuickPreview *self);
static void     populate_dir_file_list (NemoQuickPreview *self, GFile *file);
static void     load_file_content      (NemoQuickPreview *self, GFile *file);
static void     navigate_to_offset     (NemoQuickPreview *self, gint offset);
static void     update_navigation_ui   (NemoQuickPreview *self);

/* ------------------------------------------------------------------ */
/* Key press — Esc dismisses                                          */
/* ------------------------------------------------------------------ */

static gboolean
on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (widget);

	switch (event->keyval) {
	case GDK_KEY_Escape:
		nemo_quick_preview_dismiss (self);
		return GDK_EVENT_STOP;
	case GDK_KEY_Left:
		navigate_to_offset (self, -1);
		return GDK_EVENT_STOP;
	case GDK_KEY_Right:
		navigate_to_offset (self, 1);
		return GDK_EVENT_STOP;
	case GDK_KEY_space:
		if (self->mode == PREVIEW_MEDIA && self->play_btn != NULL) {
			g_signal_emit_by_name (self->play_btn, "clicked");
		}
		return GDK_EVENT_STOP;
	case GDK_KEY_m:
	case GDK_KEY_M:
		if (self->mode == PREVIEW_MEDIA && self->mute_btn != NULL) {
			g_signal_emit_by_name (self->mute_btn, "clicked");
		}
		return GDK_EVENT_STOP;
	case GDK_KEY_f:
	case GDK_KEY_F:
		if (gdk_window_get_state (gtk_widget_get_window (widget)) & GDK_WINDOW_STATE_FULLSCREEN)
			gtk_window_unfullscreen (GTK_WINDOW (self));
		else
			gtk_window_fullscreen (GTK_WINDOW (self));
		return GDK_EVENT_STOP;
	default:
		break;
	}

	return GDK_EVENT_PROPAGATE;
}

/* ------------------------------------------------------------------ */
/* Delete event — hide instead of destroy (singleton reuse)           */
/* ------------------------------------------------------------------ */

static gboolean
on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	nemo_quick_preview_dismiss (NEMO_QUICK_PREVIEW (widget));
	return GDK_EVENT_STOP;   /* prevent destruction */
}

/* ------------------------------------------------------------------ */
/* Directory scan finished callback                                   */
/* ------------------------------------------------------------------ */

static void
on_dir_scan_finished (NemoDirAnalyzer *analyzer,
                      guint64          total_size,
                      gpointer         data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);
	char *sz, *subtitle;

	(void) analyzer;

	if (self->mode != PREVIEW_DIR)
		return;

	if (total_size > 0) {
		sz = g_format_size (total_size);
		subtitle = g_strdup_printf (_("Directory — %s"), sz);
		g_free (sz);
	} else {
		subtitle = g_strdup (_("Directory — empty"));
	}

	gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->header_bar),
	                             subtitle);
	g_free (subtitle);
}

/* ------------------------------------------------------------------ */
/* Navigation button callbacks                                        */
/* ------------------------------------------------------------------ */

static void
on_prev_clicked (GtkButton *button, gpointer data)
{
	navigate_to_offset (NEMO_QUICK_PREVIEW (data), -1);
}

static void
on_next_clicked (GtkButton *button, gpointer data)
{
	navigate_to_offset (NEMO_QUICK_PREVIEW (data), 1);
}

/* ------------------------------------------------------------------ */
/* Construction                                                       */
/* ------------------------------------------------------------------ */

static void
nemo_quick_preview_init (NemoQuickPreview *self)
{
	self->mode = PREVIEW_NONE;
	self->parent_window = NULL;
	self->current_file = NULL;
	self->current_dir = NULL;
	self->dir_files = NULL;
	self->current_index = -1;
#ifdef HAVE_GSTREAMER
	self->pipeline = NULL;
	self->frame_surface = NULL;
	g_mutex_init (&self->frame_mutex);
	self->video_width = 0;
	self->video_height = 0;
	self->bus_watch_id = 0;
	self->play_btn = NULL;
	self->mute_btn = NULL;
	self->seek_scale = NULL;
	self->seek_update_id = 0;
	self->seek_lock = FALSE;
	self->video_muted = TRUE;

	/* Initialise GStreamer early (safe to call multiple times) */
	if (!gst_init_check (NULL, NULL, NULL)) {
		g_warning ("Quick preview: GStreamer initialisation failed");
	}
#endif

	/* Window setup */
	gtk_window_set_default_size (GTK_WINDOW (self), DEFAULT_WIDTH, DEFAULT_HEIGHT);
	gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_type_hint (GTK_WINDOW (self), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (self), TRUE);

	/* CSS class for theme styling */
	{
		GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self));
		gtk_style_context_add_class (context, "nemo-quick-preview");
	}

	/* Header bar */
	self->header_bar = gtk_header_bar_new ();
	gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->header_bar), TRUE);
	gtk_header_bar_set_title (GTK_HEADER_BAR (self->header_bar), _("Quick Preview"));
	gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->header_bar), "");
	gtk_window_set_titlebar (GTK_WINDOW (self), self->header_bar);

	/* Navigation buttons */
	self->prev_button = gtk_button_new_from_icon_name ("go-previous-symbolic",
	                                                    GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text (self->prev_button, _("Previous file"));
	gtk_widget_set_sensitive (self->prev_button, FALSE);
	g_signal_connect (self->prev_button, "clicked",
	                  G_CALLBACK (on_prev_clicked), self);
	gtk_header_bar_pack_start (GTK_HEADER_BAR (self->header_bar), self->prev_button);

	self->counter_label = gtk_label_new ("");
	gtk_header_bar_pack_start (GTK_HEADER_BAR (self->header_bar), self->counter_label);

	self->next_button = gtk_button_new_from_icon_name ("go-next-symbolic",
	                                                    GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text (self->next_button, _("Next file"));
	gtk_widget_set_sensitive (self->next_button, FALSE);
	g_signal_connect (self->next_button, "clicked",
	                  G_CALLBACK (on_next_clicked), self);
	gtk_header_bar_pack_start (GTK_HEADER_BAR (self->header_bar), self->next_button);

	/* Fullscreen toggle button on the right side */
	{
		GtkWidget *fs_button = gtk_button_new_from_icon_name (
			"view-fullscreen-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_widget_set_tooltip_text (fs_button, _("Toggle fullscreen (F)"));
		g_signal_connect_swapped (fs_button, "clicked",
			G_CALLBACK (gtk_window_fullscreen), self);
		gtk_header_bar_pack_end (GTK_HEADER_BAR (self->header_bar), fs_button);
	}

	/* Stack for switching between views */
	self->stack = gtk_stack_new ();
	gtk_stack_set_transition_type (GTK_STACK (self->stack),
	                               GTK_STACK_TRANSITION_TYPE_CROSSFADE);
	gtk_stack_set_transition_duration (GTK_STACK (self->stack), 100);
	gtk_container_add (GTK_CONTAINER (self), self->stack);

	/* --- Paged viewer (text + hex, handles files of any size) --- */
	self->paged_viewer = nemo_paged_viewer_new ();
	gtk_stack_add_named (GTK_STACK (self->stack),
	                     GTK_WIDGET (self->paged_viewer), "paged");

	/* --- Image page — shared NemoImageViewer widget --- */
	self->image_viewer = nemo_image_viewer_new ();
	nemo_image_viewer_set_fit (self->image_viewer, TRUE);
	nemo_image_viewer_set_show_controls (self->image_viewer, FALSE);
	gtk_stack_add_named (GTK_STACK (self->stack),
	                     GTK_WIDGET (self->image_viewer), "image");

	/* --- Media page --- */
	self->media_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
#ifdef HAVE_GSTREAMER
	self->video_area = gtk_drawing_area_new ();
	gtk_widget_set_app_paintable (self->video_area, TRUE);
	gtk_widget_set_hexpand (self->video_area, TRUE);
	gtk_widget_set_vexpand (self->video_area, TRUE);
	g_signal_connect (self->video_area, "draw",
	                  G_CALLBACK (video_area_draw_cb), self);
	gtk_box_pack_start (GTK_BOX (self->media_box), self->video_area, TRUE, TRUE, 0);
	gtk_widget_show (self->video_area);

	/* Seek bar */
	self->seek_scale = gtk_scale_new_with_range (
		GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 1.0);
	gtk_scale_set_draw_value (GTK_SCALE (self->seek_scale), FALSE);
	gtk_widget_set_margin_start (self->seek_scale, 4);
	gtk_widget_set_margin_end (self->seek_scale, 4);
	g_signal_connect (self->seek_scale, "button-press-event",
	                  G_CALLBACK (seek_press_cb), self);
	g_signal_connect (self->seek_scale, "button-release-event",
	                  G_CALLBACK (seek_release_cb), self);
	gtk_box_pack_start (GTK_BOX (self->media_box), self->seek_scale, FALSE, FALSE, 0);
	gtk_widget_show (self->seek_scale);

	/* Transport controls (play/pause + mute) */
	{
		GtkWidget *vctrl = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
		gtk_widget_set_margin_start (vctrl, 4);
		gtk_widget_set_margin_end (vctrl, 4);
		gtk_widget_set_margin_bottom (vctrl, 4);
		gtk_widget_set_halign (vctrl, GTK_ALIGN_CENTER);

		self->play_btn = gtk_button_new_from_icon_name (
			"media-playback-start-symbolic",
			GTK_ICON_SIZE_SMALL_TOOLBAR);
		gtk_button_set_relief (GTK_BUTTON (self->play_btn), GTK_RELIEF_NONE);
		gtk_widget_set_tooltip_text (self->play_btn, _("Play / Pause"));
		g_signal_connect (self->play_btn, "clicked",
		                  G_CALLBACK (play_pause_clicked_cb), self);
		gtk_box_pack_start (GTK_BOX (vctrl), self->play_btn, FALSE, FALSE, 0);
		gtk_widget_show (self->play_btn);

		self->mute_btn = gtk_button_new_from_icon_name (
			"audio-volume-muted-symbolic",
			GTK_ICON_SIZE_SMALL_TOOLBAR);
		gtk_button_set_relief (GTK_BUTTON (self->mute_btn), GTK_RELIEF_NONE);
		gtk_widget_set_tooltip_text (self->mute_btn, _("Mute / Unmute"));
		g_signal_connect (self->mute_btn, "clicked",
		                  G_CALLBACK (mute_clicked_cb), self);
		gtk_box_pack_start (GTK_BOX (vctrl), self->mute_btn, FALSE, FALSE, 0);
		gtk_widget_show (self->mute_btn);

		gtk_box_pack_end (GTK_BOX (self->media_box), vctrl, FALSE, FALSE, 0);
		gtk_widget_show (vctrl);
	}
#else
	{
		GtkWidget *label = gtk_label_new (_("Media preview requires GStreamer"));
		gtk_box_pack_start (GTK_BOX (self->media_box), label, TRUE, TRUE, 0);
	}
#endif
	gtk_stack_add_named (GTK_STACK (self->stack), self->media_box, "media");

	/* --- Directory analyzer page --- */
	self->dir_analyzer = nemo_dir_analyzer_new ();
	gtk_stack_add_named (GTK_STACK (self->stack),
	                     GTK_WIDGET (self->dir_analyzer), "directory");
	g_signal_connect (self->dir_analyzer, "scan-finished",
	                  G_CALLBACK (on_dir_scan_finished), self);

	/* Signals */
	g_signal_connect (self, "key-press-event", G_CALLBACK (on_key_press), NULL);
	g_signal_connect (self, "delete-event", G_CALLBACK (on_delete_event), NULL);

	gtk_widget_show_all (self->stack);
}

static void
nemo_quick_preview_finalize (GObject *object)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (object);

	/* Child widgets are already destroyed by GTK dispose,
	 * only clean up GStreamer and navigation state */
#ifdef HAVE_GSTREAMER
	media_stop (self);
#endif
	navigation_clear (self);

	if (self == _instance)
		_instance = NULL;

	G_OBJECT_CLASS (nemo_quick_preview_parent_class)->finalize (object);
}

static void
nemo_quick_preview_class_init (NemoQuickPreviewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = nemo_quick_preview_finalize;
}

/* ------------------------------------------------------------------ */
/* Singleton accessor                                                 */
/* ------------------------------------------------------------------ */

NemoQuickPreview *
nemo_quick_preview_get_instance (void)
{
	if (_instance == NULL) {
		_instance = g_object_new (NEMO_TYPE_QUICK_PREVIEW,
		                          "type", GTK_WINDOW_TOPLEVEL,
		                          NULL);
		/* prevent destruction — we hide/show */
		g_object_ref_sink (_instance);
	}
	return _instance;
}

/* ------------------------------------------------------------------ */
/* Clear / reset current content                                      */
/* ------------------------------------------------------------------ */

static void
preview_clear (NemoQuickPreview *self)
{
#ifdef HAVE_GSTREAMER
	media_stop (self);
#endif

	nemo_image_viewer_clear (self->image_viewer);
	nemo_paged_viewer_close_file (self->paged_viewer);
	nemo_dir_analyzer_cancel (self->dir_analyzer);
	nemo_dir_analyzer_clear (self->dir_analyzer);

	self->mode = PREVIEW_NONE;
}

/* ------------------------------------------------------------------ */
/* Directory navigation helpers                                       */
/* ------------------------------------------------------------------ */

static void
navigation_clear (NemoQuickPreview *self)
{
	g_clear_object (&self->current_file);
	g_clear_object (&self->current_dir);
	g_clear_pointer (&self->dir_files, g_ptr_array_unref);
	self->current_index = -1;
}

static gint
compare_files_by_name (gconstpointer a, gconstpointer b)
{
	GFile *fa = *(GFile **) a;
	GFile *fb = *(GFile **) b;
	char *na, *nb;
	gint result;

	na = g_file_get_basename (fa);
	nb = g_file_get_basename (fb);
	result = g_utf8_collate (na, nb);
	g_free (na);
	g_free (nb);

	return result;
}

static void
populate_dir_file_list (NemoQuickPreview *self, GFile *file)
{
	GFile *parent;
	GFileEnumerator *enumerator;
	GFileInfo *child_info;
	GError *error = NULL;

	navigation_clear (self);

	parent = g_file_get_parent (file);
	if (parent == NULL)
		return;

	self->current_dir = parent;   /* takes ownership */
	self->current_file = g_object_ref (file);
	self->dir_files = g_ptr_array_new_with_free_func (g_object_unref);

	enumerator = g_file_enumerate_children (
		parent,
		G_FILE_ATTRIBUTE_STANDARD_NAME ","
		G_FILE_ATTRIBUTE_STANDARD_TYPE ","
		G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
		G_FILE_QUERY_INFO_NONE,
		NULL, &error);

	if (enumerator == NULL) {
		g_warning ("Quick preview: could not enumerate directory: %s",
		           error->message);
		g_error_free (error);
		return;
	}

	while ((child_info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
		const char *name;
		GFile *child;

		/* Skip hidden files */
		if (g_file_info_get_is_hidden (child_info)) {
			g_object_unref (child_info);
			continue;
		}

		name = g_file_info_get_name (child_info);
		child = g_file_get_child (parent, name);
		g_ptr_array_add (self->dir_files, child);
		g_object_unref (child_info);
	}

	g_object_unref (enumerator);

	/* Sort by display name */
	g_ptr_array_sort (self->dir_files, compare_files_by_name);

	/* Find current file index */
	self->current_index = -1;
	for (guint i = 0; i < self->dir_files->len; i++) {
		if (g_file_equal (g_ptr_array_index (self->dir_files, i), file)) {
			self->current_index = (gint) i;
			break;
		}
	}

	/* If file not found (e.g. it was hidden), insert it */
	if (self->current_index < 0) {
		g_ptr_array_add (self->dir_files, g_object_ref (file));
		g_ptr_array_sort (self->dir_files, compare_files_by_name);
		for (guint i = 0; i < self->dir_files->len; i++) {
			if (g_file_equal (g_ptr_array_index (self->dir_files, i), file)) {
				self->current_index = (gint) i;
				break;
			}
		}
	}
}

static void
update_navigation_ui (NemoQuickPreview *self)
{
	gchar *text;

	if (self->dir_files == NULL || self->dir_files->len == 0) {
		gtk_widget_set_sensitive (self->prev_button, FALSE);
		gtk_widget_set_sensitive (self->next_button, FALSE);
		gtk_label_set_text (GTK_LABEL (self->counter_label), "");
		return;
	}

	gtk_widget_set_sensitive (self->prev_button,
	                          self->current_index > 0);
	gtk_widget_set_sensitive (self->next_button,
	                          self->current_index < (gint) self->dir_files->len - 1);

	text = g_strdup_printf ("%d / %d",
	                         self->current_index + 1,
	                         (gint) self->dir_files->len);
	gtk_label_set_text (GTK_LABEL (self->counter_label), text);
	g_free (text);
}

static void
load_file_content (NemoQuickPreview *self, GFile *file)
{
	GFileInfo *info;
	const gchar *content_type;
	const gchar *display_name;
	gchar *size_str;
	goffset size;
	char *subtitle;

	preview_clear (self);

	/* Query file info */
	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
	                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
	                          G_FILE_ATTRIBUTE_STANDARD_SIZE ","
	                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL, NULL);

	if (info == NULL)
		return;

	content_type = g_file_info_get_content_type (info);
	display_name = g_file_info_get_display_name (info);
	size = g_file_info_get_size (info);

	/* Title = filename */
	gtk_header_bar_set_title (GTK_HEADER_BAR (self->header_bar),
	                          display_name ? display_name : _("Quick Preview"));

	/* ── Directory → dir-analyzer ── */
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		char *path = g_file_get_path (file);

		gtk_header_bar_set_subtitle (
			GTK_HEADER_BAR (self->header_bar),
			_("Directory — scanning…"));

		nemo_dir_analyzer_scan_async (
			self->dir_analyzer, path,
			display_name, 0, 12000);

		self->mode = PREVIEW_DIR;
		gtk_stack_set_visible_child_name (
			GTK_STACK (self->stack), "directory");

		g_free (path);
		g_object_unref (info);
		return;
	}

	/* Subtitle = size + type */
	size_str = g_format_size (size);
	subtitle = g_strdup_printf ("%s — %s", size_str,
	                            content_type ? content_type : _("unknown"));
	gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->header_bar), subtitle);
	g_free (size_str);
	g_free (subtitle);

	/* Decide which view to use */
	if (nemo_preview_mime_is_image (content_type)) {
		preview_show_image (self, file);
	}
#ifdef HAVE_GSTREAMER
	else if (nemo_preview_mime_is_media (content_type)) {
		preview_show_media (self, file);
	}
#endif
	else if (nemo_preview_mime_is_text (content_type)) {
		preview_show_paged (self, file, NEMO_VIEWER_MODE_TEXT);
	} else {
		preview_show_paged (self, file, NEMO_VIEWER_MODE_HEX);
	}

	g_object_unref (info);
}

static void
navigate_to_offset (NemoQuickPreview *self, gint offset)
{
	gint new_index;
	GFile *file;

	if (self->dir_files == NULL || self->dir_files->len == 0)
		return;

	new_index = self->current_index + offset;

	if (new_index < 0 || new_index >= (gint) self->dir_files->len)
		return;

	self->current_index = new_index;
	file = g_ptr_array_index (self->dir_files, new_index);

	/* Update current_file */
	g_clear_object (&self->current_file);
	self->current_file = g_object_ref (file);

	load_file_content (self, file);
	update_navigation_ui (self);
}

/* ------------------------------------------------------------------ */
/* Paged preview (text + hex — lazy-loaded, any file size)            */
/* ------------------------------------------------------------------ */

static void
preview_show_paged (NemoQuickPreview *self, GFile *file, NemoViewerMode mode)
{
	char *path;

	path = g_file_get_path (file);
	if (path == NULL)
		return;

	nemo_paged_viewer_set_mode (self->paged_viewer, mode);

	if (!nemo_paged_viewer_open_file (self->paged_viewer, path, NULL)) {
		g_free (path);
		return;
	}

	g_free (path);

	self->mode = (mode == NEMO_VIEWER_MODE_TEXT) ? PREVIEW_TEXT : PREVIEW_HEX;
	gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "paged");
}

/* ------------------------------------------------------------------ */
/* Image preview                                                      */
/* ------------------------------------------------------------------ */

static void
preview_show_image (NemoQuickPreview *self, GFile *file)
{
	char *path;
	GError *error = NULL;

	path = g_file_get_path (file);
	if (path == NULL) {
		preview_show_paged (self, file, NEMO_VIEWER_MODE_HEX);
		return;
	}

	if (!nemo_image_viewer_load_file (self->image_viewer, path, &error)) {
		g_warning ("Quick preview: could not load image: %s",
		           error->message);
		g_error_free (error);
		g_free (path);
		preview_show_paged (self, file, NEMO_VIEWER_MODE_HEX);
		return;
	}

	g_free (path);

	self->mode = PREVIEW_IMAGE;
	gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "image");
}

/* ------------------------------------------------------------------ */
/* Media preview (GStreamer)                                           */
/* ------------------------------------------------------------------ */

#ifdef HAVE_GSTREAMER

static gboolean
media_bus_callback (GstBus *bus, GstMessage *message, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);

	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_EOS:
		/* Loop playback */
		gst_element_seek_simple (self->pipeline, GST_FORMAT_TIME,
		                         GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
		break;
	case GST_MESSAGE_ERROR: {
		GError *err = NULL;
		gchar *dbg = NULL;
		gst_message_parse_error (message, &err, &dbg);
		g_warning ("Quick preview GStreamer error: %s\n  debug: %s",
		           err->message, dbg ? dbg : "(none)");
		g_error_free (err);
		g_free (dbg);
		media_stop (self);
		break;
	}
	case GST_MESSAGE_STATE_CHANGED:
		break;
	default:
		break;
	}

	return TRUE;
}

/* ---- appsink new-sample callback (called on streaming thread) ---- */
static GstFlowReturn
new_sample_cb (GstAppSink *sink, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);
	GstSample *sample;
	GstBuffer *buffer;
	GstCaps *caps;
	GstVideoInfo vinfo;
	GstMapInfo map;
	cairo_surface_t *surface;
	int w, h, stride;
	const guint8 *src;
	guint8 *dst;
	int i;

	sample = gst_app_sink_pull_sample (sink);
	if (sample == NULL)
		return GST_FLOW_OK;

	buffer = gst_sample_get_buffer (sample);
	caps = gst_sample_get_caps (sample);
	if (buffer == NULL || caps == NULL) {
		gst_sample_unref (sample);
		return GST_FLOW_OK;
	}

	if (!gst_video_info_from_caps (&vinfo, caps)) {
		gst_sample_unref (sample);
		return GST_FLOW_OK;
	}

	w = GST_VIDEO_INFO_WIDTH (&vinfo);
	h = GST_VIDEO_INFO_HEIGHT (&vinfo);

	if (!gst_buffer_map (buffer, &map, GST_MAP_READ)) {
		gst_sample_unref (sample);
		return GST_FLOW_OK;
	}

	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
	stride = cairo_image_surface_get_stride (surface);
	dst = cairo_image_surface_get_data (surface);
	src = map.data;

	/* Copy row-by-row: GStreamer BGRx stride may differ from Cairo's */
	for (i = 0; i < h; i++) {
		memcpy (dst + i * stride,
		        src + i * (int) GST_VIDEO_INFO_PLANE_STRIDE (&vinfo, 0),
		        w * 4);
	}

	cairo_surface_mark_dirty (surface);
	gst_buffer_unmap (buffer, &map);
	gst_sample_unref (sample);

	g_mutex_lock (&self->frame_mutex);
	if (self->frame_surface != NULL)
		cairo_surface_destroy (self->frame_surface);
	self->frame_surface = surface;
	self->video_width = w;
	self->video_height = h;
	g_mutex_unlock (&self->frame_mutex);

	/* Schedule a redraw on the main/GUI thread */
	if (self->video_area != NULL)
		gtk_widget_queue_draw (self->video_area);

	return GST_FLOW_OK;
}

static void
play_pause_clicked_cb (GtkButton *btn, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);
	GstState state;
	GtkWidget *img;

	if (self->pipeline == NULL)
		return;

	gst_element_get_state (self->pipeline, &state, NULL, 0);

	if (state == GST_STATE_PLAYING) {
		gst_element_set_state (self->pipeline, GST_STATE_PAUSED);
		img = gtk_image_new_from_icon_name (
			"media-playback-start-symbolic",
			GTK_ICON_SIZE_SMALL_TOOLBAR);
	} else {
		gst_element_set_state (self->pipeline, GST_STATE_PLAYING);
		img = gtk_image_new_from_icon_name (
			"media-playback-pause-symbolic",
			GTK_ICON_SIZE_SMALL_TOOLBAR);
	}

	gtk_button_set_image (GTK_BUTTON (btn), img);
}

static void
mute_clicked_cb (GtkButton *btn, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);
	GtkWidget *img;

	if (self->pipeline == NULL)
		return;

	self->video_muted = !self->video_muted;
	g_object_set (self->pipeline, "mute", self->video_muted, NULL);

	img = gtk_image_new_from_icon_name (
		self->video_muted ? "audio-volume-muted-symbolic"
		                  : "audio-volume-high-symbolic",
		GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image (GTK_BUTTON (btn), img);
}

static gboolean
seek_position_update_cb (gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);
	gint64 pos, dur;

	if (self->pipeline == NULL)
		return G_SOURCE_REMOVE;

	if (self->seek_lock)
		return G_SOURCE_CONTINUE;

	if (!gst_element_query_position (self->pipeline, GST_FORMAT_TIME,
	                                 &pos))
		return G_SOURCE_CONTINUE;

	if (gst_element_query_duration (self->pipeline, GST_FORMAT_TIME,
	                                &dur) && dur > 0) {
		gtk_range_set_value (GTK_RANGE (self->seek_scale),
		                     (gdouble) pos / (gdouble) dur);
	}

	return G_SOURCE_CONTINUE;
}

static gboolean
seek_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);
	(void) widget;
	(void) event;
	self->seek_lock = TRUE;
	return FALSE;
}

static gboolean
seek_release_cb (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);
	gint64 dur;
	gdouble fraction;

	(void) widget;
	(void) event;
	self->seek_lock = FALSE;

	if (self->pipeline == NULL)
		return FALSE;

	fraction = gtk_range_get_value (GTK_RANGE (self->seek_scale));
	if (gst_element_query_duration (self->pipeline, GST_FORMAT_TIME,
	                                &dur) && dur > 0) {
		gst_element_seek_simple (self->pipeline,
			GST_FORMAT_TIME,
			GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
			(gint64)(fraction * dur));
	}

	return FALSE;
}

static gboolean
video_area_draw_cb (GtkWidget *widget,
                    cairo_t   *cr,
                    gpointer   data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);
	GtkAllocation alloc;

	gtk_widget_get_allocation (widget, &alloc);

	/* Always paint black background */
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_paint (cr);

	g_mutex_lock (&self->frame_mutex);
	if (self->frame_surface != NULL && self->video_width > 0 && self->video_height > 0) {
		double sx = (double) alloc.width  / (double) self->video_width;
		double sy = (double) alloc.height / (double) self->video_height;
		double scale = (sx < sy) ? sx : sy;
		double dx = (alloc.width  - self->video_width  * scale) / 2.0;
		double dy = (alloc.height - self->video_height * scale) / 2.0;

		cairo_save (cr);
		cairo_translate (cr, dx, dy);
		cairo_scale (cr, scale, scale);
		cairo_set_source_surface (cr, self->frame_surface, 0, 0);
		cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_BILINEAR);
		cairo_paint (cr);
		cairo_restore (cr);
	}
	g_mutex_unlock (&self->frame_mutex);

	return TRUE;
}

static void
preview_show_media (NemoQuickPreview *self, GFile *file)
{
	char *uri;
	GstBus *bus;
	GstElement *appsink;
	GstCaps *caps;

	media_stop (self);

	if (!gst_is_initialized ()) {
		if (!gst_init_check (NULL, NULL, NULL)) {
			g_warning ("Quick preview: GStreamer init failed");
			preview_show_paged (self, file, NEMO_VIEWER_MODE_HEX);
			return;
		}
	}

	self->pipeline = gst_element_factory_make ("playbin", "quick-preview-player");
	if (self->pipeline == NULL) {
		g_warning ("Quick preview: could not create playbin element");
		preview_show_paged (self, file, NEMO_VIEWER_MODE_HEX);
		return;
	}

	/* Use appsink as video sink — we extract frames and render with Cairo */
	appsink = gst_element_factory_make ("appsink", "qp-vsink");
	if (appsink == NULL) {
		g_warning ("Quick preview: could not create appsink element");
		gst_object_unref (self->pipeline);
		self->pipeline = NULL;
		preview_show_paged (self, file, NEMO_VIEWER_MODE_HEX);
		return;
	}

	/* Request BGRx format which maps directly to Cairo's RGB24 on little-endian */
	caps = gst_caps_from_string ("video/x-raw, format=BGRx");
	g_object_set (appsink,
	              "caps", caps,
	              "emit-signals", TRUE,
	              "max-buffers", 2,
	              "drop", TRUE,
	              NULL);
	gst_caps_unref (caps);

	g_signal_connect (appsink, "new-sample",
	                  G_CALLBACK (new_sample_cb), self);

	g_object_set (self->pipeline, "video-sink", appsink, NULL);

	uri = g_file_get_uri (file);
	g_object_set (self->pipeline,
	              "uri", uri,
	              "mute", self->video_muted,
	              NULL);
	g_free (uri);

	/* Make the media page visible */
	self->mode = PREVIEW_MEDIA;
	gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "media");
	gtk_widget_show_all (GTK_WIDGET (self));

	/* Bus watch for EOS / errors */
	bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
	self->bus_watch_id = gst_bus_add_watch (bus, media_bus_callback, self);
	gst_object_unref (bus);

	/* Reset transport controls */
	gtk_button_set_image (GTK_BUTTON (self->play_btn),
		gtk_image_new_from_icon_name (
			"media-playback-start-symbolic",
			GTK_ICON_SIZE_SMALL_TOOLBAR));
	gtk_button_set_image (GTK_BUTTON (self->mute_btn),
		gtk_image_new_from_icon_name (
			self->video_muted ? "audio-volume-muted-symbolic"
			                  : "audio-volume-high-symbolic",
			GTK_ICON_SIZE_SMALL_TOOLBAR));

	gtk_range_set_range (GTK_RANGE (self->seek_scale), 0.0, 1.0);
	gtk_range_set_value (GTK_RANGE (self->seek_scale), 0.0);
	self->seek_lock = FALSE;

	if (self->seek_update_id != 0) {
		g_source_remove (self->seek_update_id);
	}
	self->seek_update_id = g_timeout_add (250, seek_position_update_cb, self);

	/* Start PAUSED — user clicks Play to begin */
	gst_element_set_state (self->pipeline, GST_STATE_PAUSED);
}

static void
media_stop (NemoQuickPreview *self)
{
	if (self->seek_update_id > 0) {
		g_source_remove (self->seek_update_id);
		self->seek_update_id = 0;
	}

	if (self->pipeline != NULL) {
		gst_element_set_state (self->pipeline, GST_STATE_NULL);
		gst_object_unref (self->pipeline);
		self->pipeline = NULL;
	}

	if (self->bus_watch_id > 0) {
		g_source_remove (self->bus_watch_id);
		self->bus_watch_id = 0;
	}

	g_mutex_lock (&self->frame_mutex);
	if (self->frame_surface != NULL) {
		cairo_surface_destroy (self->frame_surface);
		self->frame_surface = NULL;
	}
	self->video_width = 0;
	self->video_height = 0;
	g_mutex_unlock (&self->frame_mutex);
}

#endif /* HAVE_GSTREAMER */



/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void
nemo_quick_preview_show_file (NemoQuickPreview *self,
                              GFile            *file,
                              GtkWindow        *parent)
{
	g_return_if_fail (NEMO_IS_QUICK_PREVIEW (self));
	g_return_if_fail (G_IS_FILE (file));

	/* Build the directory file list for navigation */
	populate_dir_file_list (self, file);

	/* In the modal quick preview, make sure the toplevel exists before
	 * media setup tries to create a native child window for video. */
	self->parent_window = parent;
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (self), parent);
	gtk_widget_show_all (GTK_WIDGET (self));
	gtk_window_present (GTK_WINDOW (self));

	/* Load the file content */
	load_file_content (self, file);

	/* Update navigation UI */
	update_navigation_ui (self);
}

void
nemo_quick_preview_dismiss (NemoQuickPreview *self)
{
	g_return_if_fail (NEMO_IS_QUICK_PREVIEW (self));

	preview_clear (self);
	navigation_clear (self);
	gtk_widget_hide (GTK_WIDGET (self));

	/* Return focus to parent */
	if (self->parent_window != NULL && gtk_widget_get_visible (GTK_WIDGET (self->parent_window))) {
		gtk_window_present (self->parent_window);
	}

	self->parent_window = NULL;
}
