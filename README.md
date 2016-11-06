# Modified rxvt-unicode to get widths of glyphs from the font itself

This is an experimental/modified version of rxvt-unicode to fix issues with
wide glyphs.

## The problem with wcwidth(3)

`wcwidth` is typically used to get the columns needed for wide characters, but
there are ambigious regions in the Unicode standard, and while Unicode 9 fixes
this for Emojis, there are also private use areas, where the width is
undefined, and really depends on the font you are using (e.g. FontAwesome or
powerline-extra-symbols, which has glyphs that are 4 cells wide).

## The approach

With this patch rxvt-unicode asks the font about the actual width (in
`rxvt_term::scr_add_lines`, via `rxvt_term::rxvt_wcwidth` and
`rxvt_font_xft::get_wcwidth`).

rxvtwcwidth.so is then used to overwrite `wcwidth` and `wcswidth` for client
programs (via `LD_PRELOAD`), and asks rxvt-unicode through a Unix socket.

There is caching in place in both the shared library and
`rxvt_term::rxvt_wcwidth`.  TODO: This needs to be cleaned up / improved.

## Installation

There is no `configure` switch for it yet, so you can do the normal
`./configure`, e.g.

    ./configure --enable-unicode3 --enable-256-color --disable-smart-resize

See README.configure for other options.

A PKGBUILD for Arch is available at:
https://github.com/blueyed/PKGBUILD-rxvt-unicode-wcwidthcallback

### Setup

rxvt-unicode will then already use the improved character width handling, but
to fully enable it, you have to set `LD_PRELOAD` (depending on where you
installed rxvt-unicode):

    export LD_PRELOAD=/usr/local/lib/urxvt/rxvtwcwidth.so

In the end this could be set automatically.

## TODO
 - currently only Xft fonts are handled.  I don't know if it makes sense for
   others after all?!
 - The code needs to be reviewed to ensure that there is no performance
   regression, especially when not using the mechanism at all.
 - Improve detection of our callback being used, i.e. the shell has used it,
   but the program in it has not.  This gets done in a rudimentary way in
   `rxvt_term::scr_add_lines` already; both for the internal width, and when
   NOCHAR is being replaced (e.g. Vim writing the Space after a wide glyph).
   Also when using tmux, you might attach a client where the server has not
   loaded it.
 - Handle chars carefully where the system wcwidth and rxvt-unicode's
   get_wcwidth disagree.
 - Can /etc/ld.so.conf.d or another method be used instead of LD_PRELOAD?!
