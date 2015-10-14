#!/bin/bash
#
# Create Limba release tarball from version control system
#
set -e
OPTION_SPEC="version:,git-tag:,sign"
PARSED_OPTIONS=$(getopt -n "$0" -a -o h --l "$OPTION_SPEC" -- "$@")

eval set -- "$PARSED_OPTIONS"

if [ $? != 0 ] ; then usage ; exit 1 ; fi

while true ; do
	case "$1" in
		--version ) case "$2" in
			    "") echo "version parameter needs an argument!"; exit 3 ;;
			     *) export LIMBA_VERSION=$2 ; shift 2 ;;
			   esac ;;
		--git-tag ) case "$2" in
			    "") echo "git-tag parameter needs an argument!"; exit 3 ;;
			     *) export GIT_TAG=$2 ; shift 2 ;;
			   esac ;;
		--sign )  SIGN_RELEASE=1; shift; ;;
		--) shift ; break ;;
		* ) echo "ERROR: unknown flag $1"; exit 2;;
	esac
done

if [ "$LIMBA_VERSION" = "" ]; then
 echo "No Limba version set!"
 exit 1
fi
if [ "$GIT_TAG" = "" ]; then
 echo "No Git tag set!"
 exit 1
fi

rm -rf ./release-tar-tmp

# check if we can build Limba
make -C build clean all

mkdir -p ./release-tar-tmp
git archive --prefix="Limba-$LIMBA_VERSION/" "$GIT_TAG^{tree}" | tar -x -C ./release-tar-tmp

R_ROOT="./release-tar-tmp/Limba-$LIMBA_VERSION"

# cleanup files which should not go to the release tarball
find ./release-tar-tmp -name .gitignore -type f -delete
find ./release-tar-tmp -name '*~' -type f -delete
find ./release-tar-tmp -name '*.bak' -type f -delete
find ./release-tar-tmp -name '*.o' -type f -delete
rm -rf $R_ROOT/.tx
rm -f $R_ROOT/.travis.yml
rm $R_ROOT/release.sh
rm $R_ROOT/RELEASE

# create release tarball
cd ./release-tar-tmp
tar cvJf "Limba-$LIMBA_VERSION.tar.xz" "./Limba-$LIMBA_VERSION/"
mv "Limba-$LIMBA_VERSION.tar.xz" ../
cd ..

# cleanup
rm -r ./release-tar-tmp

# sign release, if flag is set
if [ "$SIGN_RELEASE" = "1" ]; then
 gpg --armor --sign --detach-sig "Limba-$LIMBA_VERSION.tar.xz"
fi
