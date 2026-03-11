/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-preview-utils.h — Shared MIME-type helpers for preview widgets
 *
 * Used by both the sidebar preview pane (NemoPreviewPane) and the F3
 * quick-preview window (NemoQuickPreview) so the classification logic
 * lives in one place.
 *
 * Copyright (C) 2026 smplOS contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef NEMO_PREVIEW_UTILS_H
#define NEMO_PREVIEW_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

gboolean nemo_preview_mime_is_image (const gchar *mime_type);
gboolean nemo_preview_mime_is_raw_image (const gchar *mime_type);
gboolean nemo_preview_mime_is_text  (const gchar *mime_type);
gboolean nemo_preview_mime_is_video (const gchar *mime_type);
gboolean nemo_preview_mime_is_audio (const gchar *mime_type);
gboolean nemo_preview_mime_is_media (const gchar *mime_type);

G_END_DECLS

#endif /* NEMO_PREVIEW_UTILS_H */
