#!/bin/sh
# Check ncurses compatibility

# What library to link
ldflags()
{
	for pkg in ncursesw ncurses; do
		if pkg-config --exists "$pkg" 2>/dev/null; then
			pkg-config --libs "$pkg"
			return $?
		fi
	done
	for ext in so a dll.a dylib ; do
		for lib in ncursesw ncurses curses ; do
			"$@" -print-file-name=lib${lib}.${ext} | grep -q /
			if [ $? -eq 0 ]; then
				echo "-l${lib}"
				exit
			fi
		done
	done
	exit 1
}

# Where is ncurses.h?
ccflags()
{
	for pkg in ncursesw ncurses; do
		if pkg-config --exists "$pkg" 2>/dev/null; then
			pkg-config --cflags "$pkg" || return 1
			printf '%s\n' '-DCURSES_LOC="<curses.h>"'
			return 0
		fi
	done
	if [ -f /usr/include/ncursesw/curses.h ]; then
		echo '-I/usr/include/ncursesw -DCURSES_LOC="<curses.h>"'
		echo ' -DNCURSES_WIDECHAR=1'
	elif [ -f /usr/include/ncurses/ncurses.h ]; then
		echo '-I/usr/include/ncurses -DCURSES_LOC="<ncurses.h>"'
	elif [ -f /usr/include/ncurses/curses.h ]; then
		echo '-I/usr/include/ncurses -DCURSES_LOC="<curses.h>"'
	elif [ -f /usr/include/ncurses.h ]; then
		echo '-DCURSES_LOC="<ncurses.h>"'
	else
		echo '-DCURSES_LOC="<curses.h>"'
	fi
}

# Temp file, try to clean up after us


# Check if we can link to ncurses
check() {
        tmp=$(mktemp .lxdialog.XXXXXXXX) || exit 1
        trap 'rm -f "$tmp"' 0
        trap 'exit 1' 1 2 3 15
        "$@" -o "$tmp" <<'EOF'
#include CURSES_LOC
int main(void)
{
    WINDOW *win = initscr();
    if (!win)
        return 1;
    endwin();
    return 0;
}
EOF
	if [ $? != 0 ]; then
	    echo " *** Unable to find the ncurses libraries or the"       1>&2
	    echo " *** required header files."                            1>&2
	    echo " *** 'make menuconfig' requires the ncurses libraries." 1>&2
	    echo " *** "                                                  1>&2
	    echo " *** Install ncurses (ncurses-devel) and try again."    1>&2
	    echo " *** "                                                  1>&2
	    exit 1
	fi
}

usage() {
	printf 'Usage: %s [-check compiler options|-ccflags|-ldflags compiler options]\n' "$0"
}

if [ $# -eq 0 ]; then
	usage
	exit 1
fi

case "$1" in
	"-check")
		shift
		check "$@"
		;;
	"-ccflags")
		ccflags
		;;
	"-ldflags")
		shift
		ldflags "$@"
		;;
	*)
		usage
		exit 1
		;;
esac
