extproc sh

kscp_ver=$(sed -n 's/#define KSCP_VERSION.*"\(.*\)"/\1/p' kscp.h)
kscp_short_ver=$(echo $kscp_ver|sed 's/[^0-9]//g')

sed -e "s/@SHORT_VER@/$kscp_short_ver/g" -e "s/@VER@/$kscp_ver/g" kscp.txt > kscp$kscp_short_ver.txt
mv kscp.zip kscp$kscp_short_ver.zip
