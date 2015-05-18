/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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
	if (!isatty (fileno (stdin)))
		return;

	ioctl (0, TIOCGWINSZ, &ws);

	width = ws.ws_col - strlen(title) - 2;
	bar_width = width - 2 - 5;

	/* only continue if we have space to draw the progress bar */
	if (width < 0)
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

	g_print ("\r%s [%s%s] %s%i%%", title, bar_fill_str, bar_space_str, perc_space_str, (int) progress);

	g_free (bar_fill_str);
	g_free (bar_space_str);
}