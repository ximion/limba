#!/bin/sh

start ()
{
	rm -Rf autom4te.cache
	echo "Configuring..."
	exec ./configure "$@"
}

echo "Running libtoolize..."      && libtoolize -c -f && \
echo "Running aclocal..."         && aclocal && \
echo "Running autoconf..."        && autoconf && \
echo "Running automake --gnu..."  && automake --gnu -a -c && \
start "$@"
