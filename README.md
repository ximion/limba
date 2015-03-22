Limba
==============

Limba is a new project which allows 3rd-party software installations on Linux.
It is based on ideas of Glick2 and Listaller, and uses modern Linux kernel features
to allow applications to share libraries and other components, reducing the amount
of duplicate software components running on a Linux system.

![Limba logo](data/limba-small.png "Logo")

# Releases
You can find official release tarballs here: [people.freedesktop.org/~mak/limba/releases](http://people.freedesktop.org/~mak/limba/releases/)

At the current state of development, it makes more sense to clone the Git repository instead, to
get the latest changes.

# Users
## Running Limba applications
In order to run Limba applications, you will need a Linux kernel supporting OverlayFS.
This is at least 3.18[1], but many distributions already ship their kernels with an OverlayFS
kernel module.

Applications installed with limber will nicely integrate with your desktop-environment, and should not
look different from native applications.

[1]: http://lwn.net/Articles/618140/

## Building Limba packages
We have very brief instructions on how to create new Limba packages
in the [documentation](http://people.freedesktop.org/~mak/limba/docs/create-package/).

## Developers
### Dependencies
 * glib2 (>= 2.36)
 * GObject-Introspection
 * AppStream (libappstream)
 * PolicyKit
 * libarchive
 * GPGMe
 * libuuid
 * libcurl
 * systemd (optional)

### Compilation instructions
Ensure all build dependencies are installed.
Then run the following commands to compile (and install) Limba:

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

### Translating Limba
You can help localizing Limba! Take a look at the [Transifex Project](https://www.transifex.com/projects/p/limba/).
New languages will be approved as soon as possible.

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
Just create a pull request or submit a patch via the issue tracker or email.
The software is subjected to the LGPLv2+ and GPLv2+ licenses.
