Create a Limba package
**********************
.. highlight:: bash

This brief instructions assume that you have Limba already installed on your system.

Prepare your application
========================

The cool thing about Limba is that you don’t really have to do many changes on your application.
There are a few things to pay attention to, though:

* Ensure the binaries and data are installed into the right places in the directory hierarchy.
  Binaries must go to $prefix/bin, for example.
* Ensure that configuration can be found under /etc as well as under $prefix/etc

This needs to be done so your application will find its data at runtime.
Additionally, you need to write an AppStream metadata file, and find out which stuff your application depends on.


Create package metadata & install software
==========================================

Basics
------

Now you can create the metadata necessary to build a Limba package. Just run::

  cd /path/to/my/project
  limba-build make-template

This will create a “lipkg” directory, containing a “control” file and a “metainfo.xml” file, which can be a symlink to the
AppStream metadata, or be new metadata.

Now, configure your application with ``/app`` as install prefix.

.. hint::

   For CMake, use ``-DCMAKE_INSTALL_PREFIX=/app``, an Automake-based build-system
   needs ``–prefix=/app`` passed to configure/autogen.sh.

Then install the software to the ``lipkg/target`` directory.

Handling dependencies
---------------------

If your software has dependencies on other packages, just get the Limba packages for these dependencies,
or build new ones. Then place the resulting IPK packages in the lipkg/repo directory.
Ideally, you should be able to fetch Limba packages which contain the software components directly from their upstream developers.

Then, open the ``lipkg/control`` file and adjust the ``Requires`` line.
The names of the components you depend on match their AppStream-IDs (``<id/>`` tag in the AppStream XML document).
Any version-relation (>=, >>, <<, <=, <>) is supported, and specified in brackets after the component-id.

The resulting control-file might look like this:

.. code-block:: control

  Format-Version: 1.0

  Requires: Qt5Core (>= 5.4), Qt5DBus (== 5.4), libpng12

If the specified dependencies are in the ``lipkg/repo/`` subdirectory, these packages will get installed automatically, if your application package is installed.
Otherwise, Limba depends on the user to install these packages manually – there is no interaction with the distribution’s package-manager.


Building the package
====================

In order to build your package, make sure the content in ``target/`` is up to date, then run::

  lipkgen build lipkg/

This will build your package and output it in the ``lipkg/`` directory.

.. hint::

   For files installed into ``target/``, Limba will automatically split them into a SDK and a runtime package.
   The SDK package is what you install in case you want to build against the software, the runtime package is
   what user want to install to run it.
   In case you want to influence which content goes into which package, install the runtime data into a ``target.rt/``
   directory, and the SDK components into ``target.sdk/``. Limba will then do the right thing.


Testing the package
===================

You can now test your package, Just run::

  sudo limba install bundle.ipk

Your software should install successfully.
If you provided a .desktop file in ``share/applications``, you should find your application in your desktop’s application-menu.
Otherwise, you can run a binary from the command-line, just append the version of your package to the binary name (bash-comletion helps).
Alternatively, you can use the ``runapp`` command, which lets you run any binary in your bundle/package, which is quite helpful for debugging
(since the environment a Limba-installed application is run is different from the one of other applications).

Example::

  runapp ${component_id}/${version}:/bin/binary-name
