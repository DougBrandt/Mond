
README
------
Mond is a unix/linux system and program monitoring tool.


AUTHORS
-------
* Douglas Brandt
* Kerry S.


NOTES
-----
Completion Status: This program is fully functional.

* We clamp the interval to 500 if it is passed in less.
* We have noticed that if the parent runs before the child process, the
  executable name in the log file is (mond).  This can be avoided, but we felt
  that having it output this information was the most accurate for a logger to
  do.
* The man page was wrong for several fields and thus we went with the layout of
  the proc file in order to output the correct statistics.
* The exampleLog.txt that we submit was generated from:
  ./mond -s ./example 500 exampleLog.txt

Test Cases: All test cases work.

* ./mond -s ./example 500 logFile.txt
* ./mond ./example 500 logFile.txt
* ./mond ./example 100000 logFile.txt

* ./mond -s ls 500 logFile.txt
* ./mond ls 500 logFile.txt
* ./mond ls 100000 logFile.txt

