Limba
==============
[![Build Status](https://travis-ci.org/ximion/limba.svg?branch=master)](https://travis-ci.org/ximion/limba)

Limba is a new project which allows 3rd-party software installations on Linux.
It is based on ideas of Glick2 and Listaller, and uses modern Linux kernel features
to allow applications to share libraries and other components, reducing the amount
of duplicate software components running on a Linux system.

![Limba logo](data/limba-small.png "Logo")

## User information
In order to run Limba applications, you will need a Linux kernel supporting OverlayFS.
This is at least 3.18[1], but many distributions offer it already in their own kernels.

Please keep in mind that this is EXPERIMENTAL software - you may play around with it,
but don't expect it to work, or work as expected.

[1]: http://lwn.net/Articles/618140/

## Developers

### Dependencies

 * glib2 (>= 2.36)
 * GObject-Introspection
 * AppStream (libappstream)
 * libarchive
 * GPGMe
 * libuuid

### Compilation instructions
 TODO


