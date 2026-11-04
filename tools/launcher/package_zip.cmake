# Packs LTCallisto's build into ZIP_DIR/LTCallisto-<ARCH>_v<year>.<month><day>.<hours><minutes><seconds>.zip,
# run by the LTCallisto_Zip target on every build (cmake -P).
#
# Only build products and the default configs go in. OUT_DIR is also where the games are played from,
# so its game folders hold the player's archives, saves, settings and screenshots.
#
#   OUT_DIR       the build's output folder, holding LTCallisto.exe and Games
#   ZIP_DIR       where the zip goes
#   ARCH          win64 or win32
#   TOP_FILES     the engine and launcher files, separated by |
#   CONFIG_FILES  <source>*<folder in the zip> pairs, separated by |, copied when that game is in the zip
#   EMPTY_DIRS    folders made when their game is in the zip, separated by |
#   STAMP_FILE    the build stamp stamp.cmake wrote, which the launcher's footer shows too

if(STAMP_FILE AND EXISTS "${STAMP_FILE}")
    file(READ "${STAMP_FILE}" stamp)
    string(STRIP "${stamp}" stamp)
else()
    string(TIMESTAMP stamp "%Y.%m%d.%H%M%S")
endif()
set(name "LTCallisto-${ARCH}_v${stamp}")
set(work "${ZIP_DIR}/_zip")
set(stage "${work}/${name}")

file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${stage}")

string(REPLACE "|" ";" top "${TOP_FILES}")
foreach(f IN LISTS top)
    if(EXISTS "${f}")
        file(COPY "${f}" DESTINATION "${stage}")
    endif()
endforeach()

# Each game's modules, in the folders the launcher looks in (NOLF2's ClientFX is in its Game folder)
file(GLOB modules RELATIVE "${OUT_DIR}"
    "${OUT_DIR}/Games/*/*.dll" "${OUT_DIR}/Games/*/*.lto" "${OUT_DIR}/Games/*/*/*.fxd")
foreach(m IN LISTS modules)
    get_filename_component(dir "${m}" DIRECTORY)
    file(COPY "${OUT_DIR}/${m}" DESTINATION "${stage}/${dir}")
endforeach()

string(REPLACE "|" ";" configs "${CONFIG_FILES}")
foreach(pair IN LISTS configs)
    string(REPLACE "*" ";" parts "${pair}")
    list(GET parts 0 src)
    list(GET parts 1 dest)
    if(EXISTS "${stage}/${dest}" AND EXISTS "${src}")
        file(COPY "${src}" DESTINATION "${stage}/${dest}")
    endif()
endforeach()

string(REPLACE "|" ";" dirs "${EMPTY_DIRS}")
foreach(d IN LISTS dirs)
    get_filename_component(game "${d}" DIRECTORY)
    if(EXISTS "${stage}/${game}")
        file(MAKE_DIRECTORY "${stage}/${d}")
    endif()
endforeach()

execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf "${ZIP_DIR}/${name}.zip" --format=zip "${name}"
                WORKING_DIRECTORY "${work}" RESULT_VARIABLE result)
file(REMOVE_RECURSE "${work}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not write ${ZIP_DIR}/${name}.zip")
endif()
message(STATUS "Packed ${ZIP_DIR}/${name}.zip")
