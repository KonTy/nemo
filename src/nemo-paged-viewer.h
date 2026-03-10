/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-paged-viewer.h — Custom widget for viewing arbitrarily large files
 *
 * Uses pread() with an LRU page cache (like `less` / Double Commander),
 * so only a few pages (~512 KB) are ever resident regardless of file size.
 * Supports TEXT and HEX display modes.
 *
 * Copyright (C) 2026 smplOS contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef NEMO_PAGED_VIEWER_H
#define NEMO_PAGED_VIEWER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_PAGED_VIEWER (nemo_paged_viewer_get_type ())
G_DECLARE_FINAL_TYPE (NemoPagedViewer, nemo_paged_viewer, NEMO, PAGED_VIEWER, GtkBox)

typedef enum {
	NEMO_VIEWER_MODE_TEXT,
	NEMO_VIEWER_MODE_HEX,
} NemoViewerMode;

NemoPagedViewer *nemo_paged_viewer_new        (void);
gboolean         nemo_paged_viewer_open_file  (NemoPagedViewer *self,
                                                const gchar     *path,
                                                GError         **error);
void             nemo_paged_viewer_close_file (NemoPagedViewer *self);
void             nemo_paged_viewer_set_mode   (NemoPagedViewer *self,
                                                NemoViewerMode   mode);

G_END_DECLS

#endif /* NEMO_PAGED_VIEWER_H */
