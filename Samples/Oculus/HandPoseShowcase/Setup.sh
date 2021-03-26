#!/bin/bash
cd "$(dirname "$0")"

SOURCE="https://scontent.oculuscdn.com/v/t64.5771-25/10000000_262987555171346_6729143540330336339_n.tar/sho_20210118_121146.tar?_nc_cat=102&_nc_map=control&ccb=1-3&_nc_sid=f4d450&_nc_ohc=OVQnxiuT1fsAX8w-SZw&_nc_ht=scontent.oculuscdn.com&_nc_rmd=260&oh=35591e811daeee48f2efd4bd3d5f52c9&oe=608473CE"
ARCHIVE=/tmp/sho_`date +%Y%m%d_%H%M%S`.tar

echo Downloading Content...
curl $SOURCE --output $ARCHIVE

echo Extracting Content...
tar xvf $ARCHIVE

echo Success!