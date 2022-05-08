/*
 * QEmacs, tiny but powerful multimode editor
 *
 * Copyright (c) 2022-2022 undx.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "qe.h"

#if 0
/* dump trash here! */
static void do_some_stuff(EditState * s, int start, int end)
{
	char buf[800];
	size_t len;
	len = qe_get_word(s, buf, sizeof buf, s->offset, NULL);
	if (len >= sizeof(buf)) {
		put_status(s, "word too large");
		return;
	} else {
		put_status(s, "thing-at-point : %s.", buf);
	}
}
#endif

/* Try to expand text after point. */
static void do_hippie_expand(EditState * s, int args)
{
	if (check_read_only(s))
		return;
	// build tokens table for each buffer.
	// priorize tokens from current buffer.
	// use TAGS file eventually.
	// cycle thru best matches.

	put_status(s, "Not implemented yet!");
}

/* Move up/down line. */
static void do_move_line_up_down(EditState * s, int args)
{
	if (check_read_only(s))
		return;

	/* first simple implementation
	 * TODO optimize w/o kill and yank to preserve yank register
	 */
	int start, end, line, column, offset = s->offset;

	eb_get_pos(s->b, &line, &column, offset);
	do_bol(s);
	start = s->offset;
	do_eol(s);
	end = s->offset + 1;
	do_kill(s, start, end, 0, 0);
	do_up_down(s, args);
	// TODO yank checks if we're in comment and corrupts the stuff...
	do_yank(s);
	do_up_down(s, -1);
	s->offset = s->offset + column;
	put_status(s, "Moved line %s.", args < 0 ? "up" : "down");
}

/* Kill current line. */
static void do_kill_whole_line(EditState * s, int args)
{
	if (check_read_only(s))
		return;

	int start, end;
	do_bol(s);
	start = s->offset;
	do_eol(s);
	end = s->offset + 1;
	do_kill(s, start, end, 1, 0);
	put_status(s, "Killed whole line.");
}

/* Duplicate region. */
static void do_duplicate_region(EditState * s)
{
	int start, end, point, mark, size;

	point = s->offset;
	mark = s->b->mark;
	size = point - mark;
	if (size < 0) {
		start = point;
		end = mark;
	} else {
		start = mark;
		end = point;
	}
	size = abs(size);
	eb_insert_buffer(s->b, end, s->b, start, size);
	put_status(s, "Duplicated region.");
}

/* Duplicate current line. */
static void do_duplicate_line(EditState * s)
{
	int start, end, size;

	do_bol(s);
	start = s->offset;
	do_eol(s);
	end = s->offset;
	end++;
	size = abs(end - start);
	// put_status(s, "start=%d end=%d size=%d.", start, end, size);
	eb_insert_buffer(s->b, end, s->b, start, size);
	s->offset = s->offset + 1;
	put_status(s, "Duplicated line.");
}

/* Entry point for duplicating things like line or current region. */
static void do_duplicate_thing_at_point(EditState * s)
{
	if (check_read_only(s))
		return;

	/* check if a region is selected and point is in it */
	if (s->region_style == QE_STYLE_REGION_HILITE) {
		do_duplicate_region(s);
	} else {
		do_duplicate_line(s);
	}
}

/* Zap forward/backward to char (excluded). */
static void do_zap_up_back_to_char(EditState * s, const char *zap, int dir)
{
	if (check_read_only(s))
		return;

	int from, to, offset, bob, eob;
	char c, z;
	z = zap[0];		// TODO add a signature like ESci to avoid this...

	eob = s->b->total_size;
	bob = 0;
	from = to = s->offset;
	if (dir > 0) {
		c = eb_nextc(s->b, from, &offset);
		while (c != z && offset < eob)
			c = eb_nextc(s->b, offset, &offset);
		to = --offset;
	} else {
		c = eb_prevc(s->b, from, &offset);
		while (c != z && offset > bob)
			c = eb_prevc(s->b, offset, &offset);
		to = ++offset;
	}
	if (c == z) {
		eb_delete_range(s->b, from, to);
		put_status(s, "Zapped %s to %c.", dir > 1 ? "up" : "back", z);
	}
}

/* Delete all blanks before and after point but keep just one space.
 * Inspired from delete-horizontal-space.
 */
static void do_just_one_space(EditState * s)
{
	if (check_read_only(s))
		return;

	int from, to, offset;

	from = to = s->offset;
	while (qe_isblank(eb_prevc(s->b, from, &offset)))
		from = offset;

	while (qe_isblank(eb_nextc(s->b, to, &offset)))
		to = offset;
	// TODO check left boundary if no space forward.
	if (to != from)
		to--;

	eb_delete_range(s->b, from, to);
}

/* Join two lines. */
static void do_join_line(EditState * s, int args)
{
	if (check_read_only(s))
		return;

	do_eol(s);
	do_kill(s, s->offset, s->offset + 1, args, 0);
	do_just_one_space(s);
}

static CmdDef extra_commands[] = {
	CMD2(KEY_META(' '), KEY_NONE, "just-one-space", do_just_one_space, ES,
	     "*")
	    CMD3(KEY_META('z'), KEY_NONE, "zap-up-to-char",
		 do_zap_up_back_to_char, ESsi, 1,
		 "s{Zap up to:}[char]|char|" "v")
	    CMD3(KEY_META('Z'), KEY_NONE, "zap-back-to-char",
		 do_zap_up_back_to_char, ESsi, -1,
		 "s{Zap back to:}[char]|char|" "v")
	    CMD2(KEY_META('a'), KEY_NONE, "kill-whole-line", do_kill_whole_line,
		 ESi, "v")
	    CMD2(KEY_META('j'), KEY_NONE, "join-line", do_join_line, ESi, "v")
	    CMD2(KEY_META('c'), KEY_NONE, "duplicate-thing-at-point",
		 do_duplicate_thing_at_point, ES, "*")
	    CMD3(KEY_CTRL('<'), KEY_NONE, "move-line-up", do_move_line_up_down,
		 ESi, -1, "*v")
	    CMD3(KEY_CTRL('>'), KEY_NONE, "move-line-down",
		 do_move_line_up_down, ESi, 1, "*v")
	    CMD2(KEY_META('/'), KEY_NONE, "hippie-expand", do_hippie_expand,
		 ESi, "*")
	CMD_DEF_END
};

static int undx_init(void)
{
	qe_register_cmd_table(extra_commands, NULL);
	for (int key = KEY_META('0'); key <= KEY_META('9'); key++) {
		qe_register_binding(key, "numeric-argument", NULL);
	}

	return 0;
}

qe_module_init(undx_init);
