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
#include <gst/video/videooverlay.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
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
	guintptr     video_xid;
#endif

	/* Directory analyzer (F3 on folders) */
	NemoDirAnalyzer *dir_analyzer;

	/* Parent tracking */
	GtkWindow   *parent_window;
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
#endif

/* ------------------------------------------------------------------ */
/* Key press — Esc dismisses                                          */
/* ------------------------------------------------------------------ */

static gboolean
on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (widget);

	if (event->keyval == GDK_KEY_Escape) {
		nemo_quick_preview_dismiss (self);
		return GDK_EVENT_STOP;
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
/* Construction                                                       */
/* ------------------------------------------------------------------ */

static void
nemo_quick_preview_init (NemoQuickPreview *self)
{
	self->mode = PREVIEW_NONE;
	self->parent_window = NULL;
#ifdef HAVE_GSTREAMER
	self->pipeline = NULL;
	self->bus_watch_id = 0;
	self->video_xid = 0;
#endif

	/* Window setup */
	gtk_window_set_default_size (GTK_WINDOW (self), DEFAULT_WIDTH, DEFAULT_HEIGHT);
	gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_type_hint (GTK_WINDOW (self), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (self), TRUE);

	/* Header bar */
	self->header_bar = gtk_header_bar_new ();
	gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->header_bar), TRUE);
	gtk_header_bar_set_title (GTK_HEADER_BAR (self->header_bar), _("Quick Preview"));
	gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->header_bar), "");
	gtk_window_set_titlebar (GTK_WINDOW (self), self->header_bar);

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
	gtk_widget_set_double_buffered (self->video_area, FALSE);
	gtk_box_pack_start (GTK_BOX (self->media_box), self->video_area, TRUE, TRUE, 0);
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

	preview_clear (self);

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
		gst_message_parse_error (message, &err, NULL);
		g_warning ("Quick preview GStreamer error: %s", err->message);
		g_error_free (err);
		media_stop (self);
		break;
	}
	default:
		break;
	}

	return TRUE;
}

#ifdef GDK_WINDOWING_X11
static GstBusSyncReply
media_bus_sync_handler (GstBus *bus, GstMessage *message, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);

	if (!gst_is_video_overlay_prepare_window_handle_message (message))
		return GST_BUS_PASS;

	if (self->video_xid != 0) {
		GstVideoOverlay *overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));
		gst_video_overlay_set_window_handle (overlay, self->video_xid);
	}

	gst_message_unref (message);
	return GST_BUS_DROP;
}
#endif /* GDK_WINDOWING_X11 */

static void
on_video_area_realize (GtkWidget *widget, gpointer data)
{
	NemoQuickPreview *self = NEMO_QUICK_PREVIEW (data);

#ifdef GDK_WINDOWING_X11
	GdkWindow *window = gtk_widget_get_window (widget);
	if (GDK_IS_X11_WINDOW (window)) {
		if (!gdk_window_ensure_native (window))
			g_warning ("Quick preview: could not create native window for video");
		self->video_xid = GDK_WINDOW_XID (window);
	}
#endif
}

static void
preview_show_media (NemoQuickPreview *self, GFile *file)
{
	char *uri;
	GstBus *bus;

	media_stop (self);

	self->pipeline = gst_element_factory_make ("playbin", "quick-preview-player");
	if (self->pipeline == NULL) {
		g_warning ("Quick preview: could not create playbin element");
		preview_show_paged (self, file, NEMO_VIEWER_MODE_HEX);
		return;
	}

	uri = g_file_get_uri (file);
	g_object_set (self->pipeline, "uri", uri, NULL);
	g_free (uri);

	/* Bus watch for EOS / errors */
	bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
	self->bus_watch_id = gst_bus_add_watch (bus, media_bus_callback, self);

#ifdef GDK_WINDOWING_X11
	gst_bus_set_sync_handler (bus, media_bus_sync_handler, self, NULL);
#endif

	gst_object_unref (bus);

	/* Ensure video area is realized for XID */
	if (!gtk_widget_get_realized (self->video_area)) {
		g_signal_connect (self->video_area, "realize",
		                  G_CALLBACK (on_video_area_realize), self);
	} else {
		on_video_area_realize (self->video_area, self);
	}

	self->mode = PREVIEW_MEDIA;
	gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "media");

	gst_element_set_state (self->pipeline, GST_STATE_PLAYING);
}

static void
media_stop (NemoQuickPreview *self)
{
	if (self->pipeline != NULL) {
		gst_element_set_state (self->pipeline, GST_STATE_NULL);
		gst_object_unref (self->pipeline);
		self->pipeline = NULL;
	}

	if (self->bus_watch_id > 0) {
		g_source_remove (self->bus_watch_id);
		self->bus_watch_id = 0;
	}

	self->video_xid = 0;
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
	GFileInfo *info;
	const gchar *content_type;
	const gchar *display_name;
	gchar *size_str;
	goffset size;
	char *subtitle;

	g_return_if_fail (NEMO_IS_QUICK_PREVIEW (self));
	g_return_if_fail (G_IS_FILE (file));

	preview_clear (self);

	/* Query file info */
	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
	                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
	                          G_FILE_ATTRIBUTE_STANDARD_SIZE ","
	                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL, NULL);

	if (info == NULL) {
		goto show;
	}

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
		goto show;
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

show:
	self->parent_window = parent;
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (self), parent);

	gtk_widget_show_all (GTK_WIDGET (self));
	gtk_window_present (GTK_WINDOW (self));
}

void
nemo_quick_preview_dismiss (NemoQuickPreview *self)
{
	g_return_if_fail (NEMO_IS_QUICK_PREVIEW (self));

	preview_clear (self);
	gtk_widget_hide (GTK_WIDGET (self));

	/* Return focus to parent */
	if (self->parent_window != NULL && gtk_widget_get_visible (GTK_WIDGET (self->parent_window))) {
		gtk_window_present (self->parent_window);
	}

	self->parent_window = NULL;
}
