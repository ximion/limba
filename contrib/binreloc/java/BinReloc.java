// BinReloc - A class for creating relocatable applications
// Written By: Damjan Jovanovic <gmail username damjan.jov>
// http://www.autopackage.org/
//
// This source code is public domain. You can relicense this code
// under whatever license you want.
//
// See http://autopackage.org/docs/binreloc/ for
// more information and how to use this.

package org.autopackage;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.security.CodeSource;

/**
 * BinReloc is a utility that allows your program to be "location independent",
 * locating its external files using paths relative to the program.
 * @author dacha
 */
public class BinReloc {
    private final Class<?> klazz;

    /**
     * Constructs a new instance that will locate paths using the path of
     * the given class's .class or .jar file as the origin.
     *
     * @param klazz a class in your program that will be used as the origin
     */
    public BinReloc(Class<?> klazz) {
        this.klazz = klazz;
    }

    /**
     * Returns the URL from which the origin class was loaded.
     * @return the URL from which the origin class was loaded, or null on failure.
     * @throws SecurityException
     */
    public URL getCodeSourceLocation() throws SecurityException {
        final CodeSource codeSource = klazz.getProtectionDomain().getCodeSource();
        if (codeSource != null) {
            return codeSource.getLocation();
        }
        return null;
    }

    private static String getPrefixPath(URI uri) {
        final String path = uri.getPath();
        // file.class in /usr/bin will have the URL file:///usr/bin/
        // file.jar in /usr/bin will have the URL   file:///usr/bin/file.jar
        final int lastSlash = path.lastIndexOf('/');
        if (lastSlash < 0) {
            return null;
        }
        final int secondLastSlash = path.lastIndexOf('/', lastSlash - 1);
        if (secondLastSlash < 0) {
            return null;
        }
        return path.substring(0, secondLastSlash + 1);
    }

    private static URL createURLWithPath(URI uri, String path) {
        try {
            return new URI(
                uri.getScheme(), uri.getUserInfo(),
                uri.getHost(), uri.getPort(),
                path, uri.getQuery(), uri.getFragment()).toURL();
        } catch (IllegalArgumentException illegalArgumentException) {
        } catch (MalformedURLException malformedURLException) {
        } catch (URISyntaxException uriSyntaxException) {
        }
        return null;
    }

    private static File createFileWithPath(URI uri, String path) {
        try {
            return new File(new URI(
                uri.getScheme(), uri.getUserInfo(),
                uri.getHost(), uri.getPort(),
                path, uri.getQuery(), uri.getFragment()));
        } catch (IllegalArgumentException illegalArgumentException) {
        } catch (URISyntaxException uriSyntaxException) {
        }
        return null;
    }

    private URL relocURL(String path) throws SecurityException {
        final URL url = getCodeSourceLocation();
        if (url != null) {
            try {
                final URI uri = url.toURI();
                final String prefixPath = getPrefixPath(uri);
                if (prefixPath != null) {
                    return createURLWithPath(uri, prefixPath + path);
                }
            } catch (URISyntaxException uriSyntaxException) {
            }
        }
        return null;
    }

    private File relocFile(String path) throws SecurityException {
        final URL url = getCodeSourceLocation();
        if (url != null) {
            try {
                final URI uri = url.toURI();
                final String prefixPath = getPrefixPath(uri);
                if (prefixPath != null) {
                    return createFileWithPath(uri, prefixPath + path);
                }
            } catch (URISyntaxException uriSyntaxException) {
            }
        }
        return null;
    }

    /**
     * Returns the directory the origin class was loaded from, which is
     * <code>/usr/bin</code> if your class was loaded from <code>/usr/bin/file.jar</code>.
     * @return the startup directory as a URL, or null on failure.
     * @throws SecurityException
     */
    public URL getStartupDirURL() throws SecurityException {
        final URL url = getCodeSourceLocation();
        if (url != null) {
            try {
                final URI uri = url.toURI();
                final String path = uri.getPath();
                // file.class in /usr/bin will have the URL file:///usr/bin/
                // file.jar in /usr/bin will have the URL   file:///usr/bin/file.jar
                final int lastSlash = path.lastIndexOf('/');
                if (lastSlash < 0) {
                    return null;
                }
                return createURLWithPath(uri, path.substring(0, lastSlash + 1));
            } catch (URISyntaxException uriSyntaxException) {
            }
        }
        return null;
    }

    /**
     * Returns the directory the origin class was loaded from, which is
     * <code>/usr/bin</code> if your class was loaded from <code>/usr/bin/file.jar</code>.
     * @return the startup directory as a File, or null on failure.
     * @throws SecurityException
     */
    public File getStartupDirFile() throws SecurityException {
        final URL url = getCodeSourceLocation();
        if (url != null) {
            try {
                final URI uri = url.toURI();
                final String path = uri.getPath();
                // file.class in /usr/bin will have the URL file:///usr/bin/
                // file.jar in /usr/bin will have the URL   file:///usr/bin/file.jar
                final int lastSlash = path.lastIndexOf('/');
                if (lastSlash < 0) {
                    return null;
                }
                return createFileWithPath(uri, path.substring(0, lastSlash + 1));
            } catch (URISyntaxException uriSyntaxException) {
            }
        }
        return null;
    }

    /**
     * Returns the UNIX installation prefix, which is
     * <code>/usr</code> if your class was loaded from <code>/usr/bin/file.jar</code>.
     * @return the installation prefix as a URL, or null on failure.
     * @throws SecurityException
     */
    public URL getPrefixURL() throws SecurityException {
        return relocURL("");
    }

    /**
     * Returns the UNIX installation prefix, which is
     * <code>/usr</code> if your class was loaded from <code>/usr/bin/file.jar</code>.
     * @return the installation prefix as a File, or null on failure.
     * @throws SecurityException
     */
    public File getPrefixFile() throws SecurityException {
        return relocFile("");
    }

    /**
     * Returns the bin subdirectory under the UNIX installation prefix,
     * which is <code>/usr/bin</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the bin directory as a URL, or null on failure.
     * @throws SecurityException
     */
    public URL getBinDirURL() throws SecurityException {
        return relocURL("bin");
    }

    /**
     * Returns the bin subdirectory under the UNIX installation prefix,
     * which is <code>/usr/bin</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the bin directory as a File, or null on failure.
     * @throws SecurityException
     */
    public File getBinDirFile() throws SecurityException {
        return relocFile("bin");
    }

    /**
     * Returns the data subdirectory under the UNIX installation prefix,
     * which is <code>/usr/share</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the data directory as a URL, or null on failure.
     * @throws SecurityException
     */
    public URL getDataDirURL() throws SecurityException {
        return relocURL("share");
    }

    /**
     * Returns the data subdirectory under the UNIX installation prefix,
     * which is <code>/usr/share</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the data directory as a File, or null on failure.
     * @throws SecurityException
     */
    public File getDataDirFile() throws SecurityException {
        return relocFile("share");
    }

    /**
     * Returns the lib subdirectory under the UNIX installation prefix,
     * which is <code>/usr/lib</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the lib directory as a URL, or null on failure.
     * @throws SecurityException
     */
    public URL getLibDirURL() throws SecurityException {
        return relocURL("lib");
    }

    /**
     * Returns the lib subdirectory under the UNIX installation prefix,
     * which is <code>/usr/lib</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the lib directory as a File, or null on failure.
     * @throws SecurityException
     */
    public File getLibDirFile() throws SecurityException {
        return relocFile("lib");
    }

    /**
     * Returns the libexec subdirectory under the UNIX installation prefix,
     * which is <code>/usr/libexec</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the libexec directory as a URL, or null on failure.
     * @throws SecurityException
     */
    public URL getLibExecDirURL() throws SecurityException {
        return relocURL("libexec");
    }

    /**
     * Returns the libexec subdirectory under the UNIX installation prefix,
     * which is <code>/usr/libexec</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the libexec directory as a File, or null on failure.
     * @throws SecurityException
     */
    public File getLibExecDirFile() throws SecurityException {
        return relocFile("libexec");
    }

    /**
     * Returns the locale subdirectory under the UNIX installation prefix,
     * which is <code>/usr/share/locale</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the locale directory as a URL, or null on failure.
     * @throws SecurityException
     */
    public URL getLocaleDirURL() throws SecurityException {
        return relocURL("share/locale");
    }

    /**
     * Returns the locale subdirectory under the UNIX installation prefix,
     * which is <code>/usr/share/locale</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the locale directory as a File, or null on failure.
     * @throws SecurityException
     */
    public File getLocaleDirFile() throws SecurityException {
        return relocFile("share/locale");
    }

    /**
     * Returns the sbin subdirectory under the UNIX installation prefix,
     * which is <code>/usr/sbin</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the sbin directory as a URL, or null on failure.
     * @throws SecurityException
     */
    public URL getSbinDirURL() throws SecurityException {
        return relocURL("sbin");
    }

    /**
     * Returns the sbin subdirectory under the UNIX installation prefix,
     * which is <code>/usr/sbin</code> if your class was loaded from
     * <code>/usr/bin/file.jar</code>.
     * @return the sbin directory as a File, or null on failure.
     * @throws SecurityException
     */
    public File getSbinDirFile() throws SecurityException {
        return relocFile("sbin");
    }
}
