/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-preview-utils.c — Shared MIME-type helpers for preview widgets
 *
 * Copyright (C) 2026 smplOS contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <config.h>
#include "nemo-preview-utils.h"

#include <gio/gio.h>
#include <string.h>

gboolean
nemo_preview_mime_is_image (const gchar *mime)
{
	if (mime == NULL)
		return FALSE;

	return g_str_has_prefix (mime, "image/") ||
	       g_content_type_is_a (mime, "image/*");
}

gboolean
nemo_preview_mime_is_raw_image (const gchar *mime)
{
	if (mime == NULL)
		return FALSE;

	/* Camera RAW formats that gdk-pixbuf cannot decode natively */
	static const char *raw_types[] = {
		"image/x-sony-arw",
		"image/x-adobe-dng",
		"image/x-canon-cr2",
		"image/x-canon-cr3",
		"image/x-canon-crw",
		"image/x-nikon-nef",
		"image/x-nikon-nrw",
		"image/x-olympus-orf",
		"image/x-pentax-pef",
		"image/x-panasonic-rw2",
		"image/x-panasonic-raw",
		"image/x-fuji-raf",
		"image/x-samsung-srw",
		"image/x-sigma-x3f",
		"image/x-minolta-mrw",
		"image/x-kodak-dcr",
		"image/x-kodak-kdc",
		"image/x-raw",
		"image/x-dcraw",
		NULL
	};

	for (int i = 0; raw_types[i] != NULL; i++) {
		if (g_strcmp0 (mime, raw_types[i]) == 0)
			return TRUE;
	}

	return FALSE;
}

gboolean
nemo_preview_mime_is_text (const gchar *mime)
{
	if (mime == NULL)
		return FALSE;

	if (g_str_has_prefix (mime, "text/"))
		return TRUE;

	if (g_content_type_is_a (mime, "text/plain"))
		return TRUE;

	/* Common source code and configuration types that are really text */
	static const char *text_types[] = {
		"application/json",
		"application/xml",
		"application/javascript",
		"application/x-shellscript",
		"application/x-perl",
		"application/x-ruby",
		"application/x-python",
		"application/x-desktop",
		"application/toml",
		"application/yaml",
		"application/x-yaml",
		"application/sql",
		"application/x-awk",
		"application/x-m4",
		"application/xslt+xml",
		NULL
	};

	for (int i = 0; text_types[i] != NULL; i++) {
		if (g_strcmp0 (mime, text_types[i]) == 0 ||
		    g_content_type_is_a (mime, text_types[i]))
			return TRUE;
	}

	return FALSE;
}

gboolean
nemo_preview_mime_is_video (const gchar *mime)
{
	if (mime == NULL)
		return FALSE;

	return g_str_has_prefix (mime, "video/") ||
	       g_content_type_is_a (mime, "video/*");
}

gboolean
nemo_preview_mime_is_audio (const gchar *mime)
{
	if (mime == NULL)
		return FALSE;

	return g_str_has_prefix (mime, "audio/") ||
	       g_content_type_is_a (mime, "audio/*");
}

gboolean
nemo_preview_mime_is_media (const gchar *mime)
{
	return nemo_preview_mime_is_video (mime) ||
	       nemo_preview_mime_is_audio (mime);
}
