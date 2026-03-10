/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-image-viewer.h — Reusable image-preview widget
 *
 * Supports static images and animated GIFs (via GdkPixbufAnimation),
 * zoom / fit-to-container scaling, and optional zoom controls.
 *
 * Used by both the sidebar preview pane (NemoPreviewPane) and the F3
 * quick-preview window (NemoQuickPreview).
 *
 * Copyright (C) 2026 smplOS contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef NEMO_IMAGE_VIEWER_H
#define NEMO_IMAGE_VIEWER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_IMAGE_VIEWER (nemo_image_viewer_get_type ())
G_DECLARE_FINAL_TYPE (NemoImageViewer, nemo_image_viewer,
		      NEMO, IMAGE_VIEWER, GtkBox)

NemoImageViewer *nemo_image_viewer_new               (void);

/* Synchronous load (blocks on I/O — fine for the F3 quick-preview). */
gboolean         nemo_image_viewer_load_file          (NemoImageViewer *self,
                                                       const gchar     *path,
                                                       GError         **error);

/* Asynchronous load (used by the sidebar preview pane). */
void             nemo_image_viewer_load_stream_async  (NemoImageViewer *self,
                                                       GInputStream    *stream,
                                                       GCancellable    *cancellable);

/* Clear the current image. */
void             nemo_image_viewer_clear              (NemoImageViewer *self);

/* Zoom: 1.0 = original size. */
void             nemo_image_viewer_set_zoom           (NemoImageViewer *self,
                                                       double           zoom);
double           nemo_image_viewer_get_zoom           (NemoImageViewer *self);

/* Fit-to-container mode.  When TRUE the zoom level is computed
 * automatically whenever the container is resized. */
void             nemo_image_viewer_set_fit            (NemoImageViewer *self,
                                                       gboolean         fit);
gboolean         nemo_image_viewer_get_fit            (NemoImageViewer *self);

/* Show/hide the built-in zoom-slider + fit checkbox. */
void             nemo_image_viewer_set_show_controls  (NemoImageViewer *self,
                                                       gboolean         show);

G_END_DECLS

#endif /* NEMO_IMAGE_VIEWER_H */
