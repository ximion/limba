/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "li-console-utils.h"

#include <config.h>
#include <glib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

/**
 * SECTION:li-console-utils
 * @short_description: Helper functions shared between Limba console applications
 */

static gboolean
li_is_verbose_mode (void)
{
	return g_getenv ("G_MESSAGES_DEBUG") != NULL;
}

/**
 * li_write_progress_step:
 */
void
li_write_progress_step (const gchar *format, ...)
{
	gint width;
	gchar *space_str;
	struct winsize ws;
	va_list args;
	gchar *text;

	va_start (args, format);
	text = g_strdup_vprintf (format, args);
	va_end (args);

	/* if this is no tty, just write the text and exit */
	if ((!isatty (fileno (stdin))) || (li_is_verbose_mode ())) {
		g_print ("%s\n", text);
		g_free (text);
		return;
	}

	ioctl (0, TIOCGWINSZ, &ws);
	width = ws.ws_col;

	/* wipe current line */
	space_str = g_strnfill (width, ' ');
	g_print ("\r%s", space_str);
	g_free (space_str);

	/* print our message */
	g_print ("\r%s\n", text);

	g_free (text);
}

/**
 * li_draw_progress_bar:
 */
void
li_draw_progress_bar (const gchar *title, guint progress)
{
	gint width;
	gint bar_width;
	gint bar_segments;
	gchar *bar_fill_str;
	gchar *bar_space_str;
	const gchar *perc_space_str;
	struct winsize ws;

	/* don't create a progress bar if we don't have a tty */
	if ((!isatty (fileno (stdin))) || (li_is_verbose_mode ()))
		return;

	ioctl (0, TIOCGWINSZ, &ws);

	width = ws.ws_col - strlen(title) - 2;
	bar_width = width - 2 - 4;

	/* only continue if we have space to draw the progress bar */
	if (width <= 0)
		return;

	if (bar_width <= 0) {
		/* no space for a progress bar */
		g_print ("\r%s ", title);
		return;
	}

	if (progress < 100) {
		bar_segments = round ((double) bar_width / 100 * (double) progress);

		bar_fill_str = g_strnfill (bar_segments, '=');
		bar_space_str = g_strnfill (bar_width - bar_segments, ' ');
		if (progress < 10)
			perc_space_str = "  ";
		else
			perc_space_str = " ";
	} else {
		bar_fill_str = g_strnfill (bar_width, '=');
		bar_space_str = g_strdup ("");
		perc_space_str = "";
	}

	g_print ("%s [%s%s] %s%i%%\r", title, bar_fill_str, bar_space_str, perc_space_str, (int) progress);

	g_free (bar_fill_str);
	g_free (bar_space_str);

	if (progress >= 100)
		g_print ("\n");
}

/**
 * li_abort_progress_bar:
 */
void
li_abort_progress_bar (void)
{
	gint width;
	struct winsize ws;

	/* don't create a progress bar if we don't have a tty */
	if ((!isatty (fileno (stdin))) || (li_is_verbose_mode ()))
		return;

	ioctl (0, TIOCGWINSZ, &ws);
	width = ws.ws_col - 2;

	/* make a rough estimate if there is a progress bar at all, which we could cancel */
	if (width - 2 - 5 <= 0)
		return;

	g_print ("\n");
}

/**
 * li_print_stderr:
 */
void
li_print_stderr (const gchar *format, ...)
{
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_printerr ("%s\n", str);

	g_free (str);
}

/**
 * li_print_stdout:
 */
void
li_print_stdout (const gchar *format, ...)
{
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_print ("%s\n", str);

	g_free (str);
}
