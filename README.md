arduino-xpl
===========

Arduino xPL library, originally from http://xpl-arduino.googlecode.com/svn/trunk

Note that you should use at least Arduino IDE 1.0.4, as the malloc/realloc bug is fixed on this release. This library leaks memory with older IDE's because of realloc (see http://arduino.cc/en/Main/ReleaseNotes)