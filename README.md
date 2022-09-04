# txt2obj
generate an object file from a text file as an extern c-string for linking in c projects

It is to be used in ESP32-IDF projects. I am studying http-server that frequencyly embedding html-file-string into c source file. It is too tedious. Is it possible to compile an html file to object file?

It is also hard to learn ELF (the format of object file) and writing a compiler is beyond my ability.

My plan is first build a object file from a single extern c-string and then replace it by a complete content of an html file.
