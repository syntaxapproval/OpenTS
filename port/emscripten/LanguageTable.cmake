# Turns the STRINGTABLE blocks of language.rc into C++ table rows, for builds that have no
# resource compiler and no Language.dll to load the strings from.
#
#   cmake -DOPENTS_LANGUAGE_RC=<language.rc> -DOPENTS_LANGUAGE_TABLE=<output.inc> -P LanguageTable.cmake
#
# Each row is { ID, "text" }, where ID is the symbol language.h defines. Resource-script
# quoting differs from C++ in one way only: a doubled quote stands for a quote.

file(READ "${OPENTS_LANGUAGE_RC}" contents)

# A semicolon or a square bracket in a string would split or glue CMake list items, so all
# three travel under placeholders until the rows are written.
string(REPLACE ";" "@SEMI@" contents "${contents}")
string(REPLACE "[" "@LBRACKET@" contents "${contents}")
string(REPLACE "]" "@RBRACKET@" contents "${contents}")
string(REPLACE "\r" "" contents "${contents}")
string(REPLACE "\n" ";" lines "${contents}")

set(rows "")
set(inside FALSE)
set(pending "")
set(count 0)
foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    if(line MATCHES "^STRINGTABLE")
        set(inside TRUE)
        continue()
    endif()
    if(NOT inside OR line STREQUAL "BEGIN" OR line STREQUAL "")
        continue()
    endif()
    if(line STREQUAL "END")
        set(inside FALSE)
        continue()
    endif()

    set(id "")
    set(text "")
    if(line MATCHES "^([A-Za-z_][A-Za-z0-9_]*)[ \t]+\"(.*)\"$")
        set(id "${CMAKE_MATCH_1}")
        set(text "${CMAKE_MATCH_2}")
    elseif(line MATCHES "^([A-Za-z_][A-Za-z0-9_]*)$")
        # The string is on the next line.
        set(pending "${CMAKE_MATCH_1}")
        continue()
    elseif(pending AND line MATCHES "^\"(.*)\"$")
        set(id "${pending}")
        set(text "${CMAKE_MATCH_1}")
    else()
        message(FATAL_ERROR "LanguageTable: cannot read the line: ${line}")
    endif()
    set(pending "")

    string(REPLACE "\"\"" "\\\"" text "${text}")
    string(APPEND rows "\t{ ${id}, \"${text}\" },\n")
    math(EXPR count "${count} + 1")
endforeach()

string(REPLACE "@SEMI@" ";" rows "${rows}")
string(REPLACE "@LBRACKET@" "[" rows "${rows}")
string(REPLACE "@RBRACKET@" "]" rows "${rows}")

set(output "// Generated from language.rc by port/emscripten/LanguageTable.cmake. Do not edit.\n${rows}")
if(EXISTS "${OPENTS_LANGUAGE_TABLE}")
    file(READ "${OPENTS_LANGUAGE_TABLE}" existing)
    if(existing STREQUAL output)
        return()
    endif()
endif()
file(WRITE "${OPENTS_LANGUAGE_TABLE}" "${output}")
message(STATUS "LanguageTable: ${count} strings")
