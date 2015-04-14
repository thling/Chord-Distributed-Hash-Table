## Simplified Chord Distributed Hash Table
#### Tzu Hsuan (Sam) Ling - ling21@purdue.edu

-----------------------------------------------------------------------------------
The package provides a sample application to test the Chord service, the 
<code>SampleApp.cpp</code>. It is a simple two-threaded application that provides a command
line interface (see supported command below). The functions demonstrates
how the service library works. In addition, the library itself is also
documented with great detail.
-----------------------------------------------------------------------------------

###Compile & Execute###

* `make` will compile the file.
* `make call` will make and execute the file using port 30000
* `make s1` will call sample application in a way that it spawns a new Chord ring
* `make s2` will call sample application in a way that it joins the link made by `make s1`
* `make clean` to clean the directory of unnecessary object files and executables
* To execute after compile, use command `./sample -c CHORD_PORT -p APP_PORT [-j IP_ADDRESS_TO_JOIN]`

###Implementation & Design Choices###

* Listed and described in the final report PDF file

###Supported Commands####

These are supported in the sample app to provide best view on the functionalities in the Chord implementation

* `help`
	* Display help text
* `exit`
	* Terminate the connection. Note since voluntary node leaves is not implemented,
	  this command will cause the program to hang
* `time [on|off]`
	* Turn timer on or off. Turning timer on will trigger a stopwatch right after issuing any command,
      and the elapsed time will be displayed at the end of command run
* `hash [TEXT]`
    * Displays the consistent hash of [TEXT]
* `finger`
	* Prints the contents of finger table.
* `map`
	* Prints the map of the current Chord ring.
* `find`
	* Finds a certain key. Expected output will be "Uploading to HOST:PORT," but this is for demonstration
	  only and nothing will be transferred (the sample app does not have file transfer ability)
	
###Source Files###

* `src/Chord.cpp`
	* Client part of the P2P program
* `src/MessageHandler.cpp`
	* Connection manager for the program, both outgoing and incoming connections
	* Server part of the P2P program
* `include/Chord.hpp`
	* Header file for `Chord.cpp`
* `include/ChordError.hpp`
	* Contains ChordError handling procedures
* `include/MessageHandler.hpp`
	* Header file for `MessageHandler.cpp`
* `include/MessageTypes.hpp`
	* Defines all message types and message type identifier
* `include/ServiceNotification.hpp`
	* Provides abstract layer of the notification service
* `include/ThreadFactory.hpp`
	* Provides abstract layer for object-oriented threading
* `include/Utils.hpp`
	* Provides utility functions for general use