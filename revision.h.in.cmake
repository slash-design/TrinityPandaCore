#ifndef __REVISION_H__
#define __REVISION_H__
 #define _HASH                      "@rev_hash@"
 #define _DATE                      "@rev_date@"
 #define _BRANCH                    "@rev_branch@"
 #define _CMAKE_COMMAND             R"(@CMAKE_COMMAND@)"
 #define _CMAKE_VERSION             R"(@CMAKE_VERSION@)"
 #define _CMAKE_HOST_SYSTEM         R"(@CMAKE_HOST_SYSTEM_NAME@ @CMAKE_HOST_SYSTEM_VERSION@)"
 #define _SOURCE_DIRECTORY          R"(@CMAKE_SOURCE_DIR@)"
 #define _MYSQL_EXECUTABLE          R"(@MYSQL_EXECUTABLE@)"
 #define _FULL_DATABASE             "DBFULLUPDATE15-12-2021"
 #define _BUILD_DIRECTORY           R"(@BUILDDIR@)"
 #define VER_COMPANYNAME_STR        "DestinyCore Developers"
 #define VER_LEGALCOPYRIGHT_STR     "(c)2025-@rev_year@ DestinyCore"
 #define VER_FILEVERSION            0,0,0
 #define VER_FILEVERSION_STR        "@rev_hash@ @rev_date@ (@rev_branch@ branch)"
 #define VER_PRODUCTVERSION         VER_FILEVERSION
 #define VER_PRODUCTVERSION_STR     VER_FILEVERSION_STR
#endif // __REVISION_H__
