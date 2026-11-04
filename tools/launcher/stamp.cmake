# Writes this build's stamp, <year>.<month><day>.<hours><minutes><seconds>, for the launcher's footer
# and the zip's name, run by the LTCallisto_Stamp target on every build (cmake -P).
#
#   STAMP_CPP  the source the launchers compile
#   STAMP_TXT  the same stamp alone, which package_zip.cmake reads

string(TIMESTAMP stamp "%Y.%m%d.%H%M%S")
file(WRITE "${STAMP_CPP}" "// Written by tools/launcher/stamp.cmake on every build\nconst char *g_szLauncherBuild = \"${stamp}\";\n")
file(WRITE "${STAMP_TXT}" "${stamp}")
