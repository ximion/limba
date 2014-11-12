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
Ensure all build dependencies are installed.
Then run the following commands to compile (and install) Limba:
```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

### Testing Limba
Run "make test" first, This will produce two packages (file extension .ipk) in the tests/data directory.
You can install these packages using
```bash
sudo lipa install file.ipk
```
One package depends on the other (so runtime generation and dependencies can be tested in the testsuite),
this means you will have to install the library first.
You can list all installed software using the "lipa list" command.
The FooBar demo application can be run by executing
```bash
runapp foobar-1.0:/bin/foo
```
or simply by using the link which should have been installed into the GUI application menu of your desktop
environment.
If you want to remove the software again, just remove the files in /opt/software,
and the file /usr/share/applications/foobar.desktop, in case it was installed.

### Contributing
TODO
Just drop me a note / pull request.
