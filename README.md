Limba
======

Limba provides developers with a way to easily create software bundles for their applications which
run on multiple Linux distributions. It provides atomic upgrades of software, simultaneous installation
of multiple software versions and a simple way to obtain and upgrade software.

It is based on ideas of Glick2 and Listaller, and uses modern Linux kernel features
to allow applications to share libraries and other components, reducing the amount
of duplicate software components running on a Linux system.

![Limba logo](data/limba-small.png "Logo")

# Releases
You can find official release tarballs here: [people.freedesktop.org/~mak/limba/releases](http://people.freedesktop.org/~mak/limba/releases/)

At the current state of development, it makes more sense to clone the Git repository instead, to
get the latest changes.

# Getting started with Limba
## Running Limba applications
In order to run Limba applications, you will need a Linux kernel supporting OverlayFS[1] and
multiple OverlayFS layers.
This is at least Linux 4.0.

Applications installed with Limba will nicely integrate with your desktop-environment, and should not
look different from native applications.

[1]: http://lwn.net/Articles/618140/

## Building Limba packages
We have very brief instructions on how to create new Limba packages
in the [documentation](http://people.freedesktop.org/~mak/limba/docs/create-package/).
A complex example showing how to automatically create Limba packages can be found
at [Limba-Neverball](https://github.com/ximion/limba-neverball/).

## Developing Limba
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
If you want to remove the software again, just use the ```lipa remove``` command.

A more complex example can be found at the [Limba-Neverball](https://github.com/ximion/limba-neverball/) repository.
It shows how to build a Limba package, and how to distribute it via a software source.
Just take a look at the instructions there.

### Contributing

#### Translations
You can help localizing Limba! Take a look at the [Transifex Project](https://www.transifex.com/projects/p/limba/).
New languages will be approved as soon as possible.

#### Code, Bugs, Documentation, etc.
Just create a pull request or submit a patch via the issue tracker or email.
The software is subjected to the LGPLv2+ and GPLv2+ licenses.
