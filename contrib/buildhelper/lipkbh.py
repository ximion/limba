#!/usr/bin/python3
#
# Copyright (C) 2014 Matthias Klumpp <mak@debian.org>
#
# Licensed under the GNU General Public License Version 2
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os
import os.path
import sys
import yaml
import time
import subprocess
from optparse import OptionParser
from tempfile import NamedTemporaryFile

class LipkgBuilder:

    def __init__(self, chroot, use_chroot=True):
        self.chroot = chroot
        self._use_chroot = use_chroot

    def _find_build_paths(self, path):
        buildconf = os.path.join(path, "lipkg", "build.yml")
        if os.path.isfile(buildconf):
            self.build_conf_file = buildconf
            self.root_path = path
            return True
        buildconf = os.path.join(path, "build.yml")
        if os.path.isfile(buildconf):
            self.build_conf_file = buildconf
            self.root_path = path
            return True
        buildconf = os.path.join(path, ".travis.yml")
        if os.path.isfile(buildconf):
            self.build_conf_file = buildconf
            self.root_path = path
            return True
        return False

    def initialize(self, path):
        # try to find our paths
        ret = self._find_build_paths(path)
        if not ret:
            raise Exception("Could not find a 'build.yml' file!")
            return False

        if self._use_chroot and not self.chroot:
            print("You need to specify a chroot!")
            sys.exit(1)

        self.wdir = path

        f = open(self.build_conf_file, 'r')
        self.recipe = yaml.safe_load(f)
        f.close()

    def exec_cmd(self, cmd):
        if self._use_chroot:
            bcommand = ['schroot', '-c', self.chroot, '--', 'bash', cmd]
        else:
            bcommand = ['bash', cmd]
        proc = subprocess.Popen(bcommand, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    universal_newlines=True)
        while True:
            line = proc.stdout.readline()
            if not line:
                break
            print(line.rstrip())
        proc.poll()

        return proc.returncode

    def echo_command(self, cmd):
        s = cmd.replace("\"", "\\\"")
        return "echo \" ! %s\"" % (s)

    def add_buildscript_commands(self, buildscript, commands, section_name, section_id=None):
        if not commands:
            return

        seclen = len(section_name)
        buildscript.append("echo \" \"")
        buildscript.append("echo \"┌%s%s┐\"" % ("─" * seclen, "─" * 14))
        buildscript.append("echo \"│ %s%s │\"" % (section_name, " " * 12))
        buildscript.append("echo \"└%s%s┘\"" % ("─" * seclen, "─" * 14))
        buildscript.append("echo \" \"")
        if section_id:
            buildscript.append("echo \" ! [%s]\"" % (section_id))
        for cmd in commands:
            buildscript.append(self.echo_command(cmd))
            buildscript.append(cmd)

    def run(self):
        buildscript = list()

        if not self.wdir:
            print("Could not determine a working directory - is the builder initialized?")
            sys.exit(1)

        buildscript.append("#!/bin/bash")
        buildscript.append("set -e")
        buildscript.append("")
        buildscript.append("cd %s" % (self.wdir))
        buildscript.append("BUILDROOT=\"%s\"" % (self.wdir))
        buildscript.append("")

        commands = self.recipe.get('before_script')
        self.add_buildscript_commands(buildscript, commands, "Preparing Build Environment", "before_script")

        commands = self.recipe.get('script')
        if not commands:
            return 0
        self.add_buildscript_commands(buildscript, commands, "Build", "script")

        commands = self.recipe.get('after_script')
        self.add_buildscript_commands(buildscript, commands, "Cleanup", "after_script")

        f = NamedTemporaryFile(mode='w', suffix="_lbs.sh")
        for line in buildscript:
            f.write("%s\n" % line)
        f.flush()

        # NOTE: We are running a script in /tmp as root in a chroot - this is not really
        # a safe thing todo, although - since it is always chrooted - the security issue should
        # be minimal. Still, a better solution is on TODO
        ret = self.exec_cmd(f.name)
        if ret != 0:
            return ret


def main():
    parser = OptionParser()
    parser.add_option("-c", "--chroot",
                  type="string", dest="chroot", default=None,
                  help="use selected chroot")
    parser.add_option("--no-chroot",
                  action="store_true", dest="no_chroot",
                  help="explicitly do not use a chroot")

    (options, args) = parser.parse_args()

    builder = LipkgBuilder(options.chroot, not options.no_chroot)

    if len(args) == 0:
        print("You need to specify a command!")
        sys.exit(1)

    command = args[0]

    if command == "build" or command == "b":
        if len(args) > 1:
            path = args[1]
        else:
            path = os.getcwd()

        try:
            builder.initialize(path)
        except Exception as e:
            print("Error: %s" % (str(e)))
            sys.exit(2)

        res = builder.run()
        sys.exit(res)
    else:
        print("Sorry, I don't know the command '%s'." % (command))
        sys.exit(1)


if __name__ == "__main__":
    os.environ['LANG'] = 'C'
    os.environ['LC_ALL'] = 'C'
    main()
