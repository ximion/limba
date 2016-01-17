Limba
======

Limba provides developers with a way to easily create software bundles for their applications which
run on multiple Linux distributions. It provides atomic upgrades of software, simultaneous installation
of multiple software versions and a simple way to obtain and upgrade software.

It is based on ideas of Glick2 and Listaller, and uses modern Linux kernel features
to allow applications to share libraries and other components, reducing the amount
of duplicate software components running on a Linux system while still getting all the
advantages obtained by bundling software components.

![Limba logo](data/limba-small.png "Logo")

# Releases

You can find official, signed release tarballs here: [people.freedesktop.org/~mak/limba/releases](http://people.freedesktop.org/~mak/limba/releases/)

If you want to test the latest changes, you should consider cloning the Git repository instead.

# Getting started with Limba

## Running Limba applications
In order to build & run Limba applications, you will need a Linux kernel supporting OverlayFS[1] and
multiple OverlayFS layers.
This is at least Linux 4.0.

Applications installed with Limba will nicely integrate with your desktop-environment, and should not
look different from native applications.

[1]: http://lwn.net/Articles/618140/

## Building Limba packages
We have brief instructions on how to create new Limba packages in the
[documentation](http://people.freedesktop.org/~mak/limba/docs/create-package.html).

## Examples
There are some [examples on Limba bundle creation available](https://github.com/limba-project?utf8=%E2%9C%93&query=example).
They are intended to show how bundling works, and are usually not complete.
A complex example showing how to automatically create Limba packages is
[Limba-Neverball](https://github.com/limba-project/example-neverball/).

## Documentation & other resources
You can find documentation about Limba [here](http://people.freedesktop.org/~mak/limba/docs/).
The documentation is a work in progress, please send pull-requests or bug reports in case you encounter
missing bits or errors.
Limba is used by a repository service called LimbaHub, which is able to manage large Limba repositories
(unlike the minimal repository manager included in `limba-build`). You might want to check out it's
[source code](https://github.com/limba-project/limba-hub) and help developing the repository service.

# Developing Limba

## Dependencies
 * glib2 (>= 2.46)
 * GObject-Introspection
 * AppStream [2]
 * PolicyKit
 * libarchive [3]
 * GPGMe [4]
 * libuuid
 * libcurl
 * systemd (optional)

[2]: https://github.com/ximion/appstream
[3]: http://www.libarchive.org/
[4]: https://www.gnupg.org/related_software/gpgme/

## Build instructions
[![Build Status](https://semaphoreci.com/api/v1/projects/723d8655-b508-4fbf-ba10-517db063b9a4/592515/badge.svg)](https://semaphoreci.com/ximion/limba)

Ensure all build dependencies are installed.
Then run the following commands to compile (and install) Limba:

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

## Testing Limba
Run `make test` first, This will produce two packages (file extension .ipk) in the tests/data directory.
You can install these packages using
```bash
sudo limba install-local file.ipk
```
One package depends on the other (so runtime generation and dependencies can be tested in the testsuite),
this means you will have to install the library first.
You can list all installed software using the `limba list` command.
The FooBar demo application can be run by executing
```bash
runapp foobar/1.0:/bin/foo
```
or simply by using the link which should have been installed into the GUI application menu of your desktop
environment.
If you want to remove the software again, just use the `limba remove` command.

A more complex example can be found at the [Limba-Neverball](https://github.com/limba-project/limba-neverball/) repository.
It shows how to build a Limba package, and how to distribute it via a software source.
Just take a look at the instructions there.


# Contributing

## Translations
You can help localizing Limba! Take a look at the [Transifex Project](https://www.transifex.com/projects/p/limba/).
New languages will be approved as soon as possible.

## Code, Bugs, Documentation, etc.
Just create a pull request or submit a patch via the issue tracker or email.
The software is subjected to the LGPLv2+ and GPLv2+ licenses.
