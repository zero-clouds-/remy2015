# CPSC 3600 Final Project

-------------------------------------------------------------------------------  
FILE CONTENTS
-------------------------------------------------------------------------------
* GROUP MEMBERS
* PROJECT DESCRIPTION
* OTHER COMMENTS
* KNOWN PROBLEMS
* DESIGN

-------------------------------------------------------------------------------  
I. GROUP MEMBERS
-------------------------------------------------------------------------------
* Group Members: Josh Gates, Grace Glenn, Ben Green, Andrew Zhang
* Project Name: Robot Proxy Client and Server
* Due Date: Friday, December 4th, 2015, 2:30 pm

-------------------------------------------------------------------------------  
II. PROJECT DESCRIPTION
-------------------------------------------------------------------------------
This project is made up of a series of programs that enables the user to 
control a robot remotely. This control is provided by running a robot client
that communicates with a server sending requests and commands to the robot.

The first connection, between the client and server, occurs over a single UDP
connection. The second, between the server and robot, occurs over multiple
TCP connections. The server de/multiplexes the raw information of several
aspects of the robot's information -- such as GPS, dGPS, laser, and image data,
and sends it to the client based on what it has requested.

The client controls the robot explicitly by asking the server to move, turn, or
stop the robot. Commands are ordered to make a the robot draw two shapes: a polygon
of sides N with length L, and a second polygon of sides N - 1 and length L.

* Source Files:

    FILE NAME           FILE DESCRIPTION
    
    client.c            requests information about robot from server
    Makefile            Makefile to compile program
    run_client          bash script to run client with default settings
    run_server          bash script to run server with default settings
    server.c            runs a tcp client and udp server
    udp_protocol.c      provides functionality for parsing and creating udp datagrams
    udp_protocol.c      udp_protocol header, furnishes robot commands and header info
    utility.c           provides functionality for creating and manipulating buffers
    utility.h           utility header

-------------------------------------------------------------------------------  
III. OTHER COMMENTS
-------------------------------------------------------------------------------
* Compilation:

The program should be compiled using the Makefile with the following terminal
command:
            $ make 

To remove all object files:

            $ make clean

* Running Programs:

The following terminal lines should be used to run the programs after compilation:

        Machine 1
            $ ./server -h <hostname-of-robot> -i <robot-id> -n <robot-number> -p <port>
        
            -h <hostname-of-robot>: Address/Hostname of the robot to communicate with
            -i <robot-id>: ID of robot
            -n <robot-number>: Number of robot; used to get pictures
            -p <port>: Port that the server will listen on for connections

        Machine 2
            $ ./client -h <hostname-of-server> -p <port> -n <number> -l <length-of-sides>
    
            -h <hostname-of-server>: Address/Hostname of the proxy server
            -p <port>: Port that the proxy server is listening on
            -n <number-of-sides>: A number between 4 and 8 (inclusively) defining the 
            number of sides
            -l <length-of-sides>: A number that defines the length of the sides 
            

* Default Usage: 

To run the program in default mode:

    Machine 1
            $ bash run_server
    
    Machine 2
            $ bash run_client

-------------------------------------------------------------------------------  
IV. KNOWN PROBLEMS
-------------------------------------------------------------------------------


-------------------------------------------------------------------------------  
V. DESIGN
-------------------------------------------------------------------------------



