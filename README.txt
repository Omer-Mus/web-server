This file should contain:

  - Omer Mustel
  - UNI: om2349
  - lab assignment number 7
 
part1:
  - Part1 is completed. The web page working as expected, all directories 
    are chomded with 711 and files with 644, and the file
    names/directory hierarchy are ordered and named corrrectly.

part2a:
  - The program serving the static content without problems or valgrind
    errors. It interacts with the user and the server side as Jae's 
    program. 
  - credit to a helpful video: https://www.youtube.com/watch?v=veFwXrEHPsk
  - Stackoverflow: https://stackoverflow.com/questions/4989431
  - SysTutorial: https://www.systutorials.com/how-to-test-whether-a-given-path-is-a-directory-or-a-file-in-c/
part2b:
  - The program creates a connection with the mdb server, send to and read 
    from it, and then display the results on the web page.
    no input in the key means a new line and therefore prints all of the
    results. Both /mdb-lookup & /mdb-lookup are valid URIs.
    There is vlagrind "Still reachable" results; however they also occure
    in jae's valgrind output.
    This part has no interference with part1.
