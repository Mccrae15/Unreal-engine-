#!/bin/bash
cd "$(dirname "$0")"

SOURCE="https://scontent.oculuscdn.com/v/t64.5771-25/10000000_262987555171346_6729143540330336339_n.tar/sho_20210118_121146.tar?_nc_cat=102&ccb=2&_nc_sid=f4d450&_nc_ohc=VbxWwVpqySoAX9zXgIU&_nc_ht=scontent.oculuscdn.com&_nc_rmd=260&oh=5d7041ac54543450effd7aedae74e96e&oe=602D70CE"
ARCHIVE=/tmp/sho_`date +%Y%m%d_%H%M%S`.tar

echo Downloading Content...
curl $SOURCE --output $ARCHIVE

echo Extracting Content...
tar xvf $ARCHIVE

echo Success!